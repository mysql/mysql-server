/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file sql/sql_rename.cc
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/

#include "sql/sql_rename.h"

#include <string.h>
#include <set>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/log_shared.h"
#include "mysqld_error.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_table.h"                 // dd::table_storage_engine
#include "sql/dd/types/abstract_table.h"     // dd::Abstract_table
#include "sql/dd/types/table.h"              // dd::Table
#include "sql/dd_sql_view.h"                 // View_metadata_updater
#include "sql/handler.h"
#include "sql/log.h"          // query_logger
#include "sql/mysqld.h"       // lower_case_table_names
#include "sql/sp_cache.h"     // sp_cache_invalidate
#include "sql/sql_base.h"     // tdc_remove_table,
                              // lock_table_names,
#include "sql/sql_class.h"    // THD
#include "sql/sql_handler.h"  // mysql_ha_rm_tables
#include "sql/sql_table.h"    // write_bin_log,
                              // build_table_filename
#include "sql/sql_trigger.h"  // change_trigger_table_name
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"  // trans_commit_stmt

namespace dd {
class Schema;
}  // namespace dd

typedef std::set<handlerton *> post_ddl_htons_t;

static TABLE_LIST *rename_tables(
    THD *thd, TABLE_LIST *table_list, bool skip_error, bool *int_commit_done,
    post_ddl_htons_t *post_ddl_htons,
    Foreign_key_parents_invalidator *fk_invalidator);

static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list);

/**
  Rename tables from the list.

  @param thd          Thread context.
  @param table_list   Every two entries in the table_list form
                      a pair of original name and the new name.

  @return True - on failure, false - on success.
*/

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list) {
  TABLE_LIST *ren_table = 0;
  DBUG_ENTER("mysql_rename_tables");

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */

  if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction()) {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(1);
  }

  mysql_ha_rm_tables(thd, table_list);

  /*
    The below Auto_releaser allows to keep uncommitted versions of data-
    dictionary objects cached in the Dictionary_client for the whole duration
    of the statement.
  */
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (query_logger.is_log_table_enabled(QUERY_LOG_GENERAL) ||
      query_logger.is_log_table_enabled(QUERY_LOG_SLOW)) {
    int to_table;
    const char *rename_log_table[2] = {NULL, NULL};

    /*
      Rules for rename of a log table:

      IF   1. Log tables are enabled
      AND  2. Rename operates on the log table and nothing is being
              renamed to the log table.
      DO   3. Throw an error message.
      ELSE 4. Perform rename.
    */

    for (to_table = 0, ren_table = table_list; ren_table;
         to_table = 1 - to_table, ren_table = ren_table->next_local) {
      int log_table_rename = 0;

      if ((log_table_rename =
               query_logger.check_if_log_table(ren_table, true))) {
        /*
          as we use log_table_rename as an array index, we need it to start
          with 0, while QUERY_LOG_SLOW == 1 and QUERY_LOG_GENERAL == 2.
          So, we shift the value to start with 0;
        */
        log_table_rename--;
        if (rename_log_table[log_table_rename]) {
          if (to_table)
            rename_log_table[log_table_rename] = NULL;
          else {
            /*
              Two renames of "log_table TO" w/o rename "TO log_table" in
              between.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            DBUG_RETURN(true);
          }
        } else {
          if (to_table) {
            /*
              Attempt to rename a table TO log_table w/o renaming
              log_table TO some table.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            DBUG_RETURN(true);
          } else {
            /* save the name of the log table to report an error */
            rename_log_table[log_table_rename] = ren_table->table_name;
          }
        }
      }
    }
    if (rename_log_table[0] || rename_log_table[1]) {
      if (rename_log_table[0])
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[0],
                 rename_log_table[0]);
      else
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[1],
                 rename_log_table[1]);
      DBUG_RETURN(true);
    }
  }

  if (lock_table_names(thd, table_list, 0, thd->variables.lock_wait_timeout,
                       0) ||
      lock_trigger_names(thd, table_list))
    DBUG_RETURN(true);

  const dd::Table *table_def = nullptr;
  TABLE_LIST *table;
  for (table = table_list; table && table->next_local;
       table = table->next_local) {
    if (thd->dd_client()->acquire(table->db, table->table_name, &table_def)) {
      return true;
    }
    if (table_def && table_def->hidden() == dd::Abstract_table::HT_HIDDEN_SE) {
      my_error(ER_NO_SUCH_TABLE, MYF(0), table->db, table->table_name);
      DBUG_RETURN(true);
    }
  }

  for (ren_table = table_list; ren_table; ren_table = ren_table->next_local)
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, ren_table->db,
                     ren_table->table_name, false);
  bool error = false;
  bool int_commit_done = false;
  std::set<handlerton *> post_ddl_htons;
  Foreign_key_parents_invalidator fk_invalidator;
  /*
    An exclusive lock on table names is satisfactory to ensure
    no other thread accesses this table.
  */
  if ((ren_table = rename_tables(thd, table_list, 0, &int_commit_done,
                                 &post_ddl_htons, &fk_invalidator))) {
    /* Rename didn't succeed;  rename back the tables in reverse order */
    TABLE_LIST *table;

    if (int_commit_done) {
      /* Reverse the table list */
      table_list = reverse_table_list(table_list);

      /* Find the last renamed table */
      for (table = table_list; table->next_local != ren_table;
           table = table->next_local->next_local)
        ;
      table = table->next_local->next_local;  // Skip error table
      /* Revert to old names */
      rename_tables(thd, table, 1, &int_commit_done, &post_ddl_htons,
                    &fk_invalidator);

      /* Revert the table list (for prepared statements) */
      table_list = reverse_table_list(table_list);
    }

    error = true;
  }

  if (!error) {
    error = write_bin_log(thd, true, thd->query().str, thd->query().length,
                          !int_commit_done);
  }

  if (!error) {
    Uncommitted_tables_guard uncommitted_tables(thd);

    for (ren_table = table_list; ren_table;
         ren_table = ren_table->next_local->next_local) {
      TABLE_LIST *new_table = ren_table->next_local;
      DBUG_ASSERT(new_table);

      uncommitted_tables.add_table(ren_table);
      uncommitted_tables.add_table(new_table);

      if ((error = update_referencing_views_metadata(
               thd, ren_table, new_table->db, new_table->table_name,
               int_commit_done, &uncommitted_tables)))
        break;
    }
  }

  if (!error && !int_commit_done) {
    error = (trans_commit_stmt(thd) || trans_commit_implicit(thd));

    if (!error) {
      /*
        Don't try to invalidate foreign key parents on error,
        as we might miss necessary locks on them.
      */
      fk_invalidator.invalidate(thd);
    }
  }

  if (error) {
    trans_rollback_stmt(thd);
    /*
      Full rollback in case we have THD::transaction_rollback_request
      and to synchronize DD state in cache and on disk (as statement
      rollback doesn't clear DD cache of modified uncommitted objects).
    */
    trans_rollback(thd);
  }

  for (handlerton *hton : post_ddl_htons) hton->post_ddl(thd);

  if (!error) my_ok(thd);

  DBUG_RETURN(error);
}

/*
  reverse table list

  SYNOPSIS
    reverse_table_list()
    table_list pointer to table _list

  RETURN
    pointer to new (reversed) list
*/
static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list) {
  TABLE_LIST *prev = 0;

  while (table_list) {
    TABLE_LIST *next = table_list->next_local;
    table_list->next_local = prev;
    prev = table_list;
    table_list = next;
  }
  return (prev);
}

/**
  Rename a single table or a view.

  @param[in]      thd               Thread handle.
  @param[in]      ren_table         A table/view to be renamed.
  @param[in]      new_db            The database to which the
                                    table to be moved to.
  @param[in]      new_table_name    The new table/view name.
  @param[in]      new_table_alias   The new table/view alias.
  @param[in]      skip_error        Whether to skip errors.
  @param[in,out]  int_commit_done   Whether intermediate commits
                                    were done.
  @param[in,out]  post_ddl_htons    Set of SEs supporting atomic DDL
                                    for which post-DDL hooks needs
                                    to be called.
  @param[in,out]  fk_invalidator    Object keeping track of which
                                    dd::Table objects to invalidate.

  @note Unless int_commit_done is true failure of this call requires
        rollback of transaction before doing anything else.

  @return False on success, True if rename failed.
*/

static bool do_rename(THD *thd, TABLE_LIST *ren_table, const char *new_db,
                      const char *new_table_name, const char *new_table_alias,
                      bool skip_error, bool *int_commit_done,
                      std::set<handlerton *> *post_ddl_htons,
                      Foreign_key_parents_invalidator *fk_invalidator) {
  const char *new_alias = new_table_name;
  const char *old_alias = ren_table->table_name;

  DBUG_ENTER("do_rename");

  if (lower_case_table_names == 2) {
    old_alias = ren_table->alias;
    new_alias = new_table_alias;
  }
  DBUG_ASSERT(new_alias);

  // Fail if the target table already exists
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *from_schema = nullptr;
  const dd::Schema *to_schema = nullptr;
  dd::Abstract_table *from_at = nullptr;
  const dd::Abstract_table *to_table = nullptr;
  if (thd->dd_client()->acquire(ren_table->db, &from_schema) ||
      thd->dd_client()->acquire(new_db, &to_schema) ||
      thd->dd_client()->acquire(new_db, new_alias, &to_table) ||
      thd->dd_client()->acquire_for_modification(
          ren_table->db, ren_table->table_name, &from_at))
    DBUG_RETURN(true);  // This error cannot be skipped

  if (to_table != nullptr) {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
    DBUG_RETURN(true);  // This error cannot be skipped
  }

  if (from_schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), ren_table->db);
    DBUG_RETURN(!skip_error);
  }

  if (to_schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), new_db);
    DBUG_RETURN(!skip_error);
  }

  if (from_at == nullptr) {
    my_error(ER_NO_SUCH_TABLE, MYF(0), ren_table->db, old_alias);
    DBUG_RETURN(!skip_error);
  }

  // So here we know the source table exists and the target table does
  // not exist. Next is to act based on the table type.
  switch (from_at->type()) {
    case dd::enum_table_type::BASE_TABLE: {
      handlerton *hton = NULL;
      dd::Table *from_table = dynamic_cast<dd::Table *>(from_at);
      // If the engine is not found, my_error() has already been called
      if (dd::table_storage_engine(thd, from_table, &hton))
        DBUG_RETURN(!skip_error);

      if ((hton->flags & HTON_SUPPORTS_ATOMIC_DDL) && (hton->post_ddl))
        post_ddl_htons->insert(hton);

      if (check_table_triggers_are_not_in_the_same_schema(ren_table->db,
                                                          *from_table, new_db))
        DBUG_RETURN(!skip_error);

      // The below code assumes that only SE capable of atomic DDL support FK.
      DBUG_ASSERT(!(hton->flags & HTON_SUPPORTS_FOREIGN_KEYS) ||
                  (hton->flags & HTON_SUPPORTS_ATOMIC_DDL));

      /*
        If we are performing rename with intermediate commits then
        invalidation of foreign key parents should have happened
        already, right after commit. Code below dealing with failure to
        acquire locks on parent and child tables relies on this
        invariant.
      */
      DBUG_ASSERT(!(*int_commit_done) || fk_invalidator->is_empty());

      /*
        Obtain exclusive metadata lock on all tables being referenced by the
        old table, since these tables must be invalidated to force a cache miss
        on next acquisition, in order to refresh their FK information.

        Also lock all tables referencing the old table. The FK information in
        these tables must be updated to refer to the new table name.

        And also lock all tables referencing the new table. The FK information
        in these tables must be updated to refer to the (possibly) new
        unique index name.

        TODO: Long-term we should consider acquiring these locks in
              mysql_rename_tables() together with locks on other tables.
              This should decrease probability of deadlock and improve
              crash-safety for RENAME TABLES which mix InnoDB and non-InnoDB
              tables (as all waiting will happen before any changes to SEs).
      */
      if (hton->flags & HTON_SUPPORTS_FOREIGN_KEYS) {
        /*
          RENAME TABLES is prohibited under LOCK TABLES. So we don't need to
          handle LOCK TABLES case here by disallowing situations in which
          table being renamed will become parent for some orphan child tables.
        */
        DBUG_ASSERT(thd->locked_tables_mode != LTM_LOCK_TABLES);

        if (collect_and_lock_fk_tables_for_rename_table(
                thd, ren_table->db, old_alias, from_table, new_db, new_alias,
                hton, fk_invalidator)) {
          /*
            If we are performing RENAME TABLES with intermediate commits
            FK invalidator was empty before the above call. So at this
            point it only contains entries on which we might miss locks.
            We need to clear invalidator before starting process of
            reverse renames.
            If we are performing RENAME TABLES without intermediate commits
            the whole statement will be rolled back and invalidation won't
            happen. So it is safe to clear invalidator.
          */
          fk_invalidator->clear();
          DBUG_RETURN(!skip_error);
        }
      }

      /*
        We commit changes to data-dictionary immediately after renaming
        table in storage engine if SE doesn't support atomic DDL or
        there were intermediate commits already. In the latter case
        the whole statement is not crash-safe anyway and clean-up is
        simpler this way.

        The FKs of the renamed table must be changed to reflect the new table.
        The tables referencing the old and new table names must have their FK
        information updated to reflec the correct table- and unique index name.
        The parents of the old FKs must be invalidated to make sure they
        update the cached FK parent information upon next acquisition.

        If renaming fails, my_error() has already been called

        QQ: Think about (!skip_error)
      */
      if (mysql_rename_table(
              thd, hton, ren_table->db, old_alias, ren_table->db, old_alias,
              *to_schema, new_db, new_alias,
              ((hton->flags & HTON_SUPPORTS_ATOMIC_DDL) ? NO_DD_COMMIT : 0)) ||
          ((hton->flags & HTON_SUPPORTS_FOREIGN_KEYS) &&
           adjust_fks_for_rename_table(thd, ren_table->db, old_alias, new_db,
                                       new_alias, hton))) {
        /*
          If RENAME TABLE is non-atomic as whole but we didn't try to commit
          the above changes we need to clean-up them before returning.
        */
        if (*int_commit_done && (hton->flags & HTON_SUPPORTS_ATOMIC_DDL)) {
          Disable_gtid_state_update_guard disabler(thd);
          trans_rollback_stmt(thd);
          // Full rollback in case we have THD::transaction_rollback_request.
          trans_rollback(thd);
          /*
            Preserve the invariant that FK invalidator is empty after each
            step of non-atomic RENAME TABLE.
          */
          fk_invalidator->clear();
        }
        DBUG_RETURN(!skip_error);
      }

      /*
        If RENAME TABLE is non-atomic but we have not committed the above
        rename and changes to FK we need to do it now.
      */
      if (*int_commit_done && (hton->flags & HTON_SUPPORTS_ATOMIC_DDL)) {
        Disable_gtid_state_update_guard disabler(thd);

        if (trans_commit_stmt(thd) || trans_commit(thd)) {
          /*
            Preserve the invariant that FK invalidator is empty after each
            step of non-atomic RENAME TABLE.
          */
          fk_invalidator->clear();
          DBUG_RETURN(!skip_error);
        }
      }

      *int_commit_done |= !(hton->flags & HTON_SUPPORTS_ATOMIC_DDL);

      if (*int_commit_done) {
        /*
          For non-atomic RENAME TABLE we try to invalidate FK parents right
          after transaction commit. This enforces invariant that invalidator
          is empty after each step of such RENAME TABLE.

          We perform invalidation if there was commit above to handle two
          cases:
          - We committed rename of table in SE supporting atomic DDL (and so
            possibly supporting FKs) since this RENAME TABLE already started
            doing intermediate commits.
          - We committed rename of table in SE not supporting atomic DDL.
            Invalidation still necessary as this might be first non-atomic
            rename which follows chain of atomic renames which might have
            added pending invalidation requests to invalidator.
        */
        fk_invalidator->invalidate(thd);
      }

      break;
    }
    case dd::enum_table_type::SYSTEM_VIEW:  // Fall through
    case dd::enum_table_type::USER_VIEW: {
      // Changing the schema of a view is not allowed.
      if (strcmp(ren_table->db, new_db)) {
        my_error(ER_FORBID_SCHEMA_CHANGE, MYF(0), ren_table->db, new_db);
        DBUG_RETURN(!skip_error);
      }

      /* Rename view in the data-dictionary. */
      Disable_gtid_state_update_guard disabler(thd);

      // Set schema id and view name.
      from_at->set_name(new_alias);

      // Do the update. Errors will be reported by the dictionary subsystem.
      if (thd->dd_client()->update(from_at)) {
        if (*int_commit_done) {
          trans_rollback_stmt(thd);
          // Full rollback in case we have THD::transaction_rollback_request.
          trans_rollback(thd);
        }
        DBUG_RETURN(!skip_error);
      }

      if (*int_commit_done) {
        if (trans_commit_stmt(thd) || trans_commit(thd))
          DBUG_RETURN(!skip_error);
      }

      sp_cache_invalidate();
      break;
    }
    default:
      DBUG_ASSERT(false); /* purecov: deadcode */
  }

  // Now, we know that rename succeeded, and can log the schema access
  thd->add_to_binlog_accessed_dbs(ren_table->db);
  thd->add_to_binlog_accessed_dbs(new_db);

  DBUG_RETURN(false);
}
/*
  Rename all tables in list;
  Return pointer to wrong entry if something goes
  wrong.  Note that the table_list may be empty!
*/

/**
  Rename all tables/views in the list.

  @param[in]      thd               Thread handle.
  @param[in]      table_list        List of tables to rename.
  @param[in]      skip_error        Whether to skip errors.
  @param[in,out]  int_commit_done   Whether intermediate commits
                                    were done.
  @param[in,out]  post_ddl_htons    Set of SEs supporting atomic DDL
                                    for which post-DDL hooks needs
                                    to be called.
  @param[in,out]  fk_invalidator    Object keeping track of which
                                    dd::Table objects to invalidate.

  @note
    Take a table/view name from and odd list element and rename it to a
    the name taken from list element+1. Note that the table_list may be
    empty.

  @note Unless int_commit_done is true failure of this call requires
        rollback of transaction before doing anything else.

  @return 0 - on success, pointer to problematic entry if something
          goes wrong.
*/

static TABLE_LIST *rename_tables(
    THD *thd, TABLE_LIST *table_list, bool skip_error, bool *int_commit_done,
    post_ddl_htons_t *post_ddl_htons,
    Foreign_key_parents_invalidator *fk_invalidator)

{
  TABLE_LIST *ren_table, *new_table;

  DBUG_ENTER("rename_tables");

  for (ren_table = table_list; ren_table; ren_table = new_table->next_local) {
    new_table = ren_table->next_local;
    if (do_rename(thd, ren_table, new_table->db, new_table->table_name,
                  new_table->alias, skip_error, int_commit_done, post_ddl_htons,
                  fk_invalidator))
      DBUG_RETURN(ren_table);
  }
  DBUG_RETURN(0);
}

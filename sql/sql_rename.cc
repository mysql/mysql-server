/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file sql/sql_rename.cc
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/

#include "sql/sql_rename.h"

#include <string.h>

#include "dd/cache/dictionary_client.h"// dd::cache::Dictionary_client
#include "dd/dd_table.h"      // dd::table_storage_engine
#include "dd/types/abstract_table.h" // dd::Abstract_table
#include "dd/types/table.h"   // dd::Table
#include "dd_sql_view.h"      // View_metadata_updater
#include "lex_string.h"
#include "log.h"              // query_logger
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld.h"           // lower_case_table_names
#include "mysqld_error.h"
#include "sp_cache.h"         // sp_cache_invalidate
#include "sql_base.h"         // tdc_remove_table,
                              // lock_table_names,
#include "sql_cache.h"        // query_cache
#include "sql_class.h"        // THD
#include "sql_handler.h"      // mysql_ha_rm_tables
#include "sql_plugin.h"
#include "sql_table.h"        // write_bin_log,
                              // build_table_filename
#include "sql_trigger.h"      // change_trigger_table_name
#include "sql_view.h"         // mysql_rename_view
#include "system_variables.h"
#include "table.h"
#include "transaction.h"      // trans_commit_stmt


typedef std::set<handlerton*> post_ddl_htons_t;

static TABLE_LIST *rename_tables(THD *thd, TABLE_LIST *table_list,
                                 bool skip_error, bool *int_commit_done,
                                 post_ddl_htons_t *post_ddl_htons);

static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list);

/**
  Rename tables from the list.

  @param thd          Thread context.
  @param table_list   Every two entries in the table_list form
                      a pair of original name and the new name.

  @return True - on failure, false - on success.
*/

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list)
{
  TABLE_LIST *ren_table= 0;
  DBUG_ENTER("mysql_rename_tables");

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */

  if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction())
  {
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
      query_logger.is_log_table_enabled(QUERY_LOG_SLOW))
  {
    int to_table;
    const char *rename_log_table[2]= {NULL, NULL};

    /*
      Rules for rename of a log table:

      IF   1. Log tables are enabled
      AND  2. Rename operates on the log table and nothing is being
              renamed to the log table.
      DO   3. Throw an error message.
      ELSE 4. Perform rename.
    */

    for (to_table= 0, ren_table= table_list; ren_table;
         to_table= 1 - to_table, ren_table= ren_table->next_local)
    {
      int log_table_rename= 0;

      if ((log_table_rename= query_logger.check_if_log_table(ren_table, true)))
      {
        /*
          as we use log_table_rename as an array index, we need it to start
          with 0, while QUERY_LOG_SLOW == 1 and QUERY_LOG_GENERAL == 2.
          So, we shift the value to start with 0;
        */
        log_table_rename--;
        if (rename_log_table[log_table_rename])
        {
          if (to_table)
            rename_log_table[log_table_rename]= NULL;
          else
          {
            /*
              Two renames of "log_table TO" w/o rename "TO log_table" in
              between.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            DBUG_RETURN(true);
          }
        }
        else
        {
          if (to_table)
          {
            /*
              Attempt to rename a table TO log_table w/o renaming
              log_table TO some table.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            DBUG_RETURN(true);
          }
          else
          {
            /* save the name of the log table to report an error */
            rename_log_table[log_table_rename]= ren_table->table_name;
          }
        }
      }
    }
    if (rename_log_table[0] || rename_log_table[1])
    {
      if (rename_log_table[0])
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[0],
                 rename_log_table[0]);
      else
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[1],
                 rename_log_table[1]);
      DBUG_RETURN(true);
    }
  }

  if (lock_table_names(thd, table_list, 0, thd->variables.lock_wait_timeout, 0)
      || lock_trigger_names(thd, table_list))
    DBUG_RETURN(true);

  for (ren_table= table_list; ren_table; ren_table= ren_table->next_local)
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, ren_table->db,
                     ren_table->table_name, FALSE);

  bool error= false;
  bool int_commit_done= false;
  std::set<handlerton*> post_ddl_htons;
  /*
    An exclusive lock on table names is satisfactory to ensure
    no other thread accesses this table.
  */
  if ((ren_table= rename_tables(thd, table_list, 0, &int_commit_done,
                                &post_ddl_htons)))
  {
    /* Rename didn't succeed;  rename back the tables in reverse order */
    TABLE_LIST *table;

#ifdef WORKAROUND_TO_BE_REMOVED_BY_WL7016_AND_WL7896
    if (int_commit_done)
    {
#else
    if (!thd->transaction_rollback_request)
    {
      Disable_gtid_state_update_guard disabler(thd);
      trans_commit_stmt(thd);
      trans_commit(thd);
      int_commit_done= true;
#endif
      /* Reverse the table list */
      table_list= reverse_table_list(table_list);

      /* Find the last renamed table */
      for (table= table_list;
	   table->next_local != ren_table ;
	   table= table->next_local->next_local) ;
      table= table->next_local->next_local;		// Skip error table
      /* Revert to old names */
      rename_tables(thd, table, 1, &int_commit_done, &post_ddl_htons);

      /* Revert the table list (for prepared statements) */
      table_list= reverse_table_list(table_list);
    }

    error= true;
  }

  if (!error)
    query_cache.invalidate(thd, table_list, FALSE);

  if (!error)
  {
    error= write_bin_log(thd, true,
                         thd->query().str, thd->query().length,
                         !int_commit_done);
  }

  if (!error
#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL9536
      && int_commit_done
#endif
      )
  {
    Uncommitted_tables_guard uncommitted_tables(thd);

    for (ren_table= table_list; ren_table;
         ren_table= ren_table->next_local->next_local)
    {
      TABLE_LIST *new_table= ren_table->next_local;
      DBUG_ASSERT(new_table);

      uncommitted_tables.add_table(ren_table);
      uncommitted_tables.add_table(new_table);

      if ((error= update_referencing_views_metadata(thd, ren_table,
                                                    new_table->db,
                                                    new_table->table_name,
                                                    int_commit_done,
                                                    &uncommitted_tables)))
        break;
    }
  }

  if (!error && !int_commit_done)
    error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));

#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL9536
  if (!error && !int_commit_done)
  {
    for (ren_table= table_list; ren_table;
         ren_table= ren_table->next_local->next_local)
    {
      TABLE_LIST *new_table= ren_table->next_local;
      DBUG_ASSERT(new_table);

      if ((error= update_referencing_views_metadata(thd, ren_table,
                                                    new_table->db,
                                                    new_table->table_name,
                                                    true,
                                                    nullptr)))
        break;
    }
  }
#endif

  if (error)
  {
    trans_rollback_stmt(thd);
    /*
      Full rollback in case we have THD::transaction_rollback_request
      and to synchronize DD state in cache and on disk (as statement
      rollback doesn't clear DD cache of modified uncommitted objects).
    */
    trans_rollback(thd);
  }

  for (handlerton *hton : post_ddl_htons)
    hton->post_ddl(thd);

  if (!error)
    my_ok(thd);

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
static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list)
{
  TABLE_LIST *prev= 0;

  while (table_list)
  {
    TABLE_LIST *next= table_list->next_local;
    table_list->next_local= prev;
    prev= table_list;
    table_list= next;
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

  @note Unless int_commit_done is true failure of this call requires
        rollback of transaction before doing anything else.
        @sa dd::rename_table().

  @return False on success, True if rename failed.
*/

static bool
do_rename(THD *thd, TABLE_LIST *ren_table,
          const char *new_db, const char *new_table_name,
          const char *new_table_alias, bool skip_error,
          bool *int_commit_done, std::set<handlerton*> *post_ddl_htons)
{
  const char *new_alias= new_table_name;
  const char *old_alias= ren_table->table_name;

  DBUG_ENTER("do_rename");

  if (lower_case_table_names == 2)
  {
    old_alias= ren_table->alias;
    new_alias= new_table_alias;
  }
  DBUG_ASSERT(new_alias);

  // Fail if the target table already exists
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Abstract_table *target_table= nullptr;
  if (thd->dd_client()->acquire(new_db, new_alias, &target_table))
    DBUG_RETURN(true);                         // This error cannot be skipped

  if (target_table != nullptr)
  {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
    DBUG_RETURN(true);                         // This error cannot be skipped
  }

  dd::Abstract_table *abstract_table= nullptr;
  const dd::Schema *from_schema= nullptr;

  if (thd->dd_client()->acquire(ren_table->db, &from_schema) ||
      thd->dd_client()->acquire_for_modification(ren_table->db,
                                                 ren_table->table_name,
                                                 &abstract_table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (from_schema == nullptr)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), ren_table->db);
    DBUG_RETURN(!skip_error);
  }

  if (abstract_table == nullptr)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), ren_table->db, old_alias);
    DBUG_RETURN(!skip_error);
  }

  // So here we know the source table exists and the target table does
  // not exist. Next is to act based on the table type.
  switch (abstract_table->type())
  {
  case dd::enum_table_type::BASE_TABLE:
    {
      handlerton *hton= NULL;
      dd::Table *from_table= dynamic_cast<dd::Table*>(abstract_table);
      // If the engine is not found, my_error() has already been called
      if (dd::table_storage_engine(thd, from_table, &hton))
        DBUG_RETURN(!skip_error);

      /*
        Commit changes to data-dictionary immediately after renaming
        table in storage negine if SE doesn't support atomic DDL or
        there were intermediate commits already. In the latter case
        the whole statement is not crash-safe anyway and clean-up is
        simpler this way.
      */
      *int_commit_done |= !(hton->flags & HTON_SUPPORTS_ATOMIC_DDL);

      if ((hton->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
          (hton->post_ddl))
        post_ddl_htons->insert(hton);

      if (check_table_triggers_are_not_in_the_same_schema(ren_table->db,
                                                          *from_table,
                                                          new_db))
        DBUG_RETURN(!skip_error);

      // If renaming fails, my_error() has already been called
      if (mysql_rename_table(thd, hton, ren_table->db, old_alias, new_db,
                             new_alias, (*int_commit_done ? 0 : NO_DD_COMMIT)))
        DBUG_RETURN(!skip_error);

      break;
    }
  case dd::enum_table_type::SYSTEM_VIEW: // Fall through
  case dd::enum_table_type::USER_VIEW:
    {
      // Changing the schema of a view is not allowed.
      if (strcmp(ren_table->db, new_db))
      {
        my_error(ER_FORBID_SCHEMA_CHANGE, MYF(0), ren_table->db, new_db);
        DBUG_RETURN(!skip_error);
      }

      /* Rename view in the data-dictionary. */
      Disable_gtid_state_update_guard disabler(thd);

      // Set schema id and view name.
      abstract_table->set_name(new_alias);

      // Do the update. Errors will be reported by the dictionary subsystem.
      if (thd->dd_client()->update(abstract_table))
      {
        if (*int_commit_done)
        {
          trans_rollback_stmt(thd);
          // Full rollback in case we have THD::transaction_rollback_request.
          trans_rollback(thd);
          DBUG_RETURN(!skip_error);
        }
      }

      if (*int_commit_done)
      {
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

  @note
    Take a table/view name from and odd list element and rename it to a
    the name taken from list element+1. Note that the table_list may be
    empty.

  @note Unless int_commit_done is true failure of this call requires
        rollback of transaction before doing anything else.
        @sa dd::rename_table().

  @return 0 - on success, pointer to problematic entry if something
          goes wrong.
*/

static TABLE_LIST *
rename_tables(THD *thd, TABLE_LIST *table_list, bool skip_error,
              bool *int_commit_done, post_ddl_htons_t *post_ddl_htons)
{
  TABLE_LIST *ren_table, *new_table;

  DBUG_ENTER("rename_tables");

  for (ren_table= table_list; ren_table; ren_table= new_table->next_local)
  {
    new_table= ren_table->next_local;
    if (do_rename(thd, ren_table, new_table->db, new_table->table_name,
                  new_table->alias, skip_error, int_commit_done,
                  post_ddl_htons))
      DBUG_RETURN(ren_table);
  }
  DBUG_RETURN(0);
}

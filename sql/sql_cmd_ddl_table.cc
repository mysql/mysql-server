/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/sql_cmd_ddl_table.h"

#include <string.h>
#include <sys/types.h>

#include "auth/auth_common.h"   // create_table_precheck()
#include "binlog.h"             // mysql_bin_log
#include "dd/cache/dictionary_client.h"
#include "derror.h"             // ER_THD
#include "error_handler.h"      // Ignore_error_handler
#include "handler.h"
#include "item.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "partition_info.h"     // check_partition_tablespace_names()
#include "query_options.h"
#include "query_result.h"
#include "session_tracker.h"
#include "sql_alter.h"
#include "sql_base.h"           // open_tables_for_query()
#include "sql_class.h"
#include "sql_data_change.h"
#include "sql_error.h"
#include "sql_insert.h"         // Query_result_create
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_parse.h"          // prepare_index_and_data_dir_path()
#include "sql_select.h"         // handle_query()
#include "sql_table.h"          // mysql_create_like_table()
#include "sql_tablespace.h"     // validate_tablespace_name()
#include "system_variables.h"
#include "table.h"
#include "thr_lock.h"


bool Sql_cmd_create_table::execute(THD *thd)
{
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= lex->unit;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  bool link_to_local;
  TABLE_LIST *create_table= first_table;
  TABLE_LIST *select_tables= lex->create_last_non_select_table->next_global;

  /*
    Code below (especially in mysql_create_table() and Query_result_create
    methods) may modify HA_CREATE_INFO structure in LEX, so we have to
    use a copy of this structure to make execution prepared statement-
    safe. A shallow copy is enough as this code won't modify any memory
    referenced from this structure.
  */
  HA_CREATE_INFO create_info(*lex->create_info);
  /*
    We need to copy alter_info for the same reasons of re-execution
    safety, only in case of Alter_info we have to do (almost) a deep
    copy.
  */
  Alter_info alter_info(lex->alter_info, thd->mem_root);

  if (thd->is_error())
  {
    /* If out of memory when creating a copy of alter_info. */
    return true;
  }

  if (((lex->create_info->used_fields & HA_CREATE_USED_DATADIR) != 0 ||
       (lex->create_info->used_fields & HA_CREATE_USED_INDEXDIR) != 0) &&
      check_access(thd, FILE_ACL, any_db, NULL, NULL, FALSE, FALSE))
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "FILE");
    return true;
  }

  if (create_table_precheck(thd, select_tables, create_table))
    return true;

  /* Might have been updated in create_table_precheck */
  create_info.alias= create_table->alias;

  /*
    If no engine type was given, work out the default now
    rather than at parse-time.
  */
  if (!(create_info.used_fields & HA_CREATE_USED_ENGINE))
    create_info.db_type= create_info.options & HA_LEX_CREATE_TMP_TABLE ?
            ha_default_temp_handlerton(thd) : ha_default_handlerton(thd);

  /*
    Assign target tablespace name to enable locking in lock_table_names().
    Reject invalid names.
  */
  if (create_info.tablespace)
  {
    if (validate_tablespace_name_length(create_info.tablespace) ||
        validate_tablespace_name(false, create_info.tablespace,
                                 create_info.db_type))
      return true;

    if (!thd->make_lex_string(&create_table->target_tablespace_name,
                              create_info.tablespace,
                              strlen(create_info.tablespace), false))
      return true;
  }

  // Reject invalid tablespace names specified for partitions.
  if (validate_partition_tablespace_name_lengths(thd->lex->part_info) ||
      validate_partition_tablespace_names(thd->lex->part_info,
                                          create_info.db_type))
    return true;

  /* Fix names if symlinked or relocated tables */
  if (prepare_index_and_data_dir_path(thd, &create_info.data_file_name,
                                      &create_info.index_file_name,
                                      create_table->table_name))
    return true;

  /*
    If we are using SET CHARSET without DEFAULT, add an implicit
    DEFAULT to not confuse old users. (This may change).
  */
  if ((create_info.used_fields &
       (HA_CREATE_USED_DEFAULT_CHARSET | HA_CREATE_USED_CHARSET)) ==
      HA_CREATE_USED_CHARSET)
  {
    create_info.used_fields&= ~HA_CREATE_USED_CHARSET;
    create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
    create_info.default_table_charset= create_info.table_charset;
    create_info.table_charset= 0;
  }

  {
    partition_info *part_info= thd->lex->part_info;
    if (part_info != NULL && has_external_data_or_index_dir(*part_info) &&
        check_access(thd, FILE_ACL, any_db, NULL, NULL, FALSE, FALSE))
    {
      return true;
    }
    if (part_info && !(part_info= thd->lex->part_info->get_clone(true)))
      return true;
    thd->work_part_info= part_info;
  }

  bool res= false;

  if (select_lex->item_list.elements)		// With select
  {
    Query_result *result;

    /*
      CREATE TABLE...IGNORE/REPLACE SELECT... can be unsafe, unless
      ORDER BY PRIMARY KEY clause is used in SELECT statement. We therefore
      use row based logging if mixed or row based logging is available.
      TODO: Check if the order of the output of the select statement is
      deterministic. Waiting for BUG#42415
    */
    if (lex->is_ignore())
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_IGNORE_SELECT);
    
    if(lex->duplicates == DUP_REPLACE)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_REPLACE_SELECT);

    /*
      If:
      a) we inside an SP and there was NAME_CONST substitution,
      b) binlogging is on (STMT mode),
      c) we log the SP as separate statements
      raise a warning, as it may cause problems
      (see 'NAME_CONST issues' in 'Binary Logging of Stored Programs')
     */
    if (thd->query_name_consts && 
        mysql_bin_log.is_open() &&
        thd->variables.binlog_format == BINLOG_FORMAT_STMT &&
        !mysql_bin_log.is_query_in_union(thd, thd->query_id))
    {
      List_iterator_fast<Item> it(select_lex->item_list);
      Item *item;
      uint splocal_refs= 0;
      /* Count SP local vars in the top-level SELECT list */
      while ((item= it++))
      {
        if (item->is_splocal())
          splocal_refs++;
      }
      /*
        If it differs from number of NAME_CONST substitution applied,
        we may have a SOME_FUNC(NAME_CONST()) in the SELECT list,
        that may cause a problem with binary log (see BUG#35383),
        raise a warning. 
      */
      if (splocal_refs != thd->query_name_consts)
        push_warning(thd, 
                     Sql_condition::SL_WARNING,
                     ER_UNKNOWN_ERROR,
"Invoked routine ran a statement that may cause problems with "
"binary log, see 'NAME_CONST issues' in 'Binary Logging of Stored Programs' "
"section of the manual.");
    }
    
    if (unit->set_limit(thd, select_lex))
      return true;

    /*
      Disable non-empty MERGE tables with CREATE...SELECT. Too
      complicated. See Bug #26379. Empty MERGE tables are read-only
      and don't allow CREATE...SELECT anyway.
    */
    if (create_info.used_fields & HA_CREATE_USED_UNION)
    {
      my_error(ER_WRONG_OBJECT, MYF(0), create_table->db,
               create_table->table_name, "BASE TABLE");
      return true;
    }

    if (open_tables_for_query(thd, all_tables, false))
      return true;

    /* The table already exists */
    if (create_table->table || create_table->is_view())
    {
      if (create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS)
      {
        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_TABLE_EXISTS_ERROR,
                            ER_THD(thd, ER_TABLE_EXISTS_ERROR),
                            create_info.alias);
        my_ok(thd);
        return false;
      }
      else
      {
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_info.alias);
        return false;
      }
    }

    /*
      Remove target table from main select and name resolution
      context. This can't be done earlier as it will break view merging in
      statements like "CREATE TABLE IF NOT EXISTS existing_view SELECT".
    */
    lex->unlink_first_table(&link_to_local);

    /* Updating any other table is prohibited in CTS statement */
    for (TABLE_LIST *table= lex->query_tables; table;
         table= table->next_global)
    {
      if (table->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE)
      {
        lex->link_first_table_back(create_table, link_to_local);

        my_error(ER_CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, MYF(0),
                 table->table_name, create_info.alias);
        return true;
      }
    }

    /*
      Query_result_create is currently not re-execution friendly and
      needs to be created for every execution of a PS/SP.
    */
    if ((result= new (thd->mem_root) Query_result_create(thd,
                                                         create_table,
                                                         &create_info,
                                                         &alter_info,
                                                         select_lex->item_list,
                                                         lex->duplicates,
                                                         select_tables)))
    {
      // For objects acquired during table creation.
      dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

      Ignore_error_handler ignore_handler;
      Strict_error_handler strict_handler;
      if (thd->lex->is_ignore())
        thd->push_internal_handler(&ignore_handler);
      else if (thd->is_strict_mode())
        thd->push_internal_handler(&strict_handler);

      /*
        CREATE from SELECT give its SELECT_LEX for SELECT,
        and item_list belong to SELECT
      */
      res= handle_query(thd, lex, result, SELECT_NO_UNLOCK, 0);

      if (thd->lex->is_ignore() || thd->is_strict_mode())
        thd->pop_internal_handler();

      delete result;
    }

    lex->link_first_table_back(create_table, link_to_local);
  }
  else
  {
    Strict_error_handler strict_handler;
    /* Push Strict_error_handler */
    if (!thd->lex->is_ignore() && thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);
    /* regular create */
    if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
    {
      /* CREATE TABLE ... LIKE ... */
      res= mysql_create_like_table(thd, create_table, select_tables,
                                   &create_info);
    }
    else
    {
      /* Regular CREATE TABLE */
      res= mysql_create_table(thd, create_table, &create_info, &alter_info);
    }
    /* Pop Strict_error_handler */
    if (!thd->lex->is_ignore() && thd->is_strict_mode())
      thd->pop_internal_handler();
    if (!res)
    {
      /* in case of create temp tables if @@session_track_state_change is
         ON then send session state notification in OK packet */
      if(create_info.options & HA_LEX_CREATE_TMP_TABLE &&
         thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
      my_ok(thd);
    }
  }
  return res;
}

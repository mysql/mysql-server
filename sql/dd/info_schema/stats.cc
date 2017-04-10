/* Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/info_schema/stats.h"             // dd::info_schema::*

#include <string.h>
#include <cmath>
#include <memory>                             // unique_ptr

#include "auth_common.h"
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/dd.h"                            // dd::create_object
#include "dd/types/abstract_table.h"
#include "dd/types/event.h"
#include "dd/types/index_stat.h"              // dd::Index_stat
#include "dd/types/table_stat.h"              // dd::Table_stat
#include "debug_sync.h"                       // DEBUG_SYNC
#include "error_handler.h"                    // Internal_error_handler
#include "field.h"
#include "key.h"
#include "m_ctype.h"
#include "mdl.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_time.h"                          // TIME_to_ulonglong_datetime
#include "mysqld_error.h"
#include "session_tracker.h"
#include "sql_base.h"                         // open_tables_for_query
#include "sql_class.h"                        // THD
#include "sql_const.h"
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_security_ctx.h"
#include "sql_show.h"                         // make_table_list
#include "system_variables.h"
#include "table.h"                            // TABLE_LIST
#include "tztime.h"                           // Time_zone

namespace dd {
  namespace info_schema {

bool update_table_stats(THD *thd, TABLE_LIST *table)
{
  // Update the object properties
  HA_CREATE_INFO create_info;
  memset(&create_info, 0, sizeof(create_info));

  TABLE *analyze_table= table->table;
  handler *file= analyze_table->file;
  if (analyze_table->file->info(HA_STATUS_VARIABLE |
                                HA_STATUS_TIME |
                                HA_STATUS_VARIABLE_EXTRA |
                                HA_STATUS_AUTO) != 0)
    return true;

  file->update_create_info(&create_info);

  // Create a object to be stored.
  std::unique_ptr<Table_stat> ts_obj(create_object<Table_stat>());

  ts_obj->set_schema_name(String_type(table->db, strlen(table->db)));
  ts_obj->set_table_name(String_type(table->alias, strlen(table->alias)));
  ts_obj->set_table_rows(file->stats.records);
  ts_obj->set_avg_row_length(file->stats.mean_rec_length);
  ts_obj->set_data_length(file->stats.data_file_length);
  ts_obj->set_max_data_length(file->stats.max_data_file_length);
  ts_obj->set_index_length(file->stats.index_file_length);
  ts_obj->set_data_free(file->stats.delete_length);

  if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
    ts_obj->set_checksum(file->checksum());

  MYSQL_TIME time;

  if (file->stats.update_time)
  {
    thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                (my_time_t) file->stats.update_time);
    ulonglong ull_time= TIME_to_ulonglong_datetime(&time);
    ts_obj->set_update_time(ull_time);
  }

  if (file->stats.check_time)
  {
    thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                (my_time_t) file->stats.check_time);
    ulonglong ull_time= TIME_to_ulonglong_datetime(&time);
    ts_obj->set_check_time(ull_time);
  }

  if (analyze_table->found_next_number_field)
    ts_obj->set_auto_increment(file->stats.auto_increment_value);
  else
    ts_obj->set_auto_increment(-1);

  // Store the object
  if (thd->dd_client()->store(ts_obj.get()))
  {
    my_error(ER_UNABLE_TO_STORE_STATISTICS, MYF(0), "table");
    return true;
  }

  return false;
}


bool update_index_stats(THD *thd, TABLE_LIST *table)
{
  // Update the object properties
  TABLE *analyze_table= table->table;
  KEY *key_info=analyze_table->s->key_info;
  if (analyze_table->file->info(HA_STATUS_VARIABLE |
                                HA_STATUS_TIME |
                                HA_STATUS_VARIABLE_EXTRA |
                                HA_STATUS_AUTO) != 0)
    return true;

  // Create a object to be stored.
  std::unique_ptr<Index_stat> obj(create_object<Index_stat>());

  for (uint i=0; i < analyze_table->s->keys; i++, key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    const char *str;
    ha_rows records;
    for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
    {
      str=(key_part->field ? key_part->field->field_name :
             "?unknown field?");

      KEY *key=analyze_table->key_info+i;
      if (key->has_records_per_key(j))
      {
        double recs=(analyze_table->file->stats.records / key->records_per_key(j));
        records= static_cast<longlong>(round(recs));
      }
      else
        records= -1; // Treated as NULL

      obj->set_schema_name(String_type(table->db, strlen(table->db)));
      obj->set_table_name(String_type(table->alias, strlen(table->alias)));
      obj->set_index_name(String_type(key_info->name, strlen(key_info->name)));
      obj->set_column_name(String_type(str, strlen(str)));
      obj->set_cardinality((ulonglong) records);

      // Store the object
      if (thd->dd_client()->store(obj.get()))
      {
        my_error(ER_UNABLE_TO_STORE_STATISTICS, MYF(0), "index");
        return true;
      }

    } // Key part info

  } // Keys

  return false;
}


// Convert IS db to lowercase and table case upper case.
bool convert_table_name_case(char *db, char *table_name)
{
  if (db && is_infoschema_db(db))
  {
    my_casedn_str(system_charset_info, db);
    if (table_name && strncmp(table_name, "ndb", 3))
        my_caseup_str(system_charset_info, table_name);

    return true;
  }

  return false;
}


/**
  Error handler class to convert ER_LOCK_DEADLOCK error to
  ER_WARN_I_S_SKIPPED_TABLE error.

  Handler is pushed for opening a table or acquiring a MDL lock on
  tables for INFORMATION_SCHEMA views(system views) operations.
*/
class MDL_deadlock_error_handler : public Internal_error_handler
{
public:
  MDL_deadlock_error_handler(THD *thd, const String *schema_name,
                             const String *table_name)
    : m_can_deadlock(thd->mdl_context.has_locks()),
      m_schema_name(schema_name),
      m_table_name(table_name)
  {}

  virtual bool handle_condition(THD*,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level*,
                                const char*)
  {
    if (sql_errno == ER_LOCK_DEADLOCK && m_can_deadlock)
    {
      // Convert error to ER_WARN_I_S_SKIPPED_TABLE.
      my_error(ER_WARN_I_S_SKIPPED_TABLE, MYF(0),
               m_schema_name->ptr(), m_table_name->ptr());

      m_error_handled= true;
    }

    return false;
  }

  bool is_error_handled() const { return m_error_handled; }

private:
  bool m_can_deadlock;

  // Schema name
  const String *m_schema_name;

  // Table name
  const String *m_table_name;

  // Flag to indicate whether deadlock error is handled by the handler or not.
  bool m_error_handled= false;
};


// Returns the required statistics from the cache.
ulonglong Statistics_cache::get_stat(ha_statistics &stat,
                                     enum_statistics_type stype)
{
  switch (stype)
  {
  case enum_statistics_type::TABLE_ROWS:
    return(stat.records);

  case enum_statistics_type::TABLE_AVG_ROW_LENGTH:
    return(stat.mean_rec_length);

  case enum_statistics_type::DATA_LENGTH:
    return(stat.data_file_length);

  case enum_statistics_type::MAX_DATA_LENGTH:
    return(stat.max_data_file_length);

  case enum_statistics_type::INDEX_LENGTH:
    return(stat.index_file_length);

  case enum_statistics_type::DATA_FREE:
    return(stat.delete_length);

  case enum_statistics_type::AUTO_INCREMENT:
    return(stat.auto_increment_value);

  case enum_statistics_type::CHECKSUM:
    return(m_checksum);

  case enum_statistics_type::TABLE_UPDATE_TIME:
    return(stat.update_time);

  case enum_statistics_type::CHECK_TIME:
    return(stat.check_time);

  default:
    DBUG_ASSERT(!"Should not hit here");
  }

  return 0;
}


// Read dynamic table statistics from SE by opening the user table
// provided OR by reading cached statistics from SELECT_LEX.
ulonglong Statistics_cache::read_stat(
            THD *thd,
            const String &schema_name_ptr,
            const String &table_name_ptr,
            const String &index_name_ptr,
            uint index_ordinal_position,
            uint column_ordinal_position,
            const String &engine_name_ptr,
            Object_id se_private_id,
            enum_statistics_type stype)
{
  DBUG_ENTER("Statistics_cache::read_stat");
  ulonglong result;

  // NOTE: read_stat() may generate many "useless" warnings, which will be
  // ignored afterwards. On the other hand, there might be "useful"
  // warnings, which should be presented to the user. Diagnostics_area usually
  // stores no more than THD::variables.max_error_count warnings.
  // The problem is that "useless warnings" may occupy all the slots in the
  // Diagnostics_area, so "useful warnings" get rejected. In order to avoid
  // that problem we create a Diagnostics_area instance, which is capable of
  // storing "unlimited" number of warnings.
  Diagnostics_area *da= thd->get_stmt_da();
  Diagnostics_area tmp_da(true);

  // Don't copy existing conditions from the old DA so we don't get them twice
  // when we call copy_non_errors_from_da below.
  thd->push_diagnostics_area(&tmp_da, false);


  /*
    If we have InnoDB table, then we try to get statistics
    without opening the table.
  */
  if (!my_strcasecmp(system_charset_info,
                     engine_name_ptr.ptr(),
                    "InnoDB"))
    result= read_stat_from_SE(thd,
                              schema_name_ptr,
                              table_name_ptr,
                              index_name_ptr,
                              index_ordinal_position,
                              column_ordinal_position,
                              se_private_id,
                              stype);
  else
    result= read_stat_by_open_table(thd,
                                    schema_name_ptr,
                                    table_name_ptr,
                                    index_name_ptr,
                                    column_ordinal_position,
                                    stype);

  thd->pop_diagnostics_area();

  // Pass an error if any.
  if (!thd->is_error() && tmp_da.is_error())
  {
    da->set_error_status(tmp_da.mysql_errno(),
                         tmp_da.message_text(),
                         tmp_da.returned_sqlstate());
    da->push_warning(thd,
                     tmp_da.mysql_errno(),
                     tmp_da.returned_sqlstate(),
                     Sql_condition::SL_ERROR,
                     tmp_da.message_text());
  }

  // Pass warnings (if any).
  //
  // Filter out warnings with SL_ERROR level, because they
  // correspond to the errors which were filtered out in fill_table().
  da->copy_non_errors_from_da(thd, &tmp_da);


  DBUG_RETURN(result);
}


// Fetch stats from SE
ulonglong Statistics_cache::read_stat_from_SE(
            THD *thd,
            const String &schema_name_ptr,
            const String &table_name_ptr,
            const String &index_name_ptr,
            uint index_ordinal_position,
            uint column_ordinal_position,
            Object_id se_private_id,
            enum_statistics_type stype)
{
  DBUG_ENTER("Statistics_cache::read_stat_from_SE");

  uint se_flags= 0;
  bool ignore_cache= false;
  ulonglong return_value= 0;

  // Stop we have see and error already for this table.
  if (check_error_for_key(schema_name_ptr, table_name_ptr))
    DBUG_RETURN(0);

  /**
    It is faster to get first three statistics (below) alone when
    compared to getting them all.

    Also, Innodb does not give us check_time and checksum so we
    return from here.

    Notes for future, If there is a way to know which statistics
    have been requested in user query, then we can try to request
    SE with only those required statistics. E.g., A query
    requesting AUTO_INCREMENT and TABLE_ROWS both together would
    perform faster if we can combine HA_STATUS_AUTO |
    HA_STATUS_VARIABLE. Because optimizer silently removes unused
    internal UDF, we have no way to determine exactly what user
    had in the query.

    Currently if a user query requests just HA_STATUS_AUTO, it
    performance twice faster than requesting HA_STATUS_VARIABLE.
    So, for now we cache only HA_STATUS_VARIABLE, and skip cache
    for rest.
  */
  switch (stype)
  {
  case enum_statistics_type::TABLE_UPDATE_TIME:
    se_flags= HA_STATUS_TIME;
    ignore_cache= true;
    break;

  case enum_statistics_type::DATA_FREE:
    se_flags= HA_STATUS_VARIABLE_EXTRA;
    ignore_cache= true;
    break;

  case enum_statistics_type::AUTO_INCREMENT:
    se_flags= HA_STATUS_AUTO;
    ignore_cache= true;
    break;

  case enum_statistics_type::CHECK_TIME:
  case enum_statistics_type::CHECKSUM:
    // InnoDB does return always zero for these statistics.
    DBUG_RETURN(0);

  case enum_statistics_type::INDEX_COLUMN_CARDINALITY:
    ignore_cache= true;
    break;

  default:
    se_flags= HA_STATUS_VARIABLE;
  }


  //
  // Get statistics from cache, if available
  //

  if (!ignore_cache && is_stat_cached(schema_name_ptr, table_name_ptr))
    DBUG_RETURN(get_stat(stype));

  //
  // Get statistics from InnoDB SE
  //
  ha_statistics ha_stat;

  // Build table name as required by InnoDB
  uint error= 0;
  handlerton *hton= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  DBUG_ASSERT(hton); // InnoDB HA cannot be optional

  // Acquire MDL_EXPLICIT lock on table.
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE,
                   schema_name_ptr.ptr(),
                   table_name_ptr.ptr(),
                   MDL_SHARED_HIGH_PRIO, MDL_EXPLICIT);

  // Push deadlock error handler
  MDL_deadlock_error_handler mdl_deadlock_error_handler(thd, &schema_name_ptr,
                                                        &table_name_ptr);
  thd->push_internal_handler(&mdl_deadlock_error_handler);

  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout))
    error= -1;

  thd->pop_internal_handler();

  DEBUG_SYNC(thd, "after_acquiring_mdl_shared_to_fetch_stats");

  if (error == 0)
  {
    error= -1;

    //
    // Read statistics from SE
    //
    return_value= -1;

    if (stype == enum_statistics_type::INDEX_COLUMN_CARDINALITY &&
        hton->get_index_column_cardinality &&
        !hton->get_index_column_cardinality(
                 schema_name_ptr.ptr(),
                 table_name_ptr.ptr(),
                 index_name_ptr.ptr(),
                 index_ordinal_position,
                 column_ordinal_position,
                 se_private_id,
                 &return_value))
    {
      error= 0;
    }
    else if (hton->get_table_statistics &&
        !hton->get_table_statistics(schema_name_ptr.ptr(),
                                    table_name_ptr.ptr(),
                                    se_private_id,
                                    se_flags,
                                    &ha_stat))
    {
      error= 0;
    }

    // Release the lock we got
    thd->mdl_context.release_lock(mdl_request.ticket);
  }

  // Cache and return the statistics
  if (error == 0)
  {
    if (!ignore_cache)
      cache_stats(schema_name_ptr, table_name_ptr, ha_stat);

    // Only cardinality is not stored in the cache.
    if (stype != enum_statistics_type::INDEX_COLUMN_CARDINALITY)
      return_value= get_stat(ha_stat, stype);

    DBUG_RETURN(return_value);
  }
  else if (thd->is_error())
  {
    /*
      Hide error for a non-existing table.
      For example, this error can occur when we use a where condition
      with a db name and table, but the table does not exist.
     */
    if (!(thd->get_stmt_da()->mysql_errno() == ER_NO_SUCH_TABLE) &&
        !(thd->get_stmt_da()->mysql_errno() == ER_WRONG_OBJECT))
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());

    /* Cache empty statistics when we see a error.
       This will make sure,

       1. You will not invoke open_tables_for_query() gain.

       2. You will not see junk values for statistics in results.
    */
    cache_stats(schema_name_ptr, table_name_ptr, ha_stat);

    m_error= thd->get_stmt_da()->message_text();
    thd->clear_error();
  }

  DBUG_RETURN(error);
}


// Fetch stats by opening the table.
ulonglong Statistics_cache::read_stat_by_open_table(
            THD *thd,
            const String &schema_name_ptr,
            const String &table_name_ptr,
            const String &index_name_ptr,
            uint column_ordinal_position,
            enum_statistics_type stype)
{
  DBUG_ENTER("Statistics_cache::read_stat_by_open_table");
  ulonglong return_value= 0;
  uint error= 0;
  ha_statistics ha_stat;

  //
  // Get statistics from cache, if available
  //

  if (check_error_for_key(schema_name_ptr, table_name_ptr))
    DBUG_RETURN(0);

  if (stype != enum_statistics_type::INDEX_COLUMN_CARDINALITY &&
      is_stat_cached(schema_name_ptr, table_name_ptr))
    DBUG_RETURN(get_stat(stype));


  //
  // Get statistics by opening the table
  //

  MDL_deadlock_error_handler mdl_deadlock_error_handler(thd, &schema_name_ptr,
                                                        &table_name_ptr);
  Open_tables_backup open_tables_state_backup;
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup, 0);

  Query_arena i_s_arena(thd->mem_root,
                        Query_arena::STMT_CONVENTIONAL_EXECUTION);
  Query_arena *old_arena= thd->stmt_arena;
  thd->stmt_arena= &i_s_arena;
  Query_arena backup_arena;
  thd->set_n_backup_active_arena(&i_s_arena, &backup_arena);

  LEX temp_lex, *lex;
  LEX *old_lex= thd->lex;
  thd->lex= lex= &temp_lex;

  lex_start(thd);
  lex->context_analysis_only= CONTEXT_ANALYSIS_ONLY_VIEW;

  LEX_CSTRING db_name_lex_cstr, table_name_lex_cstr;
  if (!thd->make_lex_string(&db_name_lex_cstr, schema_name_ptr.ptr(),
                            schema_name_ptr.length(), FALSE) ||
      !thd->make_lex_string(&table_name_lex_cstr, table_name_ptr.ptr(),
                            table_name_ptr.length(), FALSE))
  {
    error= -1;
    goto end;
  }

  if (make_table_list(thd, lex->select_lex, db_name_lex_cstr,
                      table_name_lex_cstr))
  {
    error= -1;
    goto end;
  }

  TABLE_LIST *table_list;
  table_list= lex->select_lex->table_list.first;
  table_list->required_type= dd::enum_table_type::BASE_TABLE;

  /*
    Let us set fake sql_command so views won't try to merge
    themselves into main statement. If we don't do this,
    SELECT * from information_schema.xxxx will cause problems.
    SQLCOM_SHOW_FIELDS is used because it satisfies
    'only_view_structure()'.
   */
  lex->sql_command= SQLCOM_SELECT;

  DBUG_EXECUTE_IF("simulate_kill_query_on_open_table",
                  DBUG_SET("+d,kill_query_on_open_table_from_tz_find"););

  // Push deadlock error handler.
  thd->push_internal_handler(&mdl_deadlock_error_handler);

  bool open_result;
  open_result= open_tables_for_query(thd, table_list,
                                     MYSQL_OPEN_IGNORE_FLUSH |
                                     MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL);

  thd->pop_internal_handler();

  DBUG_EXECUTE_IF("simulate_kill_query_on_open_table",
                  DBUG_SET("-d,kill_query_on_open_table_from_tz_find"););
  DEBUG_SYNC(thd, "after_open_table_mdl_shared_to_fetch_stats");

  if (!open_result && table_list->is_view_or_derived())
  {
    open_result= table_list->resolve_derived(thd, false);
    if (!open_result)
      open_result= table_list->setup_materialized_derived(thd);
  }

  /*
    Restore old value of sql_command back as it is being looked at in
    process_table() function.
   */
  lex->sql_command= old_lex->sql_command;

  if (open_result)
  {
    DBUG_ASSERT(thd->is_error() || thd->is_killed());

    if (thd->is_error())
    {
      /*
        Hide error for a non-existing table.
        For example, this error can occur when we use a where condition
        with a db name and table, but the table does not exist.
      */
      if (!(thd->get_stmt_da()->mysql_errno() == ER_NO_SUCH_TABLE) &&
          !(thd->get_stmt_da()->mysql_errno() == ER_WRONG_OBJECT))
        push_warning(thd, Sql_condition::SL_WARNING,
                     thd->get_stmt_da()->mysql_errno(),
                     thd->get_stmt_da()->message_text());

      /* Cache empty statistics when we see a error.
         This will make sure,

         1. You will not invoke open_tables_for_query() gain.

         2. You will not see junk values for statistics in results.
      */
      cache_stats(schema_name_ptr, table_name_ptr, ha_stat);

      m_error= thd->get_stmt_da()->message_text();
      thd->clear_error();
    }
    else
    {
      /*
        Table open fails even when query or connection is killed. In this
        case Diagnostics_area might not be set. So just returning error from
        here. Query is later terminated by call to send_kill_message() when
        we check thd->killed flag.
      */
      error= -1;
    }

    goto end;
  }
  else if (!table_list->is_view() && !table_list->schema_table)
  {
    if (table_list->table->file->info(HA_STATUS_VARIABLE |
                                      HA_STATUS_TIME |
                                      HA_STATUS_VARIABLE_EXTRA |
                                      HA_STATUS_AUTO) != 0)
    {
      if (thd->is_error())
      {
        push_warning(thd, Sql_condition::SL_WARNING,
                     thd->get_stmt_da()->mysql_errno(),
                     thd->get_stmt_da()->message_text());

        /* Cache empty statistics when we see a error.
           This will make sure,

           1. You will not invoke open_tables_for_query() gain.

           2. You will not see junk values for statistics in results.
        */
        cache_stats(schema_name_ptr, table_name_ptr, ha_stat);

        m_error= thd->get_stmt_da()->message_text();
        thd->clear_error();
      }
      else
        error= -1;

      goto end;
    }

    // If we are reading cardinality, just read and do not cache it.
    if (stype == enum_statistics_type::INDEX_COLUMN_CARDINALITY)
    {
      TABLE *table= table_list->table;
      uint key_index= 0;

      // Search for key with the index name.
      while (key_index < table->s->keys)
      {
        if (!my_strcasecmp(system_charset_info,
                           (table->key_info+key_index)->name,
                           index_name_ptr.ptr()))
          break;

        key_index++;
      }

      KEY *key= table->s->key_info + key_index;

      // Calculate the cardinality.
      ha_rows records;
      if (key_index < table->s->keys &&
          key->has_records_per_key(column_ordinal_position))
      {
        records=(table->file->stats.records /
                 key->records_per_key(column_ordinal_position));
        records= static_cast<longlong>(round(records));
      }
      else
        records= -1; // Treated as NULL

      return_value= (ulonglong) records;
    }
    else // Get all statistics and cache them.
    {
      cache_stats(schema_name_ptr, table_name_ptr, table_list->table->file);
      return_value= get_stat(stype);
    }
  }
  else
  {
    error= -1;
    goto end;
  }


end:
  lex->unit->cleanup(true);

  /* Restore original LEX value, statement's arena and THD arena values. */
  lex_end(thd->lex);

  // Free items, before restoring backup_arena below.
  DBUG_ASSERT(i_s_arena.free_list == NULL);
  thd->free_items();

  /*
    For safety reset list of open temporary tables before closing
    all tables open within this Open_tables_state.
   */
  close_thread_tables(thd);
  /*
    Release metadata lock we might have acquired.
    See comment in fill_schema_table_from_frm() for details.
   */
  thd->mdl_context.rollback_to_savepoint(open_tables_state_backup.mdl_system_tables_svp);

  thd->lex= old_lex;

  thd->stmt_arena= old_arena;
  thd->restore_active_arena(&i_s_arena, &backup_arena);

  thd->restore_backup_open_tables_state(&open_tables_state_backup);

  /*
    ER_LOCK_DEADLOCK is converted to ER_WARN_I_S_SKIPPED_TABLE by deadlock
    error handler used here.
    If rollback request is set by other deadlock error handlers then
    reset it here.
  */
  if (mdl_deadlock_error_handler.is_error_handled() &&
      thd->transaction_rollback_request)
    thd->transaction_rollback_request= false;

  DBUG_RETURN(error==0 ? return_value : error);
}


}
}


/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/info_schema/tablespace_stats.h"  // dd::info_schema::Tablespace...

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
#include "partition_info.h"                   // partition_info
#include "partitioning/partition_handler.h"   // Partition_handler
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

/**
  Error handler class to convert ER_LOCK_DEADLOCK error to
  ER_I_S_SKIPPED_TABLESPACE error.

  Handler is pushed for opening a table or acquiring a MDL lock on
  tables for INFORMATION_SCHEMA views(system views) operations.
*/
class MDL_deadlock_error_handler : public Internal_error_handler
{
public:
  MDL_deadlock_error_handler(THD *thd, const String *tablespace_name)
    : m_can_deadlock(thd->mdl_context.has_locks()),
      m_tablespace_name(tablespace_name)
  {}

  virtual bool handle_condition(THD*,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level*,
                                const char*)
  {
    if (sql_errno == ER_LOCK_DEADLOCK && m_can_deadlock)
    {
      // Convert error to ER_I_S_SKIPPED_TABLESPACE.
      my_error(ER_I_S_SKIPPED_TABLESPACE, MYF(0),
               m_tablespace_name->ptr());

      m_error_handled= true;
    }

    return false;
  }

  bool is_error_handled() const { return m_error_handled; }

private:
  bool m_can_deadlock;

  // Table name
  const String *m_tablespace_name;

  // Flag to indicate whether deadlock error is handled by the handler or not.
  bool m_error_handled= false;
};


// Returns the required statistics from the cache.
void Tablespace_statistics::get_stat(enum_tablespace_stats_type stype,
                                     ulonglong *result)
{
  switch (stype)
  {
    case enum_tablespace_stats_type::TS_ID:
      *result= m_stats.m_id;
      return;

    case enum_tablespace_stats_type::TS_LOGFILE_GROUP_NUMBER:
      *result= m_stats.m_logfile_group_number;
      return;

    case enum_tablespace_stats_type::TS_FREE_EXTENTS:
      *result= m_stats.m_free_extents;
      return;

    case enum_tablespace_stats_type::TS_TOTAL_EXTENTS:
      *result= m_stats.m_total_extents;
      return;

    case enum_tablespace_stats_type::TS_EXTENT_SIZE:
      *result= m_stats.m_extent_size;
      return;

    case enum_tablespace_stats_type::TS_INITIAL_SIZE:
      *result= m_stats.m_initial_size;
      return;

    case enum_tablespace_stats_type::TS_MAXIMUM_SIZE:
        *result= m_stats.m_maximum_size;
      return;

    case enum_tablespace_stats_type::TS_AUTOEXTEND_SIZE:
      *result= m_stats.m_autoextend_size;
      return;

    case enum_tablespace_stats_type::TS_VERSION:
      *result= m_stats.m_version;
      return;

    case enum_tablespace_stats_type::TS_DATA_FREE:
      *result= m_stats.m_data_free;
      return;

  default:
    case enum_tablespace_stats_type::TS_TYPE:
    case enum_tablespace_stats_type::TS_LOGFILE_GROUP_NAME:
    case enum_tablespace_stats_type::TS_ROW_FORMAT:
    case enum_tablespace_stats_type::TS_STATUS:
    DBUG_ASSERT(!"Should not hit here");
    return;
  }

  DBUG_ASSERT(!"Should not hit here");
  return;
}

// Returns the required statistics from the cache.
void Tablespace_statistics::get_stat(enum_tablespace_stats_type stype,
                                     String_type *result)
{
  switch (stype)
  {
    case enum_tablespace_stats_type::TS_TYPE:
      *result= m_stats.m_type;
      return;

    case enum_tablespace_stats_type::TS_LOGFILE_GROUP_NAME:
      *result= m_stats.m_logfile_group_name;
      return;

    case enum_tablespace_stats_type::TS_ROW_FORMAT:
      *result= m_stats.m_row_format;
      return;

    case enum_tablespace_stats_type::TS_STATUS:
      *result= m_stats.m_status;
      return;

    case enum_tablespace_stats_type::TS_ID:
    case enum_tablespace_stats_type::TS_LOGFILE_GROUP_NUMBER:
    case enum_tablespace_stats_type::TS_FREE_EXTENTS:
    case enum_tablespace_stats_type::TS_TOTAL_EXTENTS:
    case enum_tablespace_stats_type::TS_EXTENT_SIZE:
    case enum_tablespace_stats_type::TS_INITIAL_SIZE:
    case enum_tablespace_stats_type::TS_MAXIMUM_SIZE:
    case enum_tablespace_stats_type::TS_AUTOEXTEND_SIZE:
    case enum_tablespace_stats_type::TS_VERSION:
    case enum_tablespace_stats_type::TS_DATA_FREE:
    default:
      DBUG_ASSERT(!"Should not hit here");

    return;
  }
  DBUG_ASSERT(!"Should not hit here");

  return;
}


/*
  Read dynamic table statistics from SE OR by reading cached statistics
  from SELECT_LEX.
*/
bool Tablespace_statistics::read_stat(
       THD *thd,
       const String &tablespace_name_ptr,
       const String &file_name_ptr,
       const char* ts_se_private_data)
{
  DBUG_ENTER("Tablespace_statistics::read_stat");
  bool error= false;

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
  error= read_stat_from_SE(thd, tablespace_name_ptr, file_name_ptr,
                           ts_se_private_data);
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


  DBUG_RETURN(error);
}


// Fetch stats from SE
bool Tablespace_statistics::read_stat_from_SE(
       THD *thd,
       const String &tablespace_name_ptr,
       const String &file_name_ptr,
       const char* ts_se_private_data)
{
  DBUG_ENTER("Tablespace_statistics::read_stat_from_SE");

  // Stop we have see and error already for this table.
  if (check_error_for_key(tablespace_name_ptr, file_name_ptr))
    DBUG_RETURN(true);

  //
  // Get statistics from cache, if available
  //

  if (is_stat_cached(tablespace_name_ptr, file_name_ptr))
    DBUG_RETURN(false);

  //
  // Get statistics from InnoDB SE
  //
  ha_tablespace_statistics ha_tablespace_stat;

  // Build table name as required by InnoDB
  handlerton *hton= ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  DBUG_ASSERT(hton); // InnoDB HA cannot be optional

  // Acquire MDL_EXPLICIT lock on table.
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLESPACE,
                   "",
                   tablespace_name_ptr.ptr(),
                   MDL_SHARED_HIGH_PRIO, MDL_EXPLICIT);

  // Push deadlock error handler
  bool error= false;
  MDL_deadlock_error_handler mdl_deadlock_error_handler(thd,
                                                        &tablespace_name_ptr);
  thd->push_internal_handler(&mdl_deadlock_error_handler);
  error= thd->mdl_context.acquire_lock(&mdl_request,
                                       thd->variables.lock_wait_timeout);
  thd->pop_internal_handler();

  if (!error)
  {
    /*
      It is possible that 'se_private_data' is not supplied to this
      function. The function dd::Properties::parse_properties() would
      at-least needs a single key-value pair to return a dd::Properties
      object. So, when se_private_data is not supplied, we force creation
      of dd::Properties object by passing a dummy=0 key-value pair.
    */
    std::unique_ptr<dd::Properties> ts_se_private_data_obj(
      dd::Properties::parse_properties(
        ts_se_private_data?ts_se_private_data:"dummy=0;"));

    //
    // Read statistics from SE
    //

    DBUG_ASSERT(hton->get_tablespace_statistics);
    error= hton->get_tablespace_statistics(tablespace_name_ptr.ptr(),
                                           file_name_ptr.ptr(),
                                           *ts_se_private_data_obj.get(),
                                           &ha_tablespace_stat);

    // Release the lock we got
    thd->mdl_context.release_lock(mdl_request.ticket);
  }

  // Cache statistics.
  cache_stats(tablespace_name_ptr, file_name_ptr, ha_tablespace_stat);

  if (thd->is_error())
  {
    push_warning(thd, Sql_condition::SL_WARNING,
                 thd->get_stmt_da()->mysql_errno(),
                 thd->get_stmt_da()->message_text());
    m_error= thd->get_stmt_da()->message_text();
    thd->clear_error();
  }

  DBUG_RETURN(error);
}


}
}


/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SYSTEM_VARIABLES_INCLUDED
#define SYSTEM_VARIABLES_INCLUDED

#include "my_global.h"
#include "my_base.h"          // ha_rows
#include "rpl_gtid.h"         // Gitd_specification
#include "sql_plugin_ref.h"   // plugin_ref

class MY_LOCALE;
class Time_zone;
typedef ulonglong sql_mode_t;
typedef uint32 my_thread_id;
typedef struct st_list LIST;
typedef struct charset_info_st CHARSET_INFO;

struct System_variables
{
  /*
    How dynamically allocated system variables are handled:

    The global_system_variables and max_system_variables are "authoritative"
    They both should have the same 'version' and 'size'.
    When attempting to access a dynamic variable, if the session version
    is out of date, then the session version is updated and realloced if
    neccessary and bytes copied from global to make up for missing data.
  */
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint dynamic_variables_head;    /* largest valid variable offset */
  uint dynamic_variables_size;    /* how many bytes are in use */
  LIST *dynamic_variables_allocs; /* memory hunks for PLUGIN_VAR_MEMALLOC */

  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  my_bool end_markers_in_json;
  /* A bitmap for switching optimizations on/off */
  ulonglong optimizer_switch;
  ulonglong optimizer_trace; ///< bitmap to tune optimizer tracing
  ulonglong optimizer_trace_features; ///< bitmap to select features to trace
  long      optimizer_trace_offset;
  long      optimizer_trace_limit;
  ulong     optimizer_trace_max_mem_size;
  sql_mode_t sql_mode; ///< which non-standard SQL behaviour should be enabled
  ulonglong option_bits; ///< OPTION_xxx constants, e.g. OPTION_PROFILING
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  uint  eq_range_index_dive_limit;
  ulong join_buff_size;
  ulong lock_wait_timeout;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong multi_range_count;
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong myisam_stats_method;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong div_precincrement;
  ulong sortbuff_size;
  ulong max_sp_recursion_depth;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong group_concat_max_len;

  ulong binlog_format; ///< binlog format for this thd (see enum_binlog_format)
  ulong rbr_exec_mode_options;
  my_bool binlog_direct_non_trans_update;
  ulong binlog_row_image;
  my_bool sql_log_bin;
  ulong transaction_write_set_extraction;
  ulong completion_type;
  ulong query_cache_type;
  ulong tx_isolation;
  ulong updatable_views_with_limit;
  uint max_user_connections;
  ulong my_aes_mode;

  /**
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  my_thread_id pseudo_thread_id;
  /**
    Default transaction access mode. READ ONLY (true) or READ WRITE (false).
  */
  my_bool tx_read_only;
  my_bool low_priority_updates;
  my_bool new_mode;
  my_bool query_cache_wlock_invalidate;
  my_bool keep_files_on_create;

  my_bool old_alter_table;
  uint old_passwords;
  my_bool big_tables;

  plugin_ref table_plugin;
  plugin_ref temp_table_plugin;

  /* Only charset part of these variables is sensible */
  const CHARSET_INFO *character_set_filesystem;
  const CHARSET_INFO *character_set_client;
  const CHARSET_INFO *character_set_results;

  /* Both charset and collation parts of these variables are important */
  const CHARSET_INFO  *collation_server;
  const CHARSET_INFO  *collation_database;
  const CHARSET_INFO  *collation_connection;

  /* Error messages */
  MY_LOCALE *lc_messages;
  /* Locale Support */
  MY_LOCALE *lc_time_names;

  Time_zone *time_zone;
  /*
    TIMESTAMP fields are by default created with DEFAULT clauses
    implicitly without users request. This flag when set, disables
    implicit default values and expect users to provide explicit
    default clause. i.e., when set columns are defined as NULL,
    instead of NOT NULL by default.
  */
  my_bool explicit_defaults_for_timestamp;

  my_bool sysdate_is_now;
  my_bool binlog_rows_query_log_events;

  double long_query_time_double;

  my_bool pseudo_slave_mode;

  Gtid_specification gtid_next;
  Gtid_set_or_null gtid_next_list;
  ulong session_track_gtids;

  ulong max_statement_time;

  char *track_sysvars_ptr;
  my_bool session_track_schema;
  my_bool session_track_state_change;
  /**
    Compatibility option to mark the pre MySQL-5.6.4 temporals columns using
    the old format using comments for SHOW CREATE TABLE and in I_S.COLUMNS
    'COLUMN_TYPE' field.
  */
  my_bool show_old_temporals;
};


/**
  Per thread status variables.
  Must be long/ulong up to last_system_status_var so that
  add_to_status/add_diff_to_status can work.
*/

struct System_status_var
{
  /* IMPORTANT! See first_system_status_var definition below. */
  ulonglong created_tmp_disk_tables;
  ulonglong created_tmp_tables;
  ulonglong ha_commit_count;
  ulonglong ha_delete_count;
  ulonglong ha_read_first_count;
  ulonglong ha_read_last_count;
  ulonglong ha_read_key_count;
  ulonglong ha_read_next_count;
  ulonglong ha_read_prev_count;
  ulonglong ha_read_rnd_count;
  ulonglong ha_read_rnd_next_count;
  /*
    This number doesn't include calls to the default implementation and
    calls made by range access. The intent is to count only calls made by
    BatchedKeyAccess.
  */
  ulonglong ha_multi_range_read_init_count;
  ulonglong ha_rollback_count;
  ulonglong ha_update_count;
  ulonglong ha_write_count;
  ulonglong ha_prepare_count;
  ulonglong ha_discover_count;
  ulonglong ha_savepoint_count;
  ulonglong ha_savepoint_rollback_count;
  ulonglong ha_external_lock_count;
  ulonglong opened_tables;
  ulonglong opened_shares;
  ulonglong table_open_cache_hits;
  ulonglong table_open_cache_misses;
  ulonglong table_open_cache_overflows;
  ulonglong select_full_join_count;
  ulonglong select_full_range_join_count;
  ulonglong select_range_count;
  ulonglong select_range_check_count;
  ulonglong select_scan_count;
  ulonglong long_query_count;
  ulonglong filesort_merge_passes;
  ulonglong filesort_range_count;
  ulonglong filesort_rows;
  ulonglong filesort_scan_count;
  /* Prepared statements and binary protocol. */
  ulonglong com_stmt_prepare;
  ulonglong com_stmt_reprepare;
  ulonglong com_stmt_execute;
  ulonglong com_stmt_send_long_data;
  ulonglong com_stmt_fetch;
  ulonglong com_stmt_reset;
  ulonglong com_stmt_close;

  ulonglong bytes_received;
  ulonglong bytes_sent;

  ulonglong max_statement_time_exceeded;
  ulonglong max_statement_time_set;
  ulonglong max_statement_time_set_failed;

  /* Number of statements sent from the client. */
  ulonglong questions;

  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];

  /*
    IMPORTANT! See last_system_status_var definition below. Variables after
    'last_system_status_var' cannot be handled automatically by add_to_status()
    and add_diff_to_status().
  */
  double last_query_cost;
  ulonglong last_query_partial_plans;
};

/*
  This must reference the LAST ulonglong variable in system_status_var that is
  used as a global counter. It marks the end of a contiguous block of counters
  that can be iteratively totaled. See add_to_status().
*/
#define LAST_STATUS_VAR questions

/*
  This must reference the FIRST ulonglong variable in system_status_var that is
  used as a global counter. It marks the start of a contiguous block of counters
  that can be iteratively totaled.
*/
#define FIRST_STATUS_VAR created_tmp_disk_tables

/* Number of contiguous global status variables. */
const int COUNT_GLOBAL_STATUS_VARS= ((offsetof(System_status_var, LAST_STATUS_VAR) -
                                      offsetof(System_status_var, FIRST_STATUS_VAR)) /
                                      sizeof(ulonglong)) + 1;


void add_diff_to_status(System_status_var *to_var,
                        System_status_var *from_var,
                        System_status_var *dec_var);


void add_to_status(System_status_var *to_var, const System_status_var *from_var,
                   bool add_com_vars);

#endif // SYSTEM_VARIABLES_INCLUDED

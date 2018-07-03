/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SYSTEM_VARIABLES_INCLUDED
#define SYSTEM_VARIABLES_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "my_base.h"  // ha_rows
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_thread_local.h"     // my_thread_id
#include "sql/rpl_gtid.h"        // Gitd_specification
#include "sql/sql_plugin_ref.h"  // plugin_ref

class MY_LOCALE;
class Time_zone;

typedef ulonglong sql_mode_t;
struct LIST;

// Values for binlog_format sysvar
enum enum_binlog_format {
  BINLOG_FORMAT_MIXED = 0,  ///< statement if safe, otherwise row - autodetected
  BINLOG_FORMAT_STMT = 1,   ///< statement-based
  BINLOG_FORMAT_ROW = 2,    ///< row-based
  BINLOG_FORMAT_UNSPEC =
      3  ///< thd_binlog_format() returns it when binlog is closed
};

// Values for rbr_exec_mode_options sysvar
enum enum_rbr_exec_mode {
  RBR_EXEC_MODE_STRICT,
  RBR_EXEC_MODE_IDEMPOTENT,
  RBR_EXEC_MODE_LAST_BIT
};

// Values for binlog_row_image sysvar
enum enum_binlog_row_image {
  /** PKE in the before image and changed columns in the after image */
  BINLOG_ROW_IMAGE_MINIMAL = 0,
  /** Whenever possible, before and after image contain all columns except
     blobs. */
  BINLOG_ROW_IMAGE_NOBLOB = 1,
  /** All columns in both before and after image. */
  BINLOG_ROW_IMAGE_FULL = 2
};

// Bits for binlog_row_value_options sysvar
enum enum_binlog_row_value_options {
  /// Store JSON updates in partial form
  PARTIAL_JSON_UPDATES = 1
};

// Values for binlog_row_metadata sysvar
enum enum_binlog_row_metadata {
  BINLOG_ROW_METADATA_MINIMAL = 0,
  BINLOG_ROW_METADATA_FULL = 1
};

// Values for transaction_write_set_extraction sysvar
enum enum_transaction_write_set_hashing_algorithm {
  HASH_ALGORITHM_OFF = 0,
  HASH_ALGORITHM_MURMUR32 = 1,
  HASH_ALGORITHM_XXHASH64 = 2
};

// Values for session_track_gtids sysvar
enum enum_session_track_gtids { OFF = 0, OWN_GTID = 1, ALL_GTIDS = 2 };

/** Values for use_secondary_engine sysvar. */
enum use_secondary_engine {
  SECONDARY_ENGINE_OFF = 0,
  SECONDARY_ENGINE_ON = 1,
  SECONDARY_ENGINE_FORCED = 2
};

/* Bits for different SQL modes modes (including ANSI mode) */
#define MODE_REAL_AS_FLOAT 1
#define MODE_PIPES_AS_CONCAT 2
#define MODE_ANSI_QUOTES 4
#define MODE_IGNORE_SPACE 8
#define MODE_NOT_USED 16
#define MODE_ONLY_FULL_GROUP_BY 32
#define MODE_NO_UNSIGNED_SUBTRACTION 64
#define MODE_NO_DIR_IN_CREATE 128
#define MODE_ANSI 262144L
#define MODE_NO_AUTO_VALUE_ON_ZERO (MODE_ANSI * 2)
#define MODE_NO_BACKSLASH_ESCAPES (MODE_NO_AUTO_VALUE_ON_ZERO * 2)
#define MODE_STRICT_TRANS_TABLES (MODE_NO_BACKSLASH_ESCAPES * 2)
#define MODE_STRICT_ALL_TABLES (MODE_STRICT_TRANS_TABLES * 2)
/*
 * NO_ZERO_DATE, NO_ZERO_IN_DATE and ERROR_FOR_DIVISION_BY_ZERO modes are
 * removed in 5.7 and their functionality is merged with STRICT MODE.
 * However, For backward compatibility during upgrade, these modes are kept
 * but they are not used. Setting these modes in 5.7 will give warning and
 * have no effect.
 */
#define MODE_NO_ZERO_IN_DATE (MODE_STRICT_ALL_TABLES * 2)
#define MODE_NO_ZERO_DATE (MODE_NO_ZERO_IN_DATE * 2)
#define MODE_INVALID_DATES (MODE_NO_ZERO_DATE * 2)
#define MODE_ERROR_FOR_DIVISION_BY_ZERO (MODE_INVALID_DATES * 2)
#define MODE_TRADITIONAL (MODE_ERROR_FOR_DIVISION_BY_ZERO * 2)
#define MODE_HIGH_NOT_PRECEDENCE (1ULL << 29)
#define MODE_NO_ENGINE_SUBSTITUTION (MODE_HIGH_NOT_PRECEDENCE * 2)
#define MODE_PAD_CHAR_TO_FULL_LENGTH (1ULL << 31)
/*
  If this mode is set the fractional seconds which cannot fit in given fsp will
  be truncated.
*/
#define MODE_TIME_TRUNCATE_FRACTIONAL (1ULL << 32)

#define MODE_LAST (1ULL << 33)

#define MODE_ALLOWED_MASK                                                      \
  (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |              \
   MODE_IGNORE_SPACE | MODE_NOT_USED | MODE_ONLY_FULL_GROUP_BY |               \
   MODE_NO_UNSIGNED_SUBTRACTION | MODE_NO_DIR_IN_CREATE | MODE_ANSI |          \
   MODE_NO_AUTO_VALUE_ON_ZERO | MODE_NO_BACKSLASH_ESCAPES |                    \
   MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES | MODE_NO_ZERO_IN_DATE |  \
   MODE_NO_ZERO_DATE | MODE_INVALID_DATES | MODE_ERROR_FOR_DIVISION_BY_ZERO |  \
   MODE_TRADITIONAL | MODE_HIGH_NOT_PRECEDENCE | MODE_NO_ENGINE_SUBSTITUTION | \
   MODE_PAD_CHAR_TO_FULL_LENGTH | MODE_TIME_TRUNCATE_FRACTIONAL)

/*
  We can safely ignore and reset these obsolete mode bits while replicating:
*/
#define MODE_IGNORED_MASK                         \
  (0x00100 |  /* was: MODE_POSTGRESQL          */ \
   0x00200 |  /* was: MODE_ORACLE              */ \
   0x00400 |  /* was: MODE_MSSQL               */ \
   0x00800 |  /* was: MODE_DB2                 */ \
   0x01000 |  /* was: MODE_MAXDB               */ \
   0x02000 |  /* was: MODE_NO_KEY_OPTIONS      */ \
   0x04000 |  /* was: MODE_NO_TABLE_OPTIONS    */ \
   0x08000 |  /* was: MODE_NO_FIELD_OPTIONS    */ \
   0x10000 |  /* was: MODE_MYSQL323            */ \
   0x20000 |  /* was: MODE_MYSQL40             */ \
   0x10000000 /* was: MODE_NO_AUTO_CREATE_USER */ \
  )

/*
  Replication uses 8 bytes to store SQL_MODE in the binary log. The day you
  use strictly more than 64 bits by adding one more define above, you should
  contact the replication team because the replication code should then be
  updated (to store more bytes on disk).

  NOTE: When adding new SQL_MODE types, make sure to also add them to
  the scripts used for creating the MySQL system tables
  in scripts/mysql_system_tables.sql and scripts/mysql_system_tables_fix.sql
*/

struct System_variables {
  /*
    How dynamically allocated system variables are handled:

    The global_system_variables and max_system_variables are "authoritative"
    They both should have the same 'version' and 'size'.
    When attempting to access a dynamic variable, if the session version
    is out of date, then the session version is updated and realloced if
    neccessary and bytes copied from global to make up for missing data.
  */
  ulong dynamic_variables_version;
  char *dynamic_variables_ptr;
  uint dynamic_variables_head;    /* largest valid variable offset */
  uint dynamic_variables_size;    /* how many bytes are in use */
  LIST *dynamic_variables_allocs; /* memory hunks for PLUGIN_VAR_MEMALLOC */

  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  bool end_markers_in_json;
  bool windowing_use_high_precision;
  /* A bitmap for switching optimizations on/off */
  ulonglong optimizer_switch;
  ulonglong optimizer_trace;           ///< bitmap to tune optimizer tracing
  ulonglong optimizer_trace_features;  ///< bitmap to select features to trace
  long optimizer_trace_offset;
  long optimizer_trace_limit;
  ulong optimizer_trace_max_mem_size;
  sql_mode_t sql_mode;  ///< which non-standard SQL behaviour should be enabled
  ulonglong option_bits;  ///< OPTION_xxx constants, e.g. OPTION_PROFILING
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  uint eq_range_index_dive_limit;
  uint cte_max_recursion_depth;
  ulonglong histogram_generation_max_mem_size;
  ulong join_buff_size;
  ulong lock_wait_timeout;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_points_in_geometry;
  ulong max_sort_length;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulonglong parser_max_mem_size;
  ulong range_optimizer_max_mem_size;
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
  ulong binlog_format;  ///< binlog format for this thd (see enum_binlog_format)
  ulong rbr_exec_mode_options;  // see enum_rbr_exec_mode
  bool binlog_direct_non_trans_update;
  ulong binlog_row_image;  // see enum_binlog_row_image
  ulonglong binlog_row_value_options;
  bool sql_log_bin;
  // see enum_transaction_write_set_hashing_algorithm
  ulong transaction_write_set_extraction;
  ulong completion_type;
  ulong transaction_isolation;
  ulong updatable_views_with_limit;
  uint max_user_connections;
  ulong my_aes_mode;
  ulong ssl_fips_mode;
  /**
    Controls what resultset metadata will be sent to the client.
    @sa enum_resultset_metadata
  */
  ulong resultset_metadata;

  /**
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  my_thread_id pseudo_thread_id;
  /**
    Default transaction access mode. READ ONLY (true) or READ WRITE (false).
  */
  bool transaction_read_only;
  bool low_priority_updates;
  bool new_mode;
  bool keep_files_on_create;

  bool old_alter_table;
  bool big_tables;

  plugin_ref table_plugin;
  plugin_ref temp_table_plugin;

  /* Only charset part of these variables is sensible */
  const CHARSET_INFO *character_set_filesystem;
  const CHARSET_INFO *character_set_client;
  const CHARSET_INFO *character_set_results;

  /* Both charset and collation parts of these variables are important */
  const CHARSET_INFO *collation_server;
  const CHARSET_INFO *collation_database;
  const CHARSET_INFO *collation_connection;

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
  bool explicit_defaults_for_timestamp;

  bool sysdate_is_now;
  bool binlog_rows_query_log_events;

  double long_query_time_double;

  bool pseudo_slave_mode;

  Gtid_specification gtid_next;
  Gtid_set_or_null gtid_next_list;
  ulong session_track_gtids;  // see enum_session_track_gtids

  ulong max_execution_time;

  char *track_sysvars_ptr;
  bool session_track_schema;
  bool session_track_state_change;
  ulong session_track_transaction_info;

  /*
    Time in seconds, after which the statistics in mysql.table/index_stats
    get invalid
  */
  ulong information_schema_stats_expiry;

  /**
    Used for the verbosity of SHOW CREATE TABLE. Currently used for displaying
    the row format in the output even if the table uses default row format.
  */
  bool show_create_table_verbosity;

  /**
    Compatibility option to mark the pre MySQL-5.6.4 temporals columns using
    the old format using comments for SHOW CREATE TABLE and in I_S.COLUMNS
    'COLUMN_TYPE' field.
  */
  bool show_old_temporals;
  // Used for replication delay and lag monitoring
  ulonglong original_commit_timestamp;

  ulong
      internal_tmp_mem_storage_engine;  // enum_internal_tmp_mem_storage_engine

  const CHARSET_INFO *default_collation_for_utf8mb4;

  /** Used for controlling preparation of queries against secondary engine. */
  ulong use_secondary_engine;

  /** Used for controlling Group Replication consistency guarantees */
  ulong group_replication_consistency;

  bool sql_require_primary_key;

  /**
    Used in replication to determine the server version of the original server
    where the transaction was executed.
  */
  uint32_t original_server_version;

  /**
    Used in replication to determine the server version of the immediate server
    in the replication topology.
  */
  uint32_t immediate_server_version;
};

/**
  Per thread status variables.
  Must be long/ulong up to last_system_status_var so that
  add_to_status/add_diff_to_status can work.
*/

struct System_status_var {
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

  ulonglong max_execution_time_exceeded;
  ulonglong max_execution_time_set;
  ulonglong max_execution_time_set_failed;

  /* Number of statements sent from the client. */
  ulonglong questions;

  /// How many queries have been executed on a secondary storage engine.
  ulonglong secondary_engine_execution_count;

  ulong com_other;
  ulong com_stat[(uint)SQLCOM_END];

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
#define LAST_STATUS_VAR secondary_engine_execution_count

/*
  This must reference the FIRST ulonglong variable in system_status_var that is
  used as a global counter. It marks the start of a contiguous block of counters
  that can be iteratively totaled.
*/
#define FIRST_STATUS_VAR created_tmp_disk_tables

/* Number of contiguous global status variables. */
const int COUNT_GLOBAL_STATUS_VARS =
    ((offsetof(System_status_var, LAST_STATUS_VAR) -
      offsetof(System_status_var, FIRST_STATUS_VAR)) /
     sizeof(ulonglong)) +
    1;

void add_diff_to_status(System_status_var *to_var, System_status_var *from_var,
                        System_status_var *dec_var);

void add_to_status(System_status_var *to_var, System_status_var *from_var,
                   bool reset_from_var);

#endif  // SYSTEM_VARIABLES_INCLUDED

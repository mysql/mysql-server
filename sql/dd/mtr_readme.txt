/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// Copyright just here to pass server-side git copyright check.

Here are the details of tests that are disabled due
to new data dictionary implementation.

////////////////////////////////////////////////////////////
Following are the tests that mostly pass or will be
disabled due to some other dependencies.
////////////////////////////////////////////////////////////

/** LOW
  Deals with upgrade scenarios.
  Need to re-visit once we have WL6392
*/
i_main.mysql_upgrade                   WL6378_UPGRADE
main.mysql_upgrade                     WL6378_UPGRADE

///////////////////////////////////////////////////////////////////
// RELATED TO INNODB SE
///////////////////////////////////////////////////////////////////


/** HIGH/MEDIUM - Marko
  Relates to InnoDB recovery
  Marko: These seem to wait for WL#7141/WL#7016, which is after
  the 1st push.

  WL6378_DDL_ON_READ_ONLY means that the test is executing
  InnoDB in read-only mode (using --innodb-force-recovery),
  which used to allow DDL operations (mainly DROP TABLE, DROP INDEX)
  and block DML. With the Global DD, it also blocks any modification
  of DD tables, thus blocking any DDL.

  WL7141_WL7016_RECOVERY means that the crash recovery and related
  tests must be rewritten in WL#7141 and WL#7016.
*/
i_innodb.innodb_bug16631778            WL6378_DDL_ON_READ_ONLY
i_innodb.innodb-force-recovery-3       WL6378_DDL_ON_READ_ONLY
i_innodb.innodb_bug15878013            WL7141_WL7016_RECOVERY
innodb.alter_crash                     WL7141_WL7016_RECOVERY
i_innodb.innodb_bug14676345            WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_1           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_2           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_6           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_7           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_8           WL6795_WL7016_RECOVERY
innodb_zip.wl6501                      WL6795_WL7016_RECOVERY
innodb_zip.wl6501_1                    WL6795_WL7016_RECOVERY
innodb_zip.wl6501_debug                WL6795_WL7016_RECOVERY
innodb_zip.wl6501_error_1              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_3              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_4              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_5              WL6795_WL7016_RECOVERY

// A single test case is disabled. More info in the test case.
innodb.partition                       WL6378_ALTER_PARTITION_TABLESPACE

///////////////////////////////////////////////////////////////////
// TEST COMMENTED IN-LINE WITHIN .test FILES
///////////////////////////////////////////////////////////////////

/* LOW
  Direct modification of system tables which will be disallowed.
  Revisit after WL6391
*/
i_main.plugin_auth                     WL6378_MODIFIES_SYSTEM_TABLE

/*
  Allow dump/restore of innodb_index_stats and innodb_table_stats.
  See Bug#22655287
*/
main.mysqldump                         WL6378_DDL_ON_DD_TABLE
sysschema.mysqldump                    WL6378_DDL_ON_DD_TABLE

/** MEDIUM - Joh
  Needs understanding of test case scenario and bit more
  involved study to re-write these tests. We may or may
  not be successful in re-write, need to check.
*/
main.lock_sync                         WL6378_DEBUG_SYNC


///////////////////////////////////////////////////////////////////
// TESTS DISABLED DUE TO WL6599
///////////////////////////////////////////////////////////////////

/*
  Metadata of IS tables of thread pool plugin seem to be missing
  from DD tables. How to load metadata of IS tables from dynamic
  plugins ? - Gopal
*/
thread_pool.thread_pool_i_s : WL6599_TP_IS_TABLES_MISSING

/*
  Praveen
*/
main.mdl_sync               : WL6599_DEBUG_SYNC_REWRITE

// ANALYZE TABLE under LTWRL - Gopal
main.flush_read_lock        : WL6599_ANALYZE_TABLE_LTWRL

// Hangs after 5 contineous run using ./mtr --repeat=30 - Thayu
i_innodb.innodb_bug14150372 : WL6599_INNODB_SPORADIC

/*
  This test cases has few parts disabled.

  WL6599_NEEDS_VIEW_COLUMNS
*/
main.information_schema                 : WL6599_NEEDS_VIEW_COLUMNS

nist.nist_all                           : Enabled in 7167

sysschema.pr_create_synonym_db          : WL6599_TOLOWER_CI

Restrictions OR waiting for WL/Bug fixes:
=========================================

/*
  We cannot call ANALYZE TABLE under innodb read only mode now.
  This would be a restrictions with wl6599. It is recommended
  to use 'information_schema_stats=latest' to get latest
  statistics in read only mode.

  Not sure if we can remove the ANALYZE statements in the test
  case. Need to check with Satya (Innodb)?

  "Bug#21611899 creating table on non-innodb engine when
   innodb-read-only option is set"
  
*/
i_innodb.innodb_bug16083211   : WL6599_ANALYZE_READONLY


/*
  Metadata of view column definition is mssing in DD tables.
  Needs WL7167 - Praveen.
*/
funcs_1.is_schemata                     : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_views                        : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_character_sets               : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_coll_char_set_appl           : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_collations                   : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_tables_is                    : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_statistics                   : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_table_constraints            : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_key_column_usage             : WL6599_NEEDS_VIEW_COLUMNS
i_main.information_schema               : WL6599_NEEDS_VIEW_COLUMNS
main.show_check                         : WL6599_NEEDS_VIEW_COLUMNS
i_main.view                             : WL6599_NEEDS_VIEW_COLUMNS
i_rpl.rpl_load_view                     : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.innodb_views                    : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.memory_views                    : WL6599_NEEDS_VIEW_COLUMNS
sysschema.pr_diagnostics                : WL6599_NEEDS_VIEW_COLUMNS
test_sql_views_triggers                 : WL6599_NEEDS_VIEW_COLUMNS
test_sql_stored_procedures_functions    : WL6599_NEEDS_VIEW_COLUMNS, WL6599_SHOW_IN_SP
main.ctype_binary                       : WL6599_NEEDS_VIEW_COLUMNS
main.ctype_cp1251                       : WL6599_NEEDS_VIEW_COLUMNS
main.ctype_latin1                       : WL6599_NEEDS_VIEW_COLUMNS
main.ctype_utf8                         : WL6599_NEEDS_VIEW_COLUMNS
main.view                               : WL6599_NEEDS_VIEW_COLUMNS
main.information_schema_parameters      : WL6599_NEEDS_VIEW_COLUMNS
main.information_schema_routines        : WL6599_NEEDS_VIEW_COLUMNS
main.view_alias                         : WL6599_NEEDS_VIEW_COLUMNS
main.view_grant                         : WL6599_NEEDS_VIEW_COLUMNS
main.information_schema_db              : WL6599_NEEDS_VIEW_COLUMNS
main.ctype_ucs                          : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_basic                    : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_concurrency              : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_extended                 : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_charset                  : WL6599_NEEDS_VIEW_COLUMNS
main.mysqldump                          : WL6599_NEEDS_VIEW_COLUMNS
sysschema.v_schema_auto_increment_columns : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_columns_innodb               : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_columns_is                   : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_columns_memory               : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_columns_mysql                : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_statistics_is                : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_tables                       : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.is_columns_myisam               : WL6599_NEEDS_VIEW_COLUMNS

--big-test
rpl_nogtid.transactional_ddl_locking    : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump                          : WL6599_NEEDS_VIEW_COLUMNS
main.information_schema-big             : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_filters                  : WL6599_NEEDS_VIEW_COLUMNS
main.mysqlpump_multi_thread             : WL6599_NEEDS_VIEW_COLUMNS
funcs_1.myisam_views-big                : WL6599_NEEDS_VIEW_COLUMNS

/*
  Parts of following test cases are commented out due to WL7167
*/
i_main.mysqldump
i_main.view
i_main.gis
main.olap
main.gis
main.sp
sysschema.v_innodb_buffer_stats_by_table
sysschema.v_memory_global_by_current_bytes
sysschema.v_schema_table_statistics_with_buffer
sysschema.v_memory_by_user_by_current_bytes
sysschema.v_waits_global_by_latency
sysschema.v_schema_table_statistics
sysschema.v_memory_global_total
sysschema.v_processlist
sysschema.v_io_global_by_wait_by_latency
sysschema.v_user_summary_by_stages
sysschema.v_user_summary_by_file_io_type
sysschema.v_ps_digest_95th_percentile_by_avg_us
sysschema.v_user_summary_by_file_io
sysschema.v_io_by_thread_by_latency
sysschema.v_statements_with_errors_or_warnings
sysschema.v_schema_unused_indexes
sysschema.v_ps_schema_table_statistics_io
sysschema.v_user_summary
sysschema.v_wait_classes_global_by_latency
sysschema.v_host_summary_by_statement_latency
sysschema.v_user_summary_by_statement_latency
sysschema.v_waits_by_host_by_latency
sysschema.v_statement_analysis
sysschema.v_ps_check_lost_instrumentation
sysschema.v_statements_with_sorting
sysschema.v_innodb_lock_waits
sysschema.v_wait_classes_global_by_avg_latency
sysschema.v_ps_digest_avg_latency_distribution
sysschema.v_waits_by_user_by_latency
sysschema.v_statements_with_full_table_scans
sysschema.v_schema_tables_with_full_table_scans
sysschema.v_host_summary_by_file_io_type
sysschema.v_host_summary
sysschema.v_memory_by_host_by_current_bytes
sysschema.v_host_summary_by_statement_type
sysschema.v_io_global_by_file_by_latency
sysschema.v_statements_with_temp_tables
sysschema.v_memory_by_thread_by_current_bytes
sysschema.v_latest_file_io
sysschema.v_io_global_by_file_by_bytes
sysschema.v_user_summary_by_statement_type
sysschema.v_schema_index_statistics
sysschema.v_statements_with_runtimes_in_95th_percentile
sysschema.v_innodb_buffer_stats_by_schema
sysschema.v_host_summary_by_file_io
rpl.rpl_view
innodb_gis.1
innodb_gis.gis

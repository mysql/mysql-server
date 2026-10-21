/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "health_monitor_report_manager.h"

#include "health_monitor.h"         // log
#include "health_monitor_log.h"     // log_warning()
#include "health_monitor_server.h"  // get_log_diagnostic_enable

#include <algorithm>  // for_each
#include <sstream>
#include <utility>  // move

using namespace rapidjson;

namespace hm {

Internal_Queries Report_Definition_Manager::m_internal_queries = {
#ifndef NDEBUG
    // Include queries provoking various error conditions in debug builds
    {"hm_err_parse_error",
     {"---------- Parse error ----------", "NO SQL COMMAND LIKE THIS"}},
    {"hm_err_empty_result_set",
     {"---------- Empty result set ----------", "SELECT NULL LIMIT 0"}},
    {"hm_err_no_such_table",
     {"---------- No such table ----------",
      "SELECT * FROM performance_schema.no_such_table"}},
    {"hm_err_missing_privilege",
     {"---------- Missing privilege ----------", "DROP TABLE mysql.user"}},
    {"hm_err_show_sql_mode",
     {"---------- Show SQL mode ----------", "SELECT @@SESSION.sql_mode"}},
    {"hm_err_bad_sql_mode",
     {"---------- SQL mode error ----------",
      "SELECT \"ERROR WHEN SQL_MODE = ANSI_QUOTES\""}},
#endif

    // Basic information as keyword/value pairs
    {"hm_mem_usage_overview",
     {"---------- TOTAL ALLOCATED MEMORY AND BUFFER SIZES (internal) "
      "----------",
      "SELECT JSON_OBJECT("
      "   'time', now(),"
      "   'hm_mem_usage_overview', JSON_ARRAYAGG("
      "      JSON_OBJECT('key', KEYWORD, 'val', VALUE))"
      ") AS 'hm_mem_usage_overview'"
      "FROM ("
      "SELECT 'mysql_version' AS KEYWORD, "
      " version() AS VALUE "
      " UNION "
      " SELECT 'total_allocated' AS KEYWORD, "
      "   SUM(current_number_of_bytes_used) AS VALUE "
      "   FROM performance_schema.memory_summary_global_by_event_name "
      " UNION "
      " SELECT variable_name AS KEYWORD, "
      "   variable_value AS VALUE "
      "   FROM performance_schema.global_variables "
      "   WHERE variable_name IN ("
      "     'innodb_buffer_pool_size', "
      "     'join_buffer_size', "
      "     'max_allowed_packet_size', "
      "     'rapid_memory_heap_size')"
      ") T"}},

    // Aggregation per component, see WL#14799.
    // Added group replication and rapid.
    {"hm_mem_usage_per_component",
     {"---------- MEMORY AGGREGATION PER LOGICAL COMPONENT (internal) "
      "----------",
      "WITH "
      "table_components AS ("
      "  SELECT SUBSTRING_INDEX(event_name, '/', 2) AS component,"
      "      SUM(current_count_used) AS current_count,"
      "      SUM(current_number_of_bytes_used) AS current_alloc,"
      "      SUM(high_number_of_bytes_used) AS max_alloc,"
      "      SUM(low_number_of_bytes_used) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE (event_name LIKE '%memory/archive%'"
      "      OR event_name LIKE '%memory/blackhole%'"
      "      OR event_name LIKE '%memory/client%'"
      "      OR event_name LIKE '%memory/component_sys_vars%'"
      "      OR event_name LIKE '%memory/csv%'"
      "      OR event_name LIKE '%memory/health%'"
      "      OR event_name LIKE '%memory/innodb%'"
      "      OR event_name LIKE '%memory/memory%'"
      "      OR event_name LIKE '%memory/myisam%'"
      "      OR event_name LIKE '%memory/myisammrg%'"
      "      OR event_name LIKE '%memory/mysqld_openssl%'"
      "      OR event_name LIKE '%memory/mysqlx%'"
      "      OR event_name LIKE '%memory/mysys%'"
      "      OR event_name LIKE '%memory/performance_schema%'"
      "      OR event_name LIKE '%memory/refcache%'"
      "      OR event_name LIKE '%memory/temptable%'"
      "      OR event_name LIKE '%memory/vio%')"
      "     AND event_name NOT LIKE '%memory/innodb/buf_buf_pool%'"
      "  GROUP BY SUBSTRING_INDEX(event_name, '/', 2)"
      "),"
      ""
      "table_innodb_buffer_pool AS ("
      "  SELECT 'memory/innodb: buffer pool' AS component,"
      "      SUM(current_count_used) AS current_count,"
      "      SUM(current_number_of_bytes_used) AS current_alloc,"
      "      SUM(high_number_of_bytes_used) AS max_alloc,"
      "      SUM(low_number_of_bytes_used) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ('memory/innodb/buf_buf_pool')"
      "),"
      ""
      "table_gr_conflict_detection_db AS ("
      "  SELECT 'memory/group_rpl:conflict_detection_database' AS component,"
      "      IFNULL(SUM(current_count_used), 0) AS current_count,"
      "      IFNULL(SUM(current_number_of_bytes_used), 0) AS current_alloc,"
      "      IFNULL(SUM(high_number_of_bytes_used), 0) AS max_alloc,"
      "      IFNULL(SUM(low_number_of_bytes_used), 0) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ("
      "      'memory/group_rpl/certification_data',"
      "      'memory/group_rpl/certification_data_gc',"
      "      'memory/group_rpl/certification_info'"
      "  )"
      "),"
      ""
      "table_gr_message_buffers AS ("
      "  SELECT 'memory/group_rpl:message_buffers' AS component,"
      "      IFNULL(SUM(current_count_used), 0) AS current_count,"
      "      IFNULL(SUM(current_number_of_bytes_used), 0) AS current_alloc,"
      "      IFNULL(SUM(high_number_of_bytes_used), 0) AS max_alloc,"
      "      IFNULL(SUM(low_number_of_bytes_used), 0) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ("
      "      'memory/group_rpl/Gcs_message_data::m_buffer',"
      "      'memory/group_rpl/transaction_data'"
      "  )"
      "),"
      ""
      "table_gr_xcom_message_cache AS ("
      "  SELECT 'memory/group_rpl:gcs_xcom_message_cache' AS component,"
      "      IFNULL(SUM(current_count_used), 0) AS current_count,"
      "      IFNULL(SUM(current_number_of_bytes_used), 0) AS current_alloc,"
      "      IFNULL(SUM(high_number_of_bytes_used), 0) AS max_alloc,"
      "      IFNULL(SUM(low_number_of_bytes_used), 0) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ('memory/group_rpl/GCS_XCom::xcom_cache')"
      "),"
      ""
      "table_gr_write_set AS ("
      "  SELECT 'memory/group_rpl:write_set' AS component,"
      "      SUM(current_count_used) AS current_count,"
      "      SUM(current_number_of_bytes_used) AS current_alloc,"
      "      SUM(high_number_of_bytes_used) AS max_alloc,"
      "      SUM(low_number_of_bytes_used) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ("
      "      'memory/sql/write_set_extraction',"
      "      'memory/group_rpl/write_set_encoded'"
      "  )"
      "),"
      ""
      "table_rapid_heap_usage AS ("
      "   SELECT 'memory/rapid_heap_usage' AS component, "
      "      IF( ISNULL(SUM(variable_value)), 0, 'N/A') AS current_count, "
      "      IFNULL(SUM(variable_value), 0) AS current_alloc,"
      "      IF( ISNULL(SUM(variable_value)), 0, 'N/A') AS max_alloc,"
      "      IF( ISNULL(SUM(variable_value)), 0, 'N/A') AS min_alloc"
      " FROM performance_schema.global_variables "
      " WHERE variable_name = 'rapid_heap_usage' "
      "),"
      ""
      "table_sql_session AS ("
      "  SELECT CONCAT(SUBSTRING_INDEX(event_name, '/', 2), ':session') AS "
      "component,"
      "      SUM(current_count_used) AS current_count,"
      "      SUM(current_number_of_bytes_used) AS current_alloc,"
      "      SUM(high_number_of_bytes_used) AS max_alloc,"
      "      SUM(low_number_of_bytes_used) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name IN ("
      "      'memory/sql/THD::main_mem_root',"
      "      'memory/sql/Prepared_statement::main_mem_root',"
      "      'memory/sql/THD::sp_cache',"
      "      'memory/sql/sp_head::execute_mem_root',"
      "      'memory/sql/sp_head::call_mem_root',"
      "      'memory/sql/test_quick_select',"
      "      'memory/sql/Partition::prune_exec',"
      "      'memory/sql/Blob_mem_storage::storage',"
      "      'memory/sql/Filesort_info::merge',"
      "      'memory/sql/Filesort_info::record_pointers',"
      "      'memory/sql/Filesort_buffer::sort_keys',"
      "      'memory/sql/my_bitmap_map',"
      "      'memory/sql/QUICK_RANGE_SELECT::mrr_buf_desc',"
      "      'memory/sql/Gis_read_stream::err_msg',"
      "      'memory/sql/Geometry::ptr_and_wkb_data',"
      "      'memory/sql/READ_INFO',"
      "      'memory/sql/TABLE::sort_io_cache',"
      "      'memory/sql/Unique::sort_buffer',"
      "      'memory/sql/Unique::merge_buffer',"
      "      'memory/sql/bison_stack',"
      "      'memory/sql/hash_join',"
      "      'memory/sql/String::value',"
      "      'memory/sql/Row_data_memory::memory',"
      "      'memory/sql/MYSQL_LOCK',"
      "      'memory/sql/NET::Buff',"
      "      'memory/sql/dd::infrastructure',"
      "      'memory/sql/dd::objects',"
      "      'memory/sql/dd::String_type',"
      "      'memory/sql/MDL_context::acquire_locks'"
      "    )"
      "  GROUP BY CONCAT(SUBSTRING_INDEX(event_name, '/', 2), ':session')"
      "),"
      ""
      "table_sql_other AS ("
      "  SELECT SUBSTRING_INDEX(event_name, '/', 2) AS component,"
      "      SUM(current_count_used) AS current_count,"
      "      SUM(current_number_of_bytes_used) AS current_alloc,"
      "      SUM(high_number_of_bytes_used) AS max_alloc,"
      "      SUM(low_number_of_bytes_used) AS min_alloc"
      "  FROM performance_schema.memory_summary_global_by_event_name"
      "  WHERE event_name LIKE '%memory/sql/%' AND event_name NOT IN ("
      "      'memory/sql/THD::main_mem_root',"
      "      'memory/sql/Prepared_statement::main_mem_root',"
      "      'memory/sql/THD::sp_cache',"
      "      'memory/sql/sp_head::execute_mem_root',"
      "      'memory/sql/sp_head::call_mem_root',"
      "      'memory/sql/test_quick_select',"
      "      'memory/sql/Partition::prune_exec',"
      "      'memory/sql/Blob_mem_storage::storage',"
      "      'memory/sql/Filesort_info::merge',"
      "      'memory/sql/Filesort_info::record_pointers',"
      "      'memory/sql/Filesort_buffer::sort_keys',"
      "      'memory/sql/my_bitmap_map',"
      "      'memory/sql/QUICK_RANGE_SELECT::mrr_buf_desc',"
      "      'memory/sql/Gis_read_stream::err_msg',"
      "      'memory/sql/Geometry::ptr_and_wkb_data',"
      "      'memory/sql/READ_INFO',"
      "      'memory/sql/TABLE::sort_io_cache',"
      "      'memory/sql/Unique::sort_buffer',"
      "      'memory/sql/Unique::merge_buffer',"
      "      'memory/sql/bison_stack',"
      "      'memory/sql/hash_join',"
      "      'memory/sql/String::value',"
      "      'memory/sql/Row_data_memory::memory',"
      "      'memory/sql/MYSQL_LOCK',"
      "      'memory/sql/NET::Buff',"
      "      'memory/sql/dd::infrastructure',"
      "      'memory/sql/dd::objects',"
      "      'memory/sql/dd::String_type',"
      "      'memory/sql/MDL_context::acquire_locks'"
      "    )"
      "  GROUP BY SUBSTRING_INDEX(event_name, '/', 2)"
      "),"
      ""
      "table_all_components AS"
      "("
      "  (SELECT * FROM table_components)"
      "  UNION"
      "  (SELECT * FROM table_innodb_buffer_pool)"
      "  UNION"
      "  (SELECT * FROM table_sql_session)"
      "  UNION"
      "  (SELECT * FROM table_sql_other)"
      "  UNION"
      "  (SELECT * FROM table_gr_conflict_detection_db)"
      "  UNION"
      "  (SELECT * FROM table_gr_message_buffers)"
      "  UNION"
      "  (SELECT * FROM table_gr_xcom_message_cache)"
      "  UNION"
      "  (SELECT * FROM table_gr_write_set)"
      "  UNION"
      "  (SELECT * FROM table_rapid_heap_usage)"
      ")"
      "SELECT JSON_OBJECT("
      "   'time', now(),"
      "   'hm_mem_usage_per_component', JSON_ARRAYAGG(JSON_OBJECT("
      "      'component', component,"
      "      'current_count', current_count,"
      "      'current_alloc', current_alloc,"
      "      'max_alloc', max_alloc,"
      "      'min_alloc',min_alloc))) AS 'hm_mem_usage_per_component'"
      "FROM table_all_components ORDER BY component"}},

    // Processlist details, ordered by memory usage
    {"hm_processlist_details_by_current_mem",
     {"---------- PROCESSLIST DETAILS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_processlist_details_by_current_mem', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'thd_id', thd_id, "
      "              'conn_id', conn_id, "
      "              'user', user, "
      "              'db', db, "
      "              'command', command, "
      "              'state', state, "
      "              'time', time, "
      "              'current_statement', "
      "sys.format_statement(STATEMENT_DIGEST_TEXT(current_statement)), "
      "              'execution_engine', execution_engine, "
      "              'statement_latency', statement_latency, "
      "              'progress', progress, "
      "              'lock_latency', lock_latency, "
      "              'cpu_latency', cpu_latency, "
      "              'rows_examined', rows_examined, "
      "              'rows_sent', rows_sent, "
      "              'rows_affected', rows_affected, "
      "              'tmp_tables', tmp_tables, "
      "              'tmp_disk_tables', tmp_disk_tables, "
      "              'full_scan', full_scan, "
      "              'last_statement', "
      "sys.format_statement(STATEMENT_DIGEST_TEXT(last_statement)), "
      "              'last_statement_latency', last_statement_latency, "
      "              'current_memory', current_memory, "
      "              'last_wait', last_wait, "
      "              'last_wait_latency', last_wait_latency, "
      "              'source', source, "
      "              'trx_latency', trx_latency, "
      "              'trx_state', trx_state, "
      "              'trx_autocommit', trx_autocommit, "
      "              'pid', pid, "
      "              'program_name', program_name "
      "            ))) AS 'hm_processlist_details_by_current_mem' "
      "FROM (SELECT * FROM sys.x$processlist "
      "      ORDER BY current_memory DESC LIMIT 20) T"}},

    // Stats for statement digests, ordered by statement memory
    {"hm_statement_stats_by_total_mem",
     {"---------- STATEMENT STATS PER DIGESTS ORDERED BY TOTAL MEMORY "
      "(internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_statement_stats_by_total_mem', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'query', sys.format_statement(query), "
      "              'db', db, "
      "              'full_scan', full_scan, "
      "              'exec_count', exec_count, "
      "              'exec_secondary_count', exec_secondary_count, "
      "              'err_count', err_count, "
      "              'warn_count', warn_count, "
      "              'total_latency', total_latency, "
      "              'max_latency', max_latency, "
      "              'avg_latency', avg_latency, "
      "              'lock_latency', lock_latency, "
      "              'cpu_latency', cpu_latency, "
      "              'rows_sent', rows_sent, "
      "              'rows_sent_avg', rows_sent_avg, "
      "              'rows_examined', rows_examined, "
      "              'rows_examined_avg', rows_examined_avg, "
      "              'rows_affected', rows_affected, "
      "              'rows_affected_avg', rows_affected_avg, "
      "              'tmp_tables', tmp_tables, "
      "              'tmp_disk_tables', tmp_disk_tables, "
      "              'rows_sorted', rows_sorted, "
      "              'sort_merge_passes', sort_merge_passes, "
      "              'max_controlled_memory', max_controlled_memory, "
      "              'max_total_memory', max_total_memory, "
      "              'digest', digest, "
      "              'first_seen', first_seen, "
      "              'last_seen', last_seen"
      "            ))) AS 'hm_statement_stats_by_total_mem' "
      "FROM (SELECT * FROM sys.x$statement_analysis "
      "        ORDER BY max_total_memory DESC LIMIT 20) T"}},

    // Events allocating the most memory
    {"hm_events_allocating_most_mem",
     {"---------- EVENTS ALLOCATING MOST MEMORY (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_events_allocating_most_mem', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'event_name', event_name, "
      "              'count_alloc', count_alloc, "
      "              'count_free', count_free, "
      "              'curr_count_used', current_count_used, "
      "              'high_count_used', high_count_used, "
      "              'curr_number_of_bytes_used', "
      "current_number_of_bytes_used, "
      "              'high_number_of_bytes_used', high_number_of_bytes_used "
      "            ))) AS 'hm_events_allocating_most_mem' "
      "FROM (SELECT * FROM "
      "performance_schema.memory_summary_global_by_event_name"
      "        ORDER BY current_number_of_bytes_used DESC LIMIT 20) T"}},

    // Buffer pool statistics
    {"hm_innodb_buffer_pool_stats",
     {"---------- BUFFER POOL STATISTICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_innodb_buffer_pool_stats', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'POOL_ID', POOL_ID, "
      "              'POOL_SIZE', POOL_SIZE, "
      "              'FREE_BUFFERS', FREE_BUFFERS, "
      "              'DATABASE_PAGES', DATABASE_PAGES, "
      "              'OLD_DATABASE_PAGES', OLD_DATABASE_PAGES, "
      "              'MODIFIED_DATABASE_PAGES', MODIFIED_DATABASE_PAGES, "
      "              'PENDING_DECOMPRESS', PENDING_DECOMPRESS, "
      "              'PENDING_READS', PENDING_READS, "
      "              'PENDING_FLUSH_LRU', PENDING_FLUSH_LRU, "
      "              'PENDING_FLUSH_LIST', PENDING_FLUSH_LIST, "
      "              'PAGES_MADE_YOUNG', PAGES_MADE_YOUNG, "
      "              'PAGES_NOT_MADE_YOUNG', PAGES_NOT_MADE_YOUNG, "
      "              'PAGES_MADE_YOUNG_RATE', PAGES_MADE_YOUNG_RATE, "
      "              'PAGES_MADE_NOT_YOUNG_RATE', PAGES_MADE_NOT_YOUNG_RATE, "
      "              'NUMBER_PAGES_READ', NUMBER_PAGES_READ, "
      "              'NUMBER_PAGES_CREATED', NUMBER_PAGES_CREATED, "
      "              'NUMBER_PAGES_WRITTEN', NUMBER_PAGES_WRITTEN, "
      "              'PAGES_READ_RATE', PAGES_READ_RATE, "
      "              'PAGES_CREATE_RATE', PAGES_CREATE_RATE, "
      "              'PAGES_WRITTEN_RATE', PAGES_WRITTEN_RATE, "
      "              'NUMBER_PAGES_GET', NUMBER_PAGES_GET, "
      "              'HIT_RATE', HIT_RATE, "
      "              'YOUNG_MAKE_PER_THOUSAND_GETS', "
      "YOUNG_MAKE_PER_THOUSAND_GETS, "
      "              'NOT_YOUNG_MAKE_PER_THOUSAND_GETS', "
      "NOT_YOUNG_MAKE_PER_THOUSAND_GETS, "
      "              'NUMBER_PAGES_READ_AHEAD', NUMBER_PAGES_READ_AHEAD, "
      "              'NUMBER_READ_AHEAD_EVICTED', NUMBER_READ_AHEAD_EVICTED, "
      "              'READ_AHEAD_RATE', READ_AHEAD_RATE, "
      "              'READ_AHEAD_EVICTED_RATE', READ_AHEAD_EVICTED_RATE, "
      "              'LRU_IO_TOTAL', LRU_IO_TOTAL, "
      "              'LRU_IO_CURRENT', LRU_IO_CURRENT, "
      "              'UNCOMPRESS_TOTAL', UNCOMPRESS_TOTAL, "
      "              'UNCOMPRESS_CURRENT', UNCOMPRESS_CURRENT "
      "            ))) AS 'hm_innodb_buffer_pool_stats' "
      "FROM information_schema.INNODB_BUFFER_POOL_STATS"}},

    // The most relevant global status
    {"hm_key_global_status",
     {"---------- Most relevant global status (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_key_global_status', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', VARIABLE_NAME, "
      "              'val', VARIABLE_VALUE "
      "            ))) AS 'hm_key_global_status' "
      "FROM (SELECT * FROM "
      "        performance_schema.global_status"
      "        WHERE variable_name IN ("
      "          'Aborted_clients', 'Aborted_connects', "
      "          'Binlog_cache_disk_use', 'Max_execution_time_exceeded', "
      "          'Max_used_connections', 'Max_used_connections_time', "
      "          'Queries', 'Replica_open_temp_tables', 'Slow_launch_threads', "
      "          'Slow_queries', 'Sort_scan') "
      "        OR variable_name LIKE 'Connection%' "
      "        OR variable_name LIKE 'Created%' "
      "        OR variable_name LIKE 'Gr%' "
      "        OR variable_name LIKE 'Innodb%' "
      "        OR variable_name LIKE 'Open%' "
      "        OR variable_name LIKE 'Rpl%' "
      "        OR variable_name LIKE 'Threads%' "
      ") T"}},

    // Connection control delays
    {"hm_connection_ctrl",
     {"---------- CONNECTION CONTROL (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_connection_ctrl', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'user', USERHOST, 'failed', FAILED_ATTEMPTS"
      "            ))) AS 'hm_connection_ctrl' "
      "FROM (SELECT * FROM "
      "        INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS "
      "        ORDER BY FAILED_ATTEMPTS DESC LIMIT 20) T"}},

    // Host cache, connection errors
    {"hm_connection_host_cache",
     {"---------- HOST CACHE CONNECTION ERRORS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(), "
      "         'hm_connection_host_cache', JSON_ARRAYAGG( "
      "            JSON_OBJECT("
      "               'IP', IP, "
      "               'HOST', HOST, "
      "               'HOST_VALIDATED', HOST_VALIDATED, "
      "               'SUM_CONNECT_ERRORS', SUM_CONNECT_ERRORS, "
      "               'COUNT_HOST_BLOCKED_ERRORS', COUNT_HOST_BLOCKED_ERRORS, "
      "               'COUNT_NAMEINFO_TRANSIENT_ERRORS', "
      "               COUNT_NAMEINFO_TRANSIENT_ERRORS, "
      "               'COUNT_NAMEINFO_PERMANENT_ERRORS', "
      "               COUNT_NAMEINFO_PERMANENT_ERRORS, "
      "               'COUNT_FORMAT_ERRORS', COUNT_FORMAT_ERRORS, "
      "               'COUNT_ADDRINFO_TRANSIENT_ERRORS', "
      "               COUNT_ADDRINFO_TRANSIENT_ERRORS, "
      "               'COUNT_ADDRINFO_PERMANENT_ERRORS', "
      "               COUNT_ADDRINFO_PERMANENT_ERRORS, "
      "               'COUNT_FCRDNS_ERRORS', COUNT_FCRDNS_ERRORS, "
      "               'COUNT_HOST_ACL_ERRORS', COUNT_HOST_ACL_ERRORS, "
      "               'COUNT_NO_AUTH_PLUGIN_ERRORS', "
      "COUNT_NO_AUTH_PLUGIN_ERRORS, "
      "               'COUNT_AUTH_PLUGIN_ERRORS', COUNT_AUTH_PLUGIN_ERRORS, "
      "               'COUNT_HANDSHAKE_ERRORS', COUNT_HANDSHAKE_ERRORS, "
      "               'COUNT_PROXY_USER_ERRORS', COUNT_PROXY_USER_ERRORS, "
      "               'COUNT_PROXY_USER_ACL_ERRORS', "
      "COUNT_PROXY_USER_ACL_ERRORS, "
      "               'COUNT_AUTHENTICATION_ERRORS', "
      "COUNT_AUTHENTICATION_ERRORS, "
      "               'COUNT_SSL_ERRORS', COUNT_SSL_ERRORS, "
      "               'COUNT_MAX_USER_CONNECTIONS_ERRORS', "
      "               COUNT_MAX_USER_CONNECTIONS_ERRORS, "
      "               'COUNT_MAX_USER_CONNECTIONS_PER_HOUR_ERRORS', "
      "               COUNT_MAX_USER_CONNECTIONS_PER_HOUR_ERRORS, "
      "               'COUNT_DEFAULT_DATABASE_ERRORS', "
      "COUNT_DEFAULT_DATABASE_ERRORS, "
      "               'COUNT_INIT_CONNECT_ERRORS', COUNT_INIT_CONNECT_ERRORS, "
      "               'COUNT_LOCAL_ERRORS', COUNT_LOCAL_ERRORS, "
      "               'COUNT_UNKNOWN_ERRORS', COUNT_UNKNOWN_ERRORS, "
      "               'FIRST_SEEN', FIRST_SEEN, "
      "               'LAST_SEEN', LAST_SEEN, "
      "               'FIRST_ERROR_SEEN', FIRST_ERROR_SEEN, "
      "               'LAST_ERROR_SEEN', LAST_ERROR_SEEN "
      "      ))) AS 'hm_connection_host_cache' "
      "FROM (SELECT * FROM "
      "        performance_schema.host_cache"
      "        ORDER BY SUM_CONNECT_ERRORS LIMIT 20) T"}},

    // Error codes emitted
    {"hm_error_codes_emitted",
     {"---------- ERROR CODES EMITTED (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_error_codes_emitted', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'ERROR_NUMBER', ERROR_NUMBER, "
      "              'ERROR_NAME', ERROR_NAME, "
      "              'SQL_STATE', SQL_STATE, "
      "              'SUM_ERROR_RAISED', SUM_ERROR_RAISED, "
      "              'FIRST_SEEN', FIRST_SEEN, "
      "              'LAST_SEEN', LAST_SEEN "
      "            ))) AS 'hm_error_codes_emitted' "
      "FROM (SELECT * FROM "
      "        performance_schema.events_errors_summary_global_by_error "
      "        WHERE SUM_ERROR_RAISED > 0 AND LAST_SEEN IS NOT NULL "
      "        ORDER BY SUM_ERROR_RAISED LIMIT 100) T"}},

    // Innodb metrics
    {"hm_innodb_metrics",
     {"---------- INNODB METRICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_innodb_metrics', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "+               'NAME', NAME, "
      "+               'COUNT', COUNT, "
      "+               'MAX_COUNT', MAX_COUNT, "
      "+               'MIN_COUNT', MIN_COUNT, "
      "+               'AVG_COUNT', AVG_COUNT "
      "            ))) AS 'hm_innodb_metrics' "
      "FROM (SELECT * FROM "
      "        INFORMATION_SCHEMA.INNODB_METRICS"
      "        WHERE STATUS = 'enabled' ORDER BY NAME) T"}},

    // Innodb temporary tablespaces
    {"hm_innodb_temp_tablespaces",
     {"---------- INNODB TEMPORARY TABLESPACES (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_innodb_temp_tablespaces', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'ID', ID, "
      "               'SPACE', SPACE, "
      "               'PATH', PATH, "
      "               'SIZE', SIZE, "
      "               'STATE', STATE, "
      "               'PURPOSE', PURPOSE"
      "            ))) AS 'hm_innodb_temp_tablespaces' "
      "FROM (SELECT * FROM "
      "        INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES"
      "        ORDER BY ID ASC LIMIT 20) T"}},

    // Innodb temporary table info
    {"hm_innodb_temp_table_info",
     {"---------- INNODB TEMPORARY TABLES (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_innodb_temp_table_info', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'TABLE_ID', TABLE_ID, "
      "               'N_COLS', N_COLS, "
      "               'SPACE', SPACE "
      "            ))) AS 'hm_innodb_temp_table_info' "
      "FROM (SELECT * FROM "
      "        INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO "
      "        ORDER BY TABLE_ID ASC LIMIT 20) T"}},

    // Innodb lock waits
    {"hm_innodb_lock_waits",
     {"---------- INNODB LOCK WAITS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_innodb_lock_waits', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'wait_started', wait_started, "
      "               'wait_age', wait_age, "
      "               'wait_age_secs', wait_age_secs, "
      "               'locked_table', locked_table, "
      "               'locked_table_schema', locked_table_schema, "
      "               'locked_table_name', locked_table_name, "
      "               'locked_table_partition', locked_table_partition, "
      "               'locked_table_subpartition', locked_table_subpartition, "
      "               'locked_index', locked_index, "
      "               'locked_type', locked_type, "
      "               'waiting_trx_id', waiting_trx_id, "
      "               'waiting_trx_started', waiting_trx_started, "
      "               'waiting_trx_age', waiting_trx_age, "
      "               'waiting_trx_rows_locked', waiting_trx_rows_locked, "
      "               'waiting_trx_rows_modified', waiting_trx_rows_modified, "
      "               'waiting_pid', waiting_pid, "
      "               'waiting_query', waiting_query, "
      "               'waiting_lock_id', waiting_lock_id, "
      "               'waiting_lock_mode', waiting_lock_mode, "
      "               'blocking_trx_id', blocking_trx_id, "
      "               'blocking_pid', blocking_pid, "
      "               'blocking_query', blocking_query, "
      "               'blocking_lock_id', blocking_lock_id, "
      "               'blocking_lock_mode', blocking_lock_mode, "
      "               'blocking_trx_started', blocking_trx_started, "
      "               'blocking_trx_age', blocking_trx_age, "
      "               'blocking_trx_rows_locked', blocking_trx_rows_locked, "
      "               'blocking_trx_rows_modified', "
      "blocking_trx_rows_modified, "
      "               'sql_kill_blocking_query', sql_kill_blocking_query, "
      "               'sql_kill_blocking_connection', "
      "sql_kill_blocking_connection"
      "            ))) AS 'hm_innodb_lock_waits' "
      "FROM (SELECT * FROM "
      "        sys.x$innodb_lock_waits LIMIT 20) T"}},

    // Thread pool group state
    {"hm_tp_group_state",
     {"---------- THREAD POOL GROUP STATE (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tp_group_state', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'TP_GROUP_ID', TP_GROUP_ID, "
      "               'CONSUMER_THREADS', CONSUMER_THREADS, "
      "               'RESERVE_THREADS', RESERVE_THREADS, "
      "               'CONNECT_THREAD_COUNT', CONNECT_THREAD_COUNT, "
      "               'CONNECTION_COUNT', CONNECTION_COUNT, "
      "               'QUEUED_QUERIES', QUEUED_QUERIES, "
      "               'QUEUED_TRANSACTIONS', QUEUED_TRANSACTIONS, "
      "               'STALL_LIMIT', STALL_LIMIT, "
      "               'PRIO_KICKUP_TIMER', PRIO_KICKUP_TIMER, "
      "               'ALGORITHM', ALGORITHM, "
      "               'THREAD_COUNT', THREAD_COUNT, "
      "               'ACTIVE_THREAD_COUNT', ACTIVE_THREAD_COUNT, "
      "               'STALLED_THREAD_COUNT', STALLED_THREAD_COUNT, "
      "               'WAITING_THREAD_NUMBER', WAITING_THREAD_NUMBER, "
      "               'OLDEST_QUEUED', OLDEST_QUEUED, "
      "               'MAX_THREAD_IDS_IN_GROUP', MAX_THREAD_IDS_IN_GROUP"
      "            ))) AS 'hm_tp_group_state' "
      "FROM (SELECT * FROM "
      "        performance_schema.tp_thread_group_state "
      "        ORDER BY TP_GROUP_ID DESC LIMIT 33) T"}},

    // Thread group statistics
    {"hm_tp_group_stats",
     {"---------- THREAD POOL GROUP STATISTICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tp_group_stats', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'TP_GROUP_ID', TP_GROUP_ID, "
      "               'CONNECTIONS_STARTED', CONNECTIONS_STARTED, "
      "               'CONNECTIONS_CLOSED', CONNECTIONS_CLOSED, "
      "               'QUERIES_EXECUTED', QUERIES_EXECUTED, "
      "               'QUERIES_QUEUED', QUERIES_QUEUED, "
      "               'THREADS_STARTED', THREADS_STARTED, "
      "               'PRIO_KICKUPS', PRIO_KICKUPS, "
      "               'STALLED_QUERIES_EXECUTED', STALLED_QUERIES_EXECUTED, "
      "               'BECOME_CONSUMER_THREAD', BECOME_CONSUMER_THREAD, "
      "               'BECOME_RESERVE_THREAD', BECOME_RESERVE_THREAD, "
      "               'BECOME_WAITING_THREAD', BECOME_WAITING_THREAD, "
      "               'WAKE_THREAD_STALL_CHECKER', WAKE_THREAD_STALL_CHECKER, "
      "               'SLEEP_WAITS', SLEEP_WAITS, "
      "               'DISK_IO_WAITS', DISK_IO_WAITS, "
      "               'ROW_LOCK_WAITS', ROW_LOCK_WAITS, "
      "               'GLOBAL_LOCK_WAITS', GLOBAL_LOCK_WAITS, "
      "               'META_DATA_LOCK_WAITS', META_DATA_LOCK_WAITS, "
      "               'TABLE_LOCK_WAITS', TABLE_LOCK_WAITS, "
      "               'USER_LOCK_WAITS', USER_LOCK_WAITS, "
      "               'BINLOG_WAITS', BINLOG_WAITS, "
      "               'GROUP_COMMIT_WAITS', GROUP_COMMIT_WAITS, "
      "               'FSYNC_WAITS', FSYNC_WAITS"
      "            ))) AS 'hm_tp_group_stats' "
      "FROM (SELECT * FROM "
      "        performance_schema.tp_thread_group_stats"
      "        ORDER BY TP_GROUP_ID DESC LIMIT 33) T"}},

    // Thread pool thread state
    {"hm_tp_thread_state",
     {"---------- THREAD POOL THREAD STATE (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tp_thread_state', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'TP_GROUP_ID', TP_GROUP_ID, "
      "               'TP_THREAD_NUMBER', TP_THREAD_NUMBER, "
      "               'PROCESS_COUNT', PROCESS_COUNT, "
      "               'WAIT_TYPE', WAIT_TYPE, "
      "               'TP_THREAD_TYPE', TP_THREAD_TYPE, "
      "               'THREAD_ID', THREAD_ID"
      "            ))) AS 'hm_tp_thread_state' "
      "FROM (SELECT * FROM "
      "        performance_schema.tp_thread_state"
      "        ORDER BY TP_GROUP_ID DESC LIMIT 33) T"}},

    // Oldest thread pool connections
    {"hm_tp_connections",
     {"---------- OLDEST THREAD POOL CONNECTION STATISTICS (internal) "
      "----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tp_connections', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'CONNECTION_ID', CONNECTION_ID, "
      "               'TP_GROUP_ID', TP_GROUP_ID, "
      "               'TP_PROCESSING_THREAD_NUMBER', "
      "TP_PROCESSING_THREAD_NUMBER, "
      "               'THREAD_ID', THREAD_ID, "
      "               'STATE', STATE, "
      "               'ACTIVE_FLAG', ACTIVE_FLAG, "
      "               'KILLED_STATE', KILLED_STATE, "
      "               'CLEANUP_STATE', CLEANUP_STATE, "
      "               'TIME_OF_LAST_EVENT_COMPLETION', "
      "TIME_OF_LAST_EVENT_COMPLETION, "
      "               'TIME_OF_EXPIRY', TIME_OF_EXPIRY, "
      "               'TIME_OF_ADD', TIME_OF_ADD, "
      "               'TIME_OF_POP', TIME_OF_POP, "
      "               'TIME_OF_ARM', TIME_OF_ARM, "
      "               'CONNECT_HANDLER_INDEX', CONNECT_HANDLER_INDEX, "
      "               'TYPE', TYPE, "
      "               'DIRECT_QUERY_EVENTS', DIRECT_QUERY_EVENTS, "
      "               'QUEUED_QUERY_EVENTS', QUEUED_QUERY_EVENTS, "
      "               'TIME_OF_EVENT_ARRIVAL', TIME_OF_EVENT_ARRIVAL, "
      "               'MANAGEMENT_TIME', MANAGEMENT_TIME"
      "            ))) AS 'hm_tp_connections' "
      "FROM (SELECT * FROM "
      "        performance_schema.tp_connections "
      "        ORDER BY TIME_OF_ADD ASC LIMIT 20) T"}},

    // Memory usage by temporary tables
    {"hm_tmp_table_space",
     {"---------- MEMORY USED BY TEMPORARY TABLES (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tmp_table_space', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'event', EVENT_NAME, "
      "               'sum_alloc', SUM_NUMBER_OF_BYTES_ALLOC, "
      "               'sum_free', SUM_NUMBER_OF_BYTES_FREE, "
      "               'current', CURRENT_NUMBER_OF_BYTES_USED, "
      "               'high', HIGH_NUMBER_OF_BYTES_USED "
      "            ))) AS 'hm_tmp_table_space' "
      "FROM (SELECT * FROM "
      "        performance_schema.memory_summary_global_by_event_name "
      "        WHERE EVENT_NAME = 'memory/temptable/physical_ram' "
      "        OR EVENT_NAME = 'memory/temptable/physical_disk' ) T"}},

    // Temporary table count
    {"hm_tmp_table_count",
     {"---------- TEMPORARY TABLE COUNT (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_tmp_table_count', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', VARIABLE_NAME, "
      "              'val', VARIABLE_VALUE "
      "            ))) AS 'hm_tmp_table_count' "
      "FROM (SELECT * FROM "
      "        performance_schema.global_status "
      "        WHERE variable_name LIKE 'Created_tmp%tables' "
      "      UNION "
      "      SELECT 'Created_user_tmp_tables' AS VARIABLE_NAME, "
      "             COUNT(*) AS VARIABLE_VALUE "
      "      FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO) T"}},

    // Full table scans
    {"hm_full_table_scans",
     {"---------- FULL TABLE SCANS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_full_table_scans', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'query', sys.format_statement(query), "
      "               'exec_count', exec_count, "
      "               'total_latency', total_latency, "
      "               'rows_examined_avg', rows_examined_avg, "
      "               'first_seen', first_seen, "
      "               'last_seen', last_seen "
      "            ))) AS 'hm_full_table_scans' "
      "FROM (SELECT * FROM "
      "        sys.x$statements_with_full_table_scans "
      "        ORDER BY total_latency DESC LIMIT 20) T"}},

    // Long lasting transactions
    {"hm_long_lasting_trx",
     {"---------- LONG LASTING TRANSACTIONS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_long_lasting_trx', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'thread_id', thread_id, "
      "               'connection_id', connection_id, "
      "               'command', command, "
      "               'duration', duration, "
      "               'latest_statement', latest_statement "
      "            ))) AS 'hm_long_lasting_trx' "
      "FROM (SELECT thread_id, thr.processlist_id AS connection_id, "
      "       concat(PROCESSLIST_USER,'@',PROCESSLIST_HOST) as user, "
      "       command, FORMAT_PICO_TIME(trx.timer_wait) AS duration, "
      "       sys.format_statement(STATEMENT_DIGEST_TEXT(current_statement)) AS"
      "       latest_statement "
      " FROM performance_schema.events_transactions_current trx "
      "      INNER JOIN performance_schema.threads thr USING (thread_id) "
      "      LEFT JOIN sys.x$processlist p ON p.thd_id = thread_id "
      "      WHERE thr.processlist_id IS NOT NULL "
      "      AND PROCESSLIST_USER IS NOT NULL "
      "      AND trx.state = 'ACTIVE' "
      "      GROUP BY thread_id, timer_wait "
      "      ORDER BY timer_wait DESC LIMIT 20) T"}},

    // Replication group members
    {"hm_rpl_group_members",
     {"---------- REPLICATION GROUP MEMBERS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_rpl_group_members', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'CHANNEL_NAME', "
      "               CHANNEL_NAME, "
      "               'MEMBER_ID', "
      "               MEMBER_ID, "
      "               'MEMBER_HOST', "
      "               MEMBER_HOST, "
      "               'MEMBER_PORT', "
      "               MEMBER_PORT, "
      "               'MEMBER_STATE', "
      "               MEMBER_STATE, "
      "               'MEMBER_ROLE', "
      "               MEMBER_ROLE, "
      "               'MEMBER_VERSION', "
      "               MEMBER_VERSION, "
      "               'MEMBER_COMMUNICATION_STACK', "
      "               MEMBER_COMMUNICATION_STACK"
      "            ))) AS 'hm_rpl_group_members' "
      "FROM (SELECT * FROM "
      "        performance_schema.replication_group_members LIMIT 20) T"}},

    // Replication group member statistics
    {"hm_rpl_group_member_stats",
     {"---------- REPLICATION GROUP MEMBER STATISTICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_rpl_group_member_stats', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'CHANNEL_NAME', "
      "               CHANNEL_NAME, "
      "               'VIEW_ID', "
      "               VIEW_ID, "
      "               'MEMBER_ID', "
      "               MEMBER_ID, "
      "               'COUNT_TRANSACTIONS_IN_QUEUE', "
      "               COUNT_TRANSACTIONS_IN_QUEUE, "
      "               'COUNT_TRANSACTIONS_CHECKED', "
      "               COUNT_TRANSACTIONS_CHECKED, "
      "               'COUNT_CONFLICTS_DETECTED', "
      "               COUNT_CONFLICTS_DETECTED, "
      "               'COUNT_TRANSACTIONS_ROWS_VALIDATING', "
      "               COUNT_TRANSACTIONS_ROWS_VALIDATING, "
      "               'TRANSACTIONS_COMMITTED_ALL_MEMBERS', "
      "               TRANSACTIONS_COMMITTED_ALL_MEMBERS, "
      "               'LAST_CONFLICT_FREE_TRANSACTION', "
      "               LAST_CONFLICT_FREE_TRANSACTION, "
      "               'COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE', "
      "               COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE, "
      "               'COUNT_TRANSACTIONS_REMOTE_APPLIED', "
      "               COUNT_TRANSACTIONS_REMOTE_APPLIED, "
      "               'COUNT_TRANSACTIONS_LOCAL_PROPOSED', "
      "               COUNT_TRANSACTIONS_LOCAL_PROPOSED, "
      "               'COUNT_TRANSACTIONS_LOCAL_ROLLBACK', "
      "               COUNT_TRANSACTIONS_LOCAL_ROLLBACK"
      "            ))) AS 'hm_rpl_group_member_stats' "
      "FROM (SELECT * FROM "
      "        performance_schema.replication_group_member_stats LIMIT 20) T"}},

    // Replication applier status
    {"hm_rpl_applier_status",
     {"---------- REPLICATION APPLIER STATUS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_rpl_applier_status', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'CHANNEL_NAME', "
      "               CHANNEL_NAME, "
      "               'SERVICE_STATE', "
      "               SERVICE_STATE, "
      "               'REMAINING_DELAY', "
      "               REMAINING_DELAY, "
      "               'COUNT_TRANSACTIONS_RETRIES', "
      "               COUNT_TRANSACTIONS_RETRIES"
      "            ))) AS 'hm_rpl_applier_status' "
      "FROM (SELECT * FROM "
      "        performance_schema.replication_applier_status LIMIT 20) T"}},

    // Replication group communication information
    {"hm_rpl_group_communication_information",
     {"---------- REPLICATION GROUP COMMUNICATION INFORMATION (internal) "
      "----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_rpl_group_communication_information', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'MEMBER_FAILURE_SUSPICIONS_COUNT', "
      "MEMBER_FAILURE_SUSPICIONS_COUNT "
      "            ))) AS 'hm_rpl_group_communication_information' "
      "FROM (SELECT * FROM "
      "        performance_schema.replication_group_communication_information "
      "LIMIT 20) T"}},

    // Binlog compression status
    {"hm_binlog_compression_stats",
     {"---------- BINLOG COMPRESSION STATISTICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_binlog_compression_stats', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "               'LOG_TYPE', "
      "               LOG_TYPE, "
      "               'COMPRESSION_TYPE', "
      "               COMPRESSION_TYPE, "
      "               'TRANSACTION_COUNTER', "
      "               TRANSACTION_COUNTER, "
      "               'COMPRESSED_BYTES_COUNTER', "
      "               COMPRESSED_BYTES_COUNTER, "
      "               'UNCOMPRESSED_BYTES_COUNTER', "
      "               UNCOMPRESSED_BYTES_COUNTER, "
      "               'COMPRESSION_PERCENTAGE', "
      "               COMPRESSION_PERCENTAGE, "
      "               'FIRST_TRANSACTION_ID', "
      "               FIRST_TRANSACTION_ID, "
      "               'FIRST_TRANSACTION_COMPRESSED_BYTES', "
      "               FIRST_TRANSACTION_COMPRESSED_BYTES, "
      "               'FIRST_TRANSACTION_UNCOMPRESSED_BYTES', "
      "               FIRST_TRANSACTION_UNCOMPRESSED_BYTES, "
      "               'FIRST_TRANSACTION_TIMESTAMP', "
      "               FIRST_TRANSACTION_TIMESTAMP, "
      "               'LAST_TRANSACTION_ID', "
      "               LAST_TRANSACTION_ID, "
      "               'LAST_TRANSACTION_COMPRESSED_BYTES', "
      "               LAST_TRANSACTION_COMPRESSED_BYTES, "
      "               'LAST_TRANSACTION_UNCOMPRESSED_BYTES', "
      "               LAST_TRANSACTION_UNCOMPRESSED_BYTES, "
      "               'LAST_TRANSACTION_TIMESTAMP', "
      "               LAST_TRANSACTION_TIMESTAMP"
      "            ))) AS 'hm_binlog_compression_stats' "
      "FROM (SELECT * FROM "
      "        performance_schema.binary_log_transaction_compression_stats "
      "        LIMIT 20) T"}},

    // Key usage
    {"hm_key_usage",
     {"---------- KEY USAGE STATISTICS (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_key_usage', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', KEYWORD, 'val', VALUE "
      "            ))) AS 'hm_key_usage' "
      "FROM (SELECT 'Tables without primary key' AS KEYWORD, COUNT(*) AS VALUE "
      "         FROM INFORMATION_SCHEMA.TABLES "
      "         LEFT OUTER JOIN INFORMATION_SCHEMA.STATISTICS "
      "            ON STATISTICS.TABLE_SCHEMA = TABLES.TABLE_SCHEMA "
      "            AND STATISTICS.TABLE_NAME = TABLES.TABLE_NAME "
      "            AND STATISTICS.INDEX_NAME = 'PRIMARY' "
      "         WHERE STATISTICS.TABLE_NAME IS NULL "
      "         AND TABLES.TABLE_SCHEMA NOT IN ( "
      "            'mysql', "
      "            'information_schema', "
      "            'performance_schema', "
      "            'sys') "
      "         AND TABLES.TABLE_TYPE = 'BASE TABLE' "
      "         UNION "
      "         SELECT 'Tables with foreign keys' AS KEYWORD, COUNT(*) AS "
      "VALUE FROM ( "
      "            SELECT CONSTRAINT_SCHEMA, TABLE_NAME "
      "            FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
      "            WHERE REFERENCED_TABLE_NAME IS NOT NULL "
      "            AND CONSTRAINT_SCHEMA NOT IN ( "
      "               'mysql', "
      "               'information_schema', "
      "               'performance_schema', "
      "               'sys') "
      "            GROUP BY CONSTRAINT_SCHEMA, TABLE_NAME) T1 "
      "          UNION "
      "          SELECT 'Tables with cascading FKs' AS KEYWORD, COUNT(*) AS "
      "VALUE FROM ( "
      "             SELECT CONSTRAINT_SCHEMA, TABLE_NAME "
      "             FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
      "             WHERE (UPDATE_RULE = 'CASCADE' OR DELETE_RULE = 'CASCADE') "
      "             AND CONSTRAINT_SCHEMA NOT IN ( "
      "                'mysql', "
      "                'information_schema', "
      "                'performance_schema', "
      "                'sys') "
      "             GROUP BY CONSTRAINT_SCHEMA, TABLE_NAME) T2"
      ") T"}},

    // Data volume
    {"hm_data_volume",
     {"---------- DATA VOLUME (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_data_volume', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', KEYWORD, 'val', VALUE "
      "            ))) AS 'hm_data_volume' "
      "FROM (SELECT 'Number of schemas' AS KEYWORD, COUNT(*) AS VALUE "
      "            FROM INFORMATION_SCHEMA.SCHEMATA "
      "         UNION "
      "         SELECT 'Number of system tables' AS KEYWORD, COUNT(*) AS VALUE "
      "            FROM INFORMATION_SCHEMA.TABLES "
      "            WHERE TABLE_SCHEMA IN "
      "               ('mysql', 'information_schema', 'performance_schema', "
      "'sys') "
      "         UNION "
      "         SELECT 'Number of user tables' AS KEYWORD, COUNT(*) AS VALUE "
      "            FROM INFORMATION_SCHEMA.TABLES "
      "            WHERE TABLE_SCHEMA NOT IN "
      "               ('mysql', 'information_schema', 'performance_schema', "
      "'sys') "
      "         UNION "
      "         SELECT 'Estimated total data volume' as KEYWORD, "
      "            SUM(AVG_ROW_LENGTH * TABLE_ROWS) AS VALUE "
      "            FROM INFORMATION_SCHEMA.TABLES "
      "         UNION "
      "         SELECT 'Max table rows' as KEYWORD, MAX(TABLE_ROWS) AS VALUE "
      "            FROM INFORMATION_SCHEMA.TABLES"
      ") T"}},

    // Data type usage count
    {"hm_data_type_usage_count",
     {"---------- DATA TYPE USAGE COUNT (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_data_type_usage_count', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', KEYWORD, 'val', VALUE "
      "            ))) AS 'hm_data_type_usage_count' "
      "FROM ( "
      "         SELECT DATA_TYPE AS KEYWORD, COUNT(*) AS VALUE "
      "            FROM INFORMATION_SCHEMA.COLUMNS "
      "               INNER JOIN INFORMATION_SCHEMA.TABLES "
      "               USING (TABLE_SCHEMA, TABLE_NAME) "
      "            WHERE TABLE_TYPE = 'BASE TABLE' AND "
      "                  DATA_TYPE IN "
      "                     ('blob', 'text', 'mediumblob', 'mediumtext', "
      "                      'longblob', 'longtext') AND "
      "                  TABLE_SCHEMA NOT IN "
      "                     ('mysql', 'information_schema', "
      "                      'performance_schema','sys') "
      "         GROUP BY DATA_TYPE "
      "         UNION "
      "         SELECT 'fts' AS KEYWORD, COUNT(*) AS VALUE "
      "            FROM INFORMATION_SCHEMA.STATISTICS "
      "               INNER JOIN INFORMATION_SCHEMA.TABLES "
      "               USING (TABLE_SCHEMA, TABLE_NAME) "
      "            WHERE INDEX_TYPE = 'FULLTEXT' AND "
      "               TABLE_SCHEMA NOT IN "
      "                  ('mysql', 'information_schema', 'performance_schema', "
      "'sys')"
      ") T"}},

    // Less relevant global status
    {"hm_basic_global_status",
     {"---------- Less relevant, basic global status (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_basic_global_status', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', VARIABLE_NAME, "
      "              'val', VARIABLE_VALUE "
      "            ))) AS 'hm_basic_global_status' "
      "FROM (SELECT * FROM "
      "        performance_schema.global_status"
      "        WHERE variable_name NOT IN ("
      "          'Aborted_clients', 'Aborted_connects', "
      "          'Binlog_cache_disk_use', 'Max_execution_time_exceeded', "
      "          'Max_used_connections', 'Max_used_connections_time', "
      "          'Queries', 'Replica_open_temp_tables', 'Slow_launch_threads', "
      "          'Slow_queries', 'Sort_scan') "
      "        AND variable_name NOT LIKE 'Connection%' "
      "        AND variable_name NOT LIKE 'Created%' "
      "        AND variable_name NOT LIKE 'Gr%' "
      "        AND variable_name NOT LIKE 'Innodb%' "
      "        AND variable_name NOT LIKE 'Open%' "
      "        AND variable_name NOT LIKE 'Rpl%' "
      "        AND variable_name NOT LIKE 'Threads%' "
      "        AND variable_name NOT LIKE '%_public_key' "
      "        AND variable_name NOT LIKE 'Ssl_cipher_list' "
      ") T"}},

    // Global variables
    {"hm_global_variables",
     {"---------- GLOBAL VARIABLES (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_global_variables', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'key', VARIABLE_NAME, "
      "              'val', VARIABLE_VALUE "
      "            ))) AS 'hm_global_variables' "
      "FROM (SELECT * FROM "
      "        performance_schema.global_variables"
      "      WHERE VARIABLE_NAME NOT LIKE 'health_monitor%report_definition'"
      ") T"}},

    // Instance metadata
    {"hm_instance_metadata",
     {"---------- INSTANCE METADATA (internal) ----------",
      "SELECT JSON_OBJECT("
      "         'time', now(),"
      "         'hm_instance_metadata', JSON_ARRAYAGG("
      "            JSON_OBJECT("
      "              'name', NAME, "
      "              'key', METADATA_KEY, "
      "              'val', VALUE "
      "            ))) AS 'hm_instance_metadata' "
      "FROM (SELECT * FROM performance_schema.instance_metadata) T"}}};

/**
  Iterate over all queries in a report, execute the queries, and write the
  result set to the desired destination. Do this only if the diagnostic log
  is enabled.
*/
void Report::issue_report() {
  bool error;
  if (!Server::get_log_diagnostic_enable(error)) return;
  log_diag("%s", title.c_str());
  std::for_each(queries.begin(), queries.end(), [&](const Query &q) {
    Result_Set results;
    q.run(results);
    for (auto &row : results) {
      std::stringstream line("");
      for (auto &field : row) {
        const char delim = (&field != &row.back() ? ',' : '\n');
        line << field << delim;
      }
      // Rows can be longer than max log message length, must use fprintf.
      fprintf(stdout, "%s", line.str().c_str());
      fflush(stdout);
    }
  });
  if (!footer.empty()) log_diag("%s", footer.c_str());
}

bool Report_Definition_Manager::settings_changed(const char *cfg) {
  return cfg != m_cfg;
}

bool Report_Definition_Manager::settings_valid(const char *cfg, Document *doc) {
  Document validation_doc;
  if (doc == nullptr) doc = &validation_doc;

  ParseResult const ok = doc->Parse(cfg);
  if (!ok || !doc->IsArray()) return false;

  // Every entry must be an object. Any supplied period must be an integer
  // from 0 through INT_MAX. At least one entry must have a string title
  // and an array of queries.
  bool has_report = false;
  for (const auto &p : doc->GetArray()) {
    if (!p.IsObject()) return false;
    // Report::next and the thread wait interval use signed integers.
    if (p.HasMember("period") &&
        (!p["period"].IsInt() || p["period"].GetInt() < 0)) {
      return false;
    }
    if (p.HasMember("title") && p["title"].IsString() &&
        p.HasMember("queries") && p["queries"].IsArray()) {
      has_report = true;
    }
  }

  return has_report;
}

/**
  If there are new report definitions, the report manager will refresh its
  map of reports.
*/
void Report_Definition_Manager::update_settings(const char *cfg) {
  // Parse the new definition and validate.
  Document doc;
  if (!settings_valid(cfg, &doc)) return;

  Reports new_reports;
  for (auto &p : doc.GetArray()) {
    // A report must have a title.
    if (!(p.HasMember("title") && p["title"].IsString())) continue;
    std::string const title = p["title"].GetString();

    // Default footer is empty.
    std::string footer;
    if (p.HasMember("footer") && p["footer"].IsString())
      footer = p["footer"].GetString();

    // Default period is 0.
    uint period = 0;
    if (p.HasMember("period") && p["period"].IsInt())
      period = static_cast<uint>(p["period"].GetInt());

    // It must have an array of queries.
    if (!p.HasMember("queries") || !p["queries"].IsArray()) continue;

    // Add a report entry.
    if (!new_reports.contains(title))
      new_reports[title] = {title, footer, {}, period, 0};

    // Iterate over all queries.
    for (auto &q : p["queries"].GetArray()) {
      // Default title is empty.
      std::string q_title;
      if (q.HasMember("title") && q["title"].IsString())
        q_title = q["title"].GetString();

      // Default internal is false. If no explicit title, assume internal.
      bool internal = false;
      if (q.HasMember("internal") && q["internal"].IsBool())
        internal = q["internal"].GetBool();
      else if (q_title.empty())
        internal = true;

      // Default query is empty.
      std::string query;
      if (q.HasMember("query") && q["query"].IsString())
        query = (q["query"].GetString());

      // Must have a title or a query.
      if (q_title.empty() && query.empty()) continue;

      // Allow basic Latin letters, digits 0-9, dollar, underscore.
      if (query.find_first_not_of("abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_") !=
          std::string::npos) {
        log_diag("Invalid query name '%s' referred in report definition.",
                 query.c_str());
        continue;
      }

      // If this is an internal query, we will look it up in the map.
      if (internal) {
        if (m_internal_queries.find(query) == m_internal_queries.end())
          continue;
        new_reports[title].queries.push_back(m_internal_queries.at(query));
      } else {
        new_reports[title].queries.emplace_back(q_title,
                                                "SELECT * FROM sys." + query);
      }
    }
  }

  // Finally, update the map and set the member to point to the new definition.
  m_reports.clear();
  m_reports = std::move(new_reports);
  m_cfg = cfg;
}

}  // namespace hm

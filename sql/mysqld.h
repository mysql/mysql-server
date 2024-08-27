/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLD_INCLUDED
#define MYSQLD_INCLUDED

#include "my_config.h"

#include <signal.h>
#include <stdint.h>  // int32_t
#include <sys/types.h>
#include <time.h>
#include <atomic>
#include <string>
#include <vector>

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/dynamic_loader_scheme_file.h>
#include "lex_string.h"
#include "my_command.h"
#include "my_compress.h"
#include "my_getopt.h"
#include "my_hostname.h"  // HOSTNAME_LENGTH
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "my_sqlcommand.h"  // SQLCOM_END
#include "my_sys.h"         // MY_TMPDIR
#include "my_thread.h"      // my_thread_attr_t
#include "mysql/components/services/bits/mysql_cond_bits.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/mysql_rwlock_bits.h"
#include "mysql/components/services/bits/psi_cond_bits.h"
#include "mysql/components/services/bits/psi_file_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/bits/psi_rwlock_bits.h"
#include "mysql/components/services/bits/psi_socket_bits.h"
#include "mysql/components/services/bits/psi_stage_bits.h"
#include "mysql/components/services/bits/psi_statement_bits.h"
#include "mysql/components/services/bits/psi_thread_bits.h"
#include "mysql/status_var.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_com.h"  // SERVER_VERSION_LENGTH
#ifdef _WIN32
#include "sql/nt_servc.h"
#endif  // _WIN32
#include "aggregated_stats.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"  // UUID_LENGTH

class Rpl_global_filter;
class Rpl_acf_configuration_handler;
class Source_IO_monitor;
class THD;
class Time_zone;
struct MEM_ROOT;
struct handlerton;

#if MAX_INDEXES <= 64
typedef Bitmap<64> Key_map; /* Used for finding keys */
#elif MAX_INDEXES > 255
#error "MAX_INDEXES values greater than 255 is not supported."
#else
typedef Bitmap<((MAX_INDEXES + 7) / 8 * 8)> Key_map; /* Used for finding keys */
#endif

/* Bits from testflag */
#define TEST_PRINT_CACHED_TABLES 1
#define TEST_NO_KEY_GROUP 2
#define TEST_MIT_THREAD 4
/*
  TEST_BLOCKING is made obsolete and is not used any
  where in the code base and is retained here so that
  the other bit flag values are not changed.
*/
#define OBSOLETE_TEST_BLOCKING 8
#define TEST_KEEP_TMP_TABLES 16
#define TEST_READCHECK 64 /**< Force use of readcheck */
#define TEST_NO_EXTRA 128
#define TEST_CORE_ON_SIGNAL 256 /**< Give core if signal */
#define TEST_NO_STACKTRACE 512
#define TEST_SIGINT 1024 /**< Allow sigint on threads */
#define TEST_SYNCHRONIZATION          \
  2048 /**< get server to do sleep in \
          some places */
#define TEST_DO_QUICK_LEAK_CHECK       \
  4096 /**< Do Valgrind leak check for \
          each command. */
#define TEST_NO_TEMP_TABLES \
  8192 /**< No temp table engine is loaded, so use dummy costs. */

#define SPECIAL_NO_NEW_FUNC 2  /* Skip new functions */
#define SPECIAL_SKIP_SHOW_DB 4 /* Don't allow 'show db' */
#define SPECIAL_NO_RESOLVE 64  /* Don't use gethostname */
#define SPECIAL_SHORT_LOG_FORMAT 1024

extern bool dynamic_plugins_are_initialized;

/* Function prototypes */

/**
  Signal the server thread for restart.

  @return false if the thread has been successfully signalled for restart
          else true.
*/

bool signal_restart_server();
void kill_mysql(void);
void refresh_status();
void reset_status_by_thd();
bool is_secure_file_path(const char *path);
ulong sql_rnd_with_mutex();

struct System_status_var *get_thd_status_var(THD *thd, bool *aggregated);

#ifndef NDEBUG
void thd_mem_cnt_alloc(THD *thd, size_t size, const char *key_name);
#else
void thd_mem_cnt_alloc(THD *thd, size_t size);
#endif

void thd_mem_cnt_free(THD *thd, size_t size);

// These are needed for unit testing.
void set_remaining_args(int argc, char **argv);
int init_common_variables();
void my_init_signals();
bool gtid_server_init();
void gtid_server_cleanup();
void clean_up_mysqld_mutexes();

enum enum_server_operational_state {
  SERVER_BOOTING,      /* Server is not operational. It is starting */
  SERVER_OPERATING,    /* Server is fully initialized and operating */
  SERVER_SHUTTING_DOWN /* Server is shutting down */
};
enum_server_operational_state get_server_state();

extern bool opt_large_files, server_id_supplied;
extern bool opt_bin_log;
extern bool opt_log_replica_updates;
extern bool opt_log_unsafe_statements;
extern bool opt_general_log, opt_slow_log, opt_general_log_raw;
extern ulonglong log_output_options;
extern bool opt_log_queries_not_using_indexes;
extern ulong opt_log_throttle_queries_not_using_indexes;
extern bool opt_log_slow_extra;
extern bool opt_disable_networking, opt_skip_show_db;
extern bool opt_skip_name_resolve;
extern bool opt_help;
extern bool opt_verbose;
extern MYSQL_PLUGIN_IMPORT std::atomic<int32>
    connection_events_loop_aborted_flag;
extern long opt_upgrade_mode;
extern bool opt_initialize;
extern bool opt_safe_user_create;
extern bool opt_local_infile, opt_myisam_use_mmap;
extern bool opt_replica_compressed_protocol;
extern ulong replica_exec_mode_options;
extern Rpl_global_filter rpl_global_filter;
extern Rpl_acf_configuration_handler *rpl_acf_configuration_handler;
extern Source_IO_monitor *rpl_source_io_monitor;
extern int32_t opt_regexp_time_limit;
extern int32_t opt_regexp_stack_limit;
#ifdef _WIN32
extern bool opt_no_monitor;
#endif  // _WIN32
extern bool opt_debugging;
extern bool opt_validate_config;

enum enum_replica_type_conversions {
  REPLICA_TYPE_CONVERSIONS_ALL_LOSSY,
  REPLICA_TYPE_CONVERSIONS_ALL_NON_LOSSY,
  REPLICA_TYPE_CONVERSIONS_ALL_UNSIGNED,
  REPLICA_TYPE_CONVERSIONS_ALL_SIGNED
};
extern ulonglong replica_type_conversions_options;

extern bool read_only, opt_readonly;
extern bool super_read_only, opt_super_readonly;
extern bool lower_case_file_system;

extern bool opt_require_secure_transport;

extern bool opt_replica_preserve_commit_order;

#ifndef NDEBUG
extern uint replica_rows_last_search_algorithm_used;
#endif
extern ulong mts_parallel_option;
#ifdef _WIN32
extern bool opt_enable_named_pipe;
extern char *named_pipe_full_access_group;
extern bool opt_enable_shared_memory;
extern mysql_rwlock_t LOCK_named_pipe_full_access_group;
#endif
extern bool opt_allow_suspicious_udfs;
extern const char *opt_secure_file_priv;
extern bool opt_log_slow_admin_statements, opt_log_slow_replica_statements;
extern bool sp_automatic_privileges, opt_noacl;
extern bool trust_function_creators;
extern bool check_proxy_users, sha256_password_proxy_users;
#ifdef _WIN32
extern const char *shared_memory_base_name;
#endif
extern const char *mysqld_unix_port;
extern char *default_tz_name;
extern Time_zone *default_tz;
extern const char *default_storage_engine;
extern const char *default_tmp_storage_engine;
extern ulonglong temptable_max_ram;
extern ulonglong temptable_max_mmap;
extern bool temptable_use_mmap;
extern bool using_udf_functions;
extern bool locked_in_memory;
extern bool opt_using_transactions;
extern ulong current_pid;
extern ulong binlog_expire_logs_seconds;
extern bool opt_binlog_expire_logs_auto_purge;
extern uint sync_binlog_period, sync_relaylog_period, sync_relayloginfo_period,
    sync_masterinfo_period, opt_mta_checkpoint_period, opt_mta_checkpoint_group;
extern ulong opt_tc_log_size, tc_log_max_pages_used, tc_log_page_size;
extern ulong tc_log_page_waits;
extern bool relay_log_purge;
extern bool relay_log_recovery;
extern std::atomic<bool> offline_mode;
extern uint test_flags, select_errors, ha_open_options;
extern uint protocol_version, mysqld_port;

enum enum_delay_key_write {
  DELAY_KEY_WRITE_NONE,
  DELAY_KEY_WRITE_ON,
  DELAY_KEY_WRITE_ALL
};
extern ulong delay_key_write_options;

extern ulong opt_log_timestamps;
extern const char *timestamp_type_names[];
extern char *opt_general_logname, *opt_slow_logname, *opt_bin_logname,
    *opt_relay_logname;
extern char *mysql_home_ptr, *pidfile_name_ptr;
extern uint default_password_lifetime;
extern bool password_require_current;
/*
  @warning : The real value is in @ref partial_revokes. The @ref
  opt_partial_revokes is just a tool to trick the Sys_var class into
  operating on an atomic variable.

  Thus : do not use or access @ref opt_partial_revokes in your code.
         If you need the value of the flag please use the @ref partial_revokes
         global.
  @todo :
    @ref opt_partial_revokes to be removed when the Sys_var classes can operate
         safely on an atomic.
 */
extern bool opt_partial_revokes;
extern char *my_bind_addr_str;
extern char *my_admin_bind_addr_str;
extern uint mysqld_admin_port;
extern bool listen_admin_interface_in_separate_thread;
extern char glob_hostname[HOSTNAME_LENGTH + 1];
extern char system_time_zone_dst_on[30], system_time_zone_dst_off[30];
extern char *opt_init_file;
extern const char *opt_tc_log_file;
extern char server_uuid[UUID_LENGTH + 1];
extern const char *server_uuid_ptr;
#if defined(HAVE_BUILD_ID_SUPPORT)
extern char server_build_id[42];
extern const char *server_build_id_ptr;
#endif
extern const double log_10[309];
extern ulong binlog_cache_use, binlog_cache_disk_use;
extern ulong binlog_stmt_cache_use, binlog_stmt_cache_disk_use;
extern ulong aborted_threads;
extern ulong delayed_insert_timeout;
extern ulong delayed_insert_limit, delayed_queue_size;
extern std::atomic<int32> atomic_replica_open_temp_tables;
extern ulong slow_launch_time;
extern ulong table_cache_size;
extern ulong schema_def_size;
extern ulong stored_program_def_size;
extern ulong table_def_size;
extern ulong tablespace_def_size;
extern MYSQL_PLUGIN_IMPORT ulong max_connections;
extern ulong max_digest_length;
extern ulong max_connect_errors, connect_timeout;
extern bool opt_replica_allow_batching;
extern ulong slave_trans_retries;
extern uint replica_net_timeout;
extern ulong opt_mts_replica_parallel_workers;
extern ulonglong opt_mts_pending_jobs_size_max;
extern ulong rpl_stop_replica_timeout;
extern ulong what_to_log, flush_time;
extern ulong max_prepared_stmt_count, prepared_stmt_count;
extern ulong open_files_limit;
extern bool clone_startup;
extern bool clone_recovery_error;
extern ulong binlog_cache_size, binlog_stmt_cache_size;
extern ulonglong max_binlog_cache_size, max_binlog_stmt_cache_size;
extern int32 opt_binlog_max_flush_queue_time;
extern long opt_binlog_group_commit_sync_delay;
extern ulong opt_binlog_group_commit_sync_no_delay_count;
extern ulong max_binlog_size, max_relay_log_size;
extern ulong replica_max_allowed_packet;
extern ulong binlog_row_event_max_size;
extern ulong binlog_checksum_options;
extern ulong binlog_row_metadata;
extern const char *binlog_checksum_type_names[];
extern bool opt_source_verify_checksum;
extern bool opt_replica_sql_verify_checksum;
extern uint32 gtid_executed_compression_period;
extern bool binlog_gtid_simple_recovery;
extern ulong binlog_error_action;
extern ulong locked_account_connection_count;
enum enum_binlog_error_action {
  /// Ignore the error and let server continue without binlogging
  IGNORE_ERROR = 0,
  /// Abort the server
  ABORT_SERVER = 1
};
extern const char *binlog_error_action_list[];
extern char *opt_authentication_policy;

extern ulong stored_program_cache_size;
extern ulong back_log;
extern "C" MYSQL_PLUGIN_IMPORT ulong server_id;
extern time_t server_start_time;
extern char *opt_mysql_tmpdir;
extern size_t mysql_unpacked_real_data_home_len;
extern MYSQL_PLUGIN_IMPORT MY_TMPDIR mysql_tmpdir_list;
extern const char *show_comp_option_name[];
extern const char *first_keyword, *binary_keyword;
extern MYSQL_PLUGIN_IMPORT const char *my_localhost;
extern const char *in_left_expr_name;
extern SHOW_VAR status_vars[];
extern struct System_variables max_system_variables;
extern struct System_status_var global_status_var;
extern struct aggregated_stats global_aggregated_stats;
extern struct rand_struct sql_rand;
extern handlerton *myisam_hton;
extern handlerton *heap_hton;
extern handlerton *temptable_hton;
extern handlerton *innodb_hton;
extern uint opt_server_id_bits;
extern ulong opt_server_id_mask;
extern const char *load_default_groups[];
extern struct my_option my_long_early_options[];
extern "C" MYSQL_PLUGIN_IMPORT bool mysqld_server_started;
extern "C" MYSQL_PLUGIN_IMPORT int orig_argc;
extern "C" MYSQL_PLUGIN_IMPORT char **orig_argv;
extern bool server_shutting_down;
extern my_thread_attr_t connection_attrib;
extern LEX_STRING opt_init_connect, opt_init_replica;
extern ulong connection_errors_internal;
extern ulong connection_errors_peer_addr;
extern char *opt_log_error_suppression_list;
extern char *opt_log_error_services;
extern char *opt_protocol_compression_algorithms;
/** The size of the host_cache. */
extern uint host_cache_size;
extern ulong log_error_verbosity;

extern bool persisted_globals_load;
extern bool opt_keyring_operations;
extern bool opt_table_encryption_privilege_check;
extern char *opt_keyring_migration_user;
extern char *opt_keyring_migration_host;
extern char *opt_keyring_migration_password;
extern char *opt_keyring_migration_socket;
extern char *opt_keyring_migration_source;
extern char *opt_keyring_migration_destination;
extern ulong opt_keyring_migration_port;

extern ulonglong global_conn_mem_limit;
extern ulonglong global_conn_mem_counter;

extern ulonglong global_conn_memory_status_limit;
extern ulonglong conn_memory_status_limit;
extern std::atomic<long>
    atomic_count_hit_query_past_global_conn_mem_status_limit;
extern std::atomic<long> atomic_count_hit_query_past_conn_mem_status_limit;

/**
  Variable to check if connection related options are set
  as part of keyring migration.
*/
extern bool migrate_connect_options;

extern LEX_CSTRING sql_statement_names[(uint)SQLCOM_END + 1];

extern thread_local MEM_ROOT **THR_MALLOC;

extern PSI_file_key key_file_binlog_cache;
extern PSI_file_key key_file_binlog_index_cache;

#ifdef HAVE_PSI_INTERFACE

extern PSI_mutex_key key_LOCK_tc;
extern PSI_mutex_key key_hash_filo_lock;
extern PSI_mutex_key key_LOCK_error_log;
extern PSI_mutex_key key_LOCK_thd_data;
extern PSI_mutex_key key_LOCK_thd_sysvar;
extern PSI_mutex_key key_LOCK_thd_protocol;
extern PSI_mutex_key key_LOCK_thd_security_ctx;
extern PSI_mutex_key key_LOG_LOCK_log;
extern PSI_mutex_key key_source_info_data_lock;
extern PSI_mutex_key key_source_info_run_lock;
extern PSI_mutex_key key_source_info_sleep_lock;
extern PSI_mutex_key key_source_info_thd_lock;
extern PSI_mutex_key key_source_info_rotate_lock;
extern PSI_mutex_key key_mutex_replica_reporting_capability_err_lock;
extern PSI_mutex_key key_relay_log_info_data_lock;
extern PSI_mutex_key key_relay_log_info_sleep_lock;
extern PSI_mutex_key key_relay_log_info_thd_lock;
extern PSI_mutex_key key_relay_log_info_log_space_lock;
extern PSI_mutex_key key_relay_log_info_run_lock;
extern PSI_mutex_key key_mutex_slave_parallel_pend_jobs;
extern PSI_mutex_key key_mutex_slave_parallel_worker;
extern PSI_mutex_key key_mutex_slave_parallel_worker_count;
extern PSI_mutex_key key_structure_guard_mutex;
extern PSI_mutex_key key_TABLE_SHARE_LOCK_ha_data;
extern PSI_mutex_key key_LOCK_query_plan;
extern PSI_mutex_key key_LOCK_thd_query;
extern PSI_mutex_key key_LOCK_cost_const;
extern PSI_mutex_key key_LOCK_current_cond;
extern PSI_mutex_key key_RELAYLOG_LOCK_commit;
extern PSI_mutex_key key_RELAYLOG_LOCK_index;
extern PSI_mutex_key key_RELAYLOG_LOCK_log;
extern PSI_mutex_key key_RELAYLOG_LOCK_log_end_pos;
extern PSI_mutex_key key_RELAYLOG_LOCK_sync;
extern PSI_mutex_key key_RELAYLOG_LOCK_xids;
extern PSI_mutex_key key_gtid_ensure_index_mutex;
extern PSI_mutex_key key_mta_temp_table_LOCK;
extern PSI_mutex_key key_mta_gaq_LOCK;
extern PSI_mutex_key key_thd_timer_mutex;
extern PSI_mutex_key key_monitor_info_run_lock;
extern PSI_mutex_key key_LOCK_delegate_connection_mutex;
extern PSI_mutex_key key_LOCK_group_replication_connection_mutex;

extern PSI_mutex_key key_commit_order_manager_mutex;
extern PSI_mutex_key key_mutex_replica_worker_hash;

extern PSI_rwlock_key key_rwlock_LOCK_logger;
extern PSI_rwlock_key key_rwlock_channel_map_lock;
extern PSI_rwlock_key key_rwlock_channel_lock;
extern PSI_rwlock_key key_rwlock_gtid_mode_lock;
extern PSI_rwlock_key key_rwlock_receiver_tsid_lock;
extern PSI_rwlock_key key_rwlock_rpl_filter_lock;
extern PSI_rwlock_key key_rwlock_channel_to_filter_lock;
extern PSI_rwlock_key key_rwlock_resource_group_mgr_map_lock;

extern PSI_cond_key key_PAGE_cond;
extern PSI_cond_key key_COND_active;
extern PSI_cond_key key_COND_pool;
extern PSI_cond_key key_COND_cache_status_changed;
extern PSI_cond_key key_item_func_sleep_cond;
extern PSI_cond_key key_source_info_data_cond;
extern PSI_cond_key key_source_info_start_cond;
extern PSI_cond_key key_source_info_stop_cond;
extern PSI_cond_key key_source_info_sleep_cond;
extern PSI_cond_key key_source_info_rotate_cond;
extern PSI_cond_key key_relay_log_info_data_cond;
extern PSI_cond_key key_relay_log_info_log_space_cond;
extern PSI_cond_key key_relay_log_info_start_cond;
extern PSI_cond_key key_relay_log_info_stop_cond;
extern PSI_cond_key key_relay_log_info_sleep_cond;
extern PSI_cond_key key_cond_slave_parallel_pend_jobs;
extern PSI_cond_key key_cond_slave_parallel_worker;
extern PSI_cond_key key_cond_mta_gaq;
extern PSI_cond_key key_RELAYLOG_update_cond;
extern PSI_cond_key key_gtid_ensure_index_cond;
extern PSI_cond_key key_COND_thr_lock;
extern PSI_cond_key key_cond_slave_worker_hash;
extern PSI_cond_key key_commit_order_manager_cond;
extern PSI_cond_key key_COND_group_replication_connection_cond_var;
extern PSI_thread_key key_thread_bootstrap;
extern PSI_thread_key key_thread_handle_manager;
extern PSI_thread_key key_thread_one_connection;
extern PSI_thread_key key_thread_compress_gtid_table;
extern PSI_thread_key key_thread_parser_service;
extern PSI_thread_key key_thread_handle_con_admin_sockets;
extern PSI_cond_key key_monitor_info_run_cond;

extern PSI_file_key key_file_binlog;
extern PSI_file_key key_file_binlog_index;
extern PSI_file_key key_file_dbopt;
extern PSI_file_key key_file_ERRMSG;
extern PSI_file_key key_select_to_file;
extern PSI_file_key key_file_fileparser;
extern PSI_file_key key_file_frm;
extern PSI_file_key key_file_load;
extern PSI_file_key key_file_loadfile;
extern PSI_file_key key_file_log_event_data;
extern PSI_file_key key_file_log_event_info;
extern PSI_file_key key_file_misc;
extern PSI_file_key key_file_tclog;
extern PSI_file_key key_file_trg;
extern PSI_file_key key_file_trn;
extern PSI_file_key key_file_init;
extern PSI_file_key key_file_general_log;
extern PSI_file_key key_file_slow_log;
extern PSI_file_key key_file_relaylog;
extern PSI_file_key key_file_relaylog_cache;
extern PSI_file_key key_file_relaylog_index;
extern PSI_file_key key_file_relaylog_index_cache;
extern PSI_file_key key_file_sdi;
extern PSI_file_key key_file_hash_join;

extern PSI_socket_key key_socket_tcpip;
extern PSI_socket_key key_socket_unix;
extern PSI_socket_key key_socket_client_connection;

#endif /* HAVE_PSI_INTERFACE */

/*
  MAINTAINER: Please keep this list in order, to limit merge collisions.
  Hint: grep PSI_stage_info | sort -u
*/
extern PSI_stage_info stage_after_create;
extern PSI_stage_info stage_alter_inplace_prepare;
extern PSI_stage_info stage_alter_inplace;
extern PSI_stage_info stage_alter_inplace_commit;
extern PSI_stage_info stage_changing_source;
extern PSI_stage_info stage_checking_source_version;
extern PSI_stage_info stage_checking_permissions;
extern PSI_stage_info stage_cleaning_up;
extern PSI_stage_info stage_closing_tables;
extern PSI_stage_info stage_compressing_gtid_table;
extern PSI_stage_info stage_connecting_to_source;
extern PSI_stage_info stage_converting_heap_to_ondisk;
extern PSI_stage_info stage_copy_to_tmp_table;
extern PSI_stage_info stage_creating_table;
extern PSI_stage_info stage_creating_tmp_table;
extern PSI_stage_info stage_deleting_from_main_table;
extern PSI_stage_info stage_deleting_from_reference_tables;
extern PSI_stage_info stage_discard_or_import_tablespace;
extern PSI_stage_info stage_end;
extern PSI_stage_info stage_executing;
extern PSI_stage_info stage_execution_of_init_command;
extern PSI_stage_info stage_explaining;
extern PSI_stage_info
    stage_finished_reading_one_binlog_switching_to_next_binlog;
extern PSI_stage_info stage_flushing_applier_and_connection_metadata;
extern PSI_stage_info stage_flushing_applier_metadata;
extern PSI_stage_info stage_freeing_items;
extern PSI_stage_info stage_fulltext_initialization;
extern PSI_stage_info stage_init;
extern PSI_stage_info stage_killing_replica;
extern PSI_stage_info stage_logging_slow_query;
extern PSI_stage_info stage_making_temp_file_append_before_load_data;
extern PSI_stage_info stage_manage_keys;
extern PSI_stage_info stage_source_has_sent_all_binlog_to_replica;
extern PSI_stage_info stage_opening_tables;
extern PSI_stage_info stage_optimizing;
extern PSI_stage_info stage_preparing;
extern PSI_stage_info stage_purging_old_relay_logs;
extern PSI_stage_info stage_query_end;
extern PSI_stage_info stage_queueing_source_event_to_the_relay_log;
extern PSI_stage_info stage_reading_event_from_the_relay_log;
extern PSI_stage_info stage_registering_replica_on_source;
extern PSI_stage_info stage_removing_tmp_table;
extern PSI_stage_info stage_rename;
extern PSI_stage_info stage_rename_result_table;
extern PSI_stage_info stage_requesting_binlog_dump;
extern PSI_stage_info stage_searching_rows_for_update;
extern PSI_stage_info stage_sending_binlog_event_to_replica;
extern PSI_stage_info stage_setup;
extern PSI_stage_info stage_replica_has_read_all_relay_log;
extern PSI_stage_info
    stage_replica_reconnecting_after_failed_binlog_dump_request;
extern PSI_stage_info stage_replica_reconnecting_after_failed_event_read;
extern PSI_stage_info
    stage_replica_reconnecting_after_failed_registration_on_source;
extern PSI_stage_info stage_replica_waiting_event_from_coordinator;
extern PSI_stage_info stage_replica_waiting_for_workers_to_process_queue;
extern PSI_stage_info
    stage_replica_waiting_to_reconnect_after_failed_binlog_dump_request;
extern PSI_stage_info
    stage_replica_waiting_to_reconnect_after_failed_event_read;
extern PSI_stage_info
    stage_replica_waiting_to_reconnect_after_failed_registration_on_source;
extern PSI_stage_info stage_replica_waiting_worker_queue;
extern PSI_stage_info stage_replica_waiting_worker_to_free_events;
extern PSI_stage_info stage_replica_waiting_worker_to_release_partition;
extern PSI_stage_info stage_replica_waiting_workers_to_exit;
extern PSI_stage_info stage_rpl_apply_row_evt_write;
extern PSI_stage_info stage_rpl_apply_row_evt_update;
extern PSI_stage_info stage_rpl_apply_row_evt_delete;
extern PSI_stage_info stage_sql_thd_waiting_until_delay;
extern PSI_stage_info stage_statistics;
extern PSI_stage_info stage_system_lock;
extern PSI_stage_info stage_update;
extern PSI_stage_info stage_updating;
extern PSI_stage_info stage_updating_main_table;
extern PSI_stage_info stage_updating_reference_tables;
extern PSI_stage_info stage_user_sleep;
extern PSI_stage_info stage_verifying_table;
extern PSI_stage_info stage_waiting_for_gtid_to_be_committed;
extern PSI_stage_info stage_waiting_for_handler_commit;
extern PSI_stage_info stage_waiting_for_source_to_send_event;
extern PSI_stage_info stage_waiting_for_source_update;
extern PSI_stage_info stage_waiting_for_relay_log_space;
extern PSI_stage_info stage_waiting_for_replica_mutex_on_exit;
extern PSI_stage_info stage_waiting_for_replica_thread_to_start;
extern PSI_stage_info stage_waiting_for_table_flush;
extern PSI_stage_info stage_waiting_for_the_next_event_in_relay_log;
extern PSI_stage_info stage_waiting_for_the_replica_thread_to_advance_position;
extern PSI_stage_info stage_waiting_to_finalize_termination;
extern PSI_stage_info stage_worker_waiting_for_its_turn_to_commit;
extern PSI_stage_info stage_worker_waiting_for_commit_parent;
extern PSI_stage_info stage_suspending;
extern PSI_stage_info stage_starting;
extern PSI_stage_info stage_waiting_for_no_channel_reference;
extern PSI_stage_info stage_hook_begin_trans;
extern PSI_stage_info stage_binlog_transaction_compress;
extern PSI_stage_info stage_binlog_transaction_decompress;
extern PSI_stage_info stage_rpl_failover_fetching_source_member_details;
extern PSI_stage_info stage_rpl_failover_updating_source_member_details;
extern PSI_stage_info stage_rpl_failover_wait_before_next_fetch;
extern PSI_stage_info stage_communication_delegation;
extern PSI_stage_info stage_wait_on_commit_ticket;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
/**
  Statement instrumentation keys (sql).
  The last entry, at [SQLCOM_END], is for parsing errors.
*/
extern PSI_statement_info sql_statement_info[(uint)SQLCOM_END + 1];

/**
  Statement instrumentation keys (com).
  The last entry, at [COM_END], is for packet errors.
*/
extern PSI_statement_info com_statement_info[(uint)COM_END + 1];

/**
  Statement instrumentation key for replication.
*/
extern PSI_statement_info stmt_info_rpl;
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

extern struct st_VioSSLFd *ssl_acceptor_fd;

extern bool opt_large_pages;
extern uint opt_large_page_size;
extern char lc_messages_dir[FN_REFLEN];
extern char *lc_messages_dir_ptr;
extern const char *log_error_dest;
extern MYSQL_PLUGIN_IMPORT char reg_ext[FN_EXTLEN];
extern MYSQL_PLUGIN_IMPORT uint reg_ext_length;
extern MYSQL_PLUGIN_IMPORT uint lower_case_table_names;

extern long tc_heuristic_recover;

extern ulong specialflag;
extern size_t mysql_data_home_len;
extern const char *mysql_real_data_home_ptr;
extern MYSQL_PLUGIN_IMPORT char *mysql_data_home;
extern "C" MYSQL_PLUGIN_IMPORT char server_version[SERVER_VERSION_LENGTH];
extern MYSQL_PLUGIN_IMPORT char mysql_real_data_home[];
extern char mysql_unpacked_real_data_home[];
extern MYSQL_PLUGIN_IMPORT struct System_variables global_system_variables;
extern char default_logfile_name[FN_REFLEN];
extern bool log_bin_supplied;
extern char default_binlogfile_name[FN_REFLEN];
extern MYSQL_PLUGIN_IMPORT char pidfile_name[];

#define mysql_tmpdir (my_tmpdir(&mysql_tmpdir_list))

/*
  Server mutex locks and condition variables.
 */
extern mysql_mutex_t LOCK_status;
extern mysql_mutex_t LOCK_uuid_generator;
extern mysql_mutex_t LOCK_crypt;
extern mysql_mutex_t LOCK_manager;
extern mysql_mutex_t LOCK_global_system_variables;
extern mysql_mutex_t LOCK_user_conn;
extern mysql_mutex_t LOCK_log_throttle_qni;
extern mysql_mutex_t LOCK_prepared_stmt_count;
extern mysql_mutex_t LOCK_replica_list;
extern mysql_mutex_t LOCK_error_messages;
extern mysql_mutex_t LOCK_sql_replica_skip_counter;
extern mysql_mutex_t LOCK_replica_net_timeout;
extern mysql_mutex_t LOCK_replica_trans_dep_tracker;
extern mysql_mutex_t LOCK_mandatory_roles;
extern mysql_mutex_t LOCK_password_history;
extern mysql_mutex_t LOCK_password_reuse_interval;
extern mysql_mutex_t LOCK_default_password_lifetime;
extern mysql_mutex_t LOCK_server_started;
extern mysql_mutex_t LOCK_reset_gtid_table;
extern mysql_mutex_t LOCK_compress_gtid_table;
extern mysql_mutex_t LOCK_keyring_operations;
extern mysql_mutex_t LOCK_collect_instance_log;
extern mysql_mutex_t LOCK_tls_ctx_options;
extern mysql_mutex_t LOCK_admin_tls_ctx_options;
extern mysql_mutex_t LOCK_rotate_binlog_master_key;
extern mysql_mutex_t LOCK_partial_revokes;
extern mysql_mutex_t LOCK_global_conn_mem_limit;
extern mysql_mutex_t LOCK_authentication_policy;

extern mysql_cond_t COND_server_started;
extern mysql_cond_t COND_compress_gtid_table;
extern mysql_cond_t COND_manager;

extern mysql_rwlock_t LOCK_sys_init_connect;
extern mysql_rwlock_t LOCK_sys_init_replica;
extern mysql_rwlock_t LOCK_system_variables_hash;
extern mysql_rwlock_t LOCK_server_shutting_down;

extern ulong opt_ssl_fips_mode;

extern char *opt_disabled_storage_engines;

extern sigset_t mysqld_signal_mask;
/* query_id */
typedef int64 query_id_t;
extern std::atomic<query_id_t> atomic_global_query_id;

int *get_remaining_argc();
char ***get_remaining_argv();

/* increment query_id and return it.  */
[[nodiscard]] inline query_id_t next_query_id() {
  return ++atomic_global_query_id;
}

#define ER(X) please_use_ER_THD_or_ER_DEFAULT_instead(X)

/* Accessor function for _connection_events_loop_aborted flag */
[[nodiscard]] inline bool connection_events_loop_aborted() {
  return connection_events_loop_aborted_flag.load();
}

/* only here because of unireg_init(). */
static inline void set_connection_events_loop_aborted(bool value) {
  connection_events_loop_aborted_flag.store(value);
}

/**

  Check if --help option or --validate-config is specified.

  @retval false   Neither 'help' or 'validate-config' option is enabled.
  @retval true    Either 'help' or 'validate-config' or both options
                  are enabled.
*/
inline bool is_help_or_validate_option() {
  return (opt_help || opt_validate_config);
}

/**
  Get mysqld offline mode.

  @return a bool indicating the offline mode status of the server.
*/
inline bool mysqld_offline_mode() { return offline_mode.load(); }

/**
  Set offline mode with a given value

  @param value true or false indicating the offline mode status of server.
*/
inline void set_mysqld_offline_mode(bool value) { offline_mode.store(value); }

/**
  Get status partial_revokes on server

  @return a bool indicating partial_revokes status of the server.
    @retval true  Parital revokes is ON
    @retval flase Partial revokes is OFF
*/
bool mysqld_partial_revokes();

/**
  Set partial_revokes with a given value

  @param value true or false indicating the status of partial revokes
               turned ON/OFF on server.
*/
void set_mysqld_partial_revokes(bool value);

bool check_and_update_partial_revokes_sysvar(THD *thd);

#ifdef _WIN32

bool is_windows_service();
NTService *get_win_service_ptr();
bool update_named_pipe_full_access_group(const char *new_group_name);

#endif

extern LEX_STRING opt_mandatory_roles;
extern bool opt_mandatory_roles_cache;
extern bool opt_always_activate_granted_roles;

extern mysql_component_t mysql_component_mysql_server;
extern mysql_component_t mysql_component_performance_schema;
/* This variable is a registry handler, defined in mysql_server component and
   used as a output parameter for minimal chassis. */
extern SERVICE_TYPE_NO_CONST(registry) * srv_registry;
extern SERVICE_TYPE_NO_CONST(registry) * srv_registry_no_lock;
/* These global variables which are defined and used in
   mysql_server component */
extern SERVICE_TYPE(dynamic_loader_scheme_file) * scheme_file_srv;
extern SERVICE_TYPE(dynamic_loader) * dynamic_loader_srv;
extern SERVICE_TYPE(registry_registration) * registry_registration;
extern SERVICE_TYPE(registry_registration) * registry_registration_no_lock;

class Deployed_components;
extern Deployed_components *g_deployed_components;

extern bool opt_persist_sensitive_variables_in_plaintext;

void persisted_variables_refresh_keyring_support();

/**
  Stores the value of argc during server start up that contains
  the count of arguments specified by the user in the
  configuration files and command line.
  The server refers the cached argument count during
  plugin and component installation.
*/
extern int argc_cached;
/**
  Stores the value of argv during server start up that contains
  the vector of arguments specified by the user in the
  configuration files and command line.
  The server refers the cached argument vector during
  plugin and component installation.
*/
extern char **argv_cached;

/// Stores the last time the warning for non-composable engine is emitted
extern std::atomic<time_t> last_mixed_non_transactional_engine_warning;
/// The time period for which no warning for non-composable engines should
/// be written to the error log after a similar warning was written

const uint16_t mixed_non_transactional_engine_warning_period = 60 * 2;
#endif /* MYSQLD_INCLUDED */

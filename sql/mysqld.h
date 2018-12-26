/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_INCLUDED
#define MYSQLD_INCLUDED

#include "my_global.h" /* MYSQL_PLUGIN_IMPORT, FN_REFLEN, FN_EXTLEN */
#include "my_bitmap.h"                     /* MY_BITMAP */
#include "my_decimal.h"                         /* my_decimal */
#include "mysql_com.h"                     /* SERVER_VERSION_LENGTH */
#include "my_atomic.h"                     /* my_atomic_add64 */
#include "sql_cmd.h"                       /* SQLCOM_END */
#include "my_thread_local.h"               /* my_get_thread_local */
#include "my_thread.h"                     /* my_thread_attr_t */
#include "atomic_class.h"                  /* Atomic_int32 */

class THD;
struct handlerton;
class Time_zone;
template <uint default_width> class Bitmap;

typedef struct st_mysql_const_lex_string LEX_CSTRING;
typedef struct st_mysql_show_var SHOW_VAR;

/*
  This forward declaration is used from C files where the real
  definition is included before.  Since C does not allow repeated
  typedef declarations, even when identical, the definition may not be
  repeated.
*/
#ifndef CHARSET_INFO_DEFINED
typedef struct charset_info_st CHARSET_INFO;
#endif  /* CHARSET_INFO_DEFINED */

#if MAX_INDEXES <= 64
typedef Bitmap<64>  key_map;          /* Used for finding keys */
#elif MAX_INDEXES > 255
#error "MAX_INDEXES values greater than 255 is not supported."
#else
typedef Bitmap<((MAX_INDEXES+7)/8*8)> key_map; /* Used for finding keys */
#endif

	/* Bits from testflag */
#define TEST_PRINT_CACHED_TABLES 1
#define TEST_NO_KEY_GROUP	 2
#define TEST_MIT_THREAD		4
/*
  TEST_BLOCKING is made obsolete and is not used any
  where in the code base and is retained here so that
  the other bit flag values are not changed.
*/
#define OBSOLETE_TEST_BLOCKING	8
#define TEST_KEEP_TMP_TABLES	16
#define TEST_READCHECK		64	/**< Force use of readcheck */
#define TEST_NO_EXTRA		128
#define TEST_CORE_ON_SIGNAL	256	/**< Give core if signal */
#define TEST_NO_STACKTRACE	512
#define TEST_SIGINT	        1024    /**< Allow sigint on threads */
#define TEST_SYNCHRONIZATION    2048    /**< get server to do sleep in
                                           some places */
#define TEST_DO_QUICK_LEAK_CHECK 4096   /**< Do Valgrind leak check for
                                           each command. */

#define SPECIAL_NO_NEW_FUNC	2		/* Skip new functions */
#define SPECIAL_SKIP_SHOW_DB    4               /* Don't allow 'show db' */
#define SPECIAL_NO_RESOLVE     64		/* Don't use gethostname */
#define SPECIAL_NO_HOST_CACHE	512		/* Don't cache hosts */
#define SPECIAL_SHORT_LOG_FORMAT 1024

/* Function prototypes */
#ifndef EMBEDDED_LIBRARY
void kill_mysql(void);
#endif
void refresh_status(THD *thd);
bool is_secure_file_path(char *path);
int handle_early_options();
void adjust_related_options(ulong *requested_open_files);
ulong sql_rnd_with_mutex();

// These are needed for unit testing.
void set_remaining_args(int argc, char **argv);
int init_common_variables();
void my_init_signals();
bool gtid_server_init();
void gtid_server_cleanup();
const char *fixup_enforce_gtid_consistency_command_line(char *value_arg);

extern "C" MYSQL_PLUGIN_IMPORT CHARSET_INFO *system_charset_info;
extern MYSQL_PLUGIN_IMPORT CHARSET_INFO *files_charset_info ;
extern MYSQL_PLUGIN_IMPORT CHARSET_INFO *national_charset_info;
extern MYSQL_PLUGIN_IMPORT CHARSET_INFO *table_alias_charset;

enum enum_server_operational_state
{
  SERVER_BOOTING,      /* Server is not operational. It is starting */
  SERVER_OPERATING,    /* Server is fully initialized and operating */
  SERVER_SHUTTING_DOWN /* erver is shutting down */
};
enum_server_operational_state get_server_state();

/**
  Character set of the buildin error messages loaded from errmsg.sys.
*/
extern CHARSET_INFO *error_message_charset_info;

extern CHARSET_INFO *character_set_filesystem;

extern MY_BITMAP temp_pool;
extern bool opt_large_files, server_id_supplied;
extern bool opt_update_log, opt_bin_log;
extern my_bool opt_log_slave_updates;
extern my_bool opt_log_unsafe_statements;
extern bool opt_general_log, opt_slow_log, opt_general_log_raw;
extern my_bool opt_backup_history_log;
extern my_bool opt_backup_progress_log;
extern ulonglong log_output_options;
extern ulong log_backup_output_options;
extern my_bool opt_log_queries_not_using_indexes;
extern ulong opt_log_throttle_queries_not_using_indexes;
extern bool opt_disable_networking, opt_skip_show_db;
extern bool opt_skip_name_resolve;
extern my_bool opt_help;
extern my_bool opt_verbose;
extern bool opt_ignore_builtin_innodb;
extern my_bool opt_character_set_client_handshake;
extern MYSQL_PLUGIN_IMPORT bool volatile abort_loop;
extern my_bool opt_bootstrap, opt_initialize;
extern my_bool opt_safe_user_create;
extern my_bool opt_safe_show_db, opt_local_infile, opt_myisam_use_mmap;
extern my_bool opt_slave_compressed_protocol, use_temp_pool;
extern ulong slave_exec_mode_options;
extern ulonglong slave_type_conversions_options;
extern my_bool read_only, opt_readonly;
extern my_bool super_read_only, opt_super_readonly;
extern my_bool lower_case_file_system;
extern ulonglong slave_rows_search_algorithms_options;
extern my_bool opt_require_secure_transport;

#ifdef HAVE_REPLICATION
extern my_bool opt_slave_preserve_commit_order;
#endif

#ifndef DBUG_OFF
extern uint slave_rows_last_search_algorithm_used;
#endif
extern ulong mts_parallel_option;
extern my_bool opt_enable_named_pipe, opt_sync_frm, opt_allow_suspicious_udfs;
extern my_bool opt_secure_auth;
extern char* opt_secure_file_priv;
extern char* opt_secure_backup_file_priv;
extern size_t opt_secure_backup_file_priv_len;
extern my_bool opt_log_slow_admin_statements, opt_log_slow_slave_statements;
extern my_bool sp_automatic_privileges, opt_noacl;
extern my_bool opt_old_style_user_limits, trust_function_creators;
extern my_bool check_proxy_users, mysql_native_password_proxy_users, sha256_password_proxy_users;
extern uint opt_crash_binlog_innodb;
extern char *shared_memory_base_name, *mysqld_unix_port;
extern my_bool opt_enable_shared_memory;
extern char *default_tz_name;
extern Time_zone *default_tz;
extern char *default_storage_engine;
extern char *default_tmp_storage_engine;
extern ulong internal_tmp_disk_storage_engine;
extern bool opt_endinfo, using_udf_functions;
extern my_bool locked_in_memory;
extern bool opt_using_transactions;
extern ulong max_long_data_size;
extern ulong current_pid;
extern ulong expire_logs_days;
extern my_bool relay_log_recovery;
extern uint sync_binlog_period, sync_relaylog_period,
            sync_relayloginfo_period, sync_masterinfo_period,
            opt_mts_checkpoint_period, opt_mts_checkpoint_group;
extern ulong opt_tc_log_size, tc_log_max_pages_used, tc_log_page_size;
extern ulong tc_log_page_waits;
extern my_bool relay_log_purge, opt_innodb_safe_binlog, opt_innodb;
extern my_bool relay_log_recovery;
extern my_bool offline_mode;
extern my_bool opt_log_builtin_as_identified_by_password;
extern uint test_flags,select_errors,ha_open_options;
extern uint protocol_version, mysqld_port, dropping_tables;
extern ulong delay_key_write_options;
extern ulong opt_log_timestamps;
extern const char *timestamp_type_names[];
extern char *opt_general_logname, *opt_slow_logname, *opt_bin_logname,
            *opt_relay_logname;
extern char *opt_backup_history_logname, *opt_backup_progress_logname,
            *opt_backup_settings_name;
extern const char *log_output_str;
extern const char *log_backup_output_str;
extern char *mysql_home_ptr, *pidfile_name_ptr;
extern char *default_auth_plugin;
extern uint default_password_lifetime;
extern char *my_bind_addr_str;
extern char glob_hostname[FN_REFLEN], mysql_home[FN_REFLEN];
extern char pidfile_name[FN_REFLEN], system_time_zone[30], *opt_init_file;
extern char default_logfile_name[FN_REFLEN];
extern char *opt_tc_log_file;
/*Move UUID_LENGTH from item_strfunc.h*/
#define UUID_LENGTH (8+1+4+1+4+1+4+1+12)
extern char server_uuid[UUID_LENGTH+1];
extern const char *server_uuid_ptr;
extern const double log_10[309];
extern ulonglong keybuff_size;
extern ulonglong thd_startup_options;
extern ulong binlog_cache_use, binlog_cache_disk_use;
extern ulong binlog_stmt_cache_use, binlog_stmt_cache_disk_use;
extern ulong aborted_threads;
extern ulong delayed_insert_timeout;
extern ulong delayed_insert_limit, delayed_queue_size;
extern ulong delayed_insert_threads, delayed_insert_writes;
extern ulong delayed_rows_in_use,delayed_insert_errors;
extern Atomic_int32 slave_open_temp_tables;
extern ulong query_cache_size, query_cache_min_res_unit;
extern ulong slow_launch_time;
extern ulong table_cache_size, table_def_size;
extern ulong table_cache_size_per_instance, table_cache_instances;
extern MYSQL_PLUGIN_IMPORT ulong max_connections;
extern ulong max_digest_length;
extern ulong max_connect_errors, connect_timeout;
extern my_bool opt_slave_allow_batching;
extern my_bool allow_slave_start;
extern LEX_CSTRING reason_slave_blocked;
extern ulong slave_trans_retries;
extern uint  slave_net_timeout;
extern ulong opt_mts_slave_parallel_workers;
extern ulonglong opt_mts_pending_jobs_size_max;
extern uint max_user_connections;
extern ulong rpl_stop_slave_timeout;
extern my_bool log_bin_use_v1_row_events;
extern ulong what_to_log,flush_time;
extern ulong max_prepared_stmt_count, prepared_stmt_count;
extern ulong open_files_limit;
extern ulong binlog_cache_size, binlog_stmt_cache_size;
extern ulonglong max_binlog_cache_size, max_binlog_stmt_cache_size;
extern int32 opt_binlog_max_flush_queue_time;
extern long opt_binlog_group_commit_sync_delay;
extern ulong opt_binlog_group_commit_sync_no_delay_count;
extern ulong max_binlog_size, max_relay_log_size;
extern ulong slave_max_allowed_packet;
extern ulong opt_binlog_rows_event_max_size;
extern ulong binlog_checksum_options;
extern const char *binlog_checksum_type_names[];
extern my_bool opt_master_verify_checksum;
extern my_bool opt_slave_sql_verify_checksum;
extern uint32 gtid_executed_compression_period;
extern my_bool binlog_gtid_simple_recovery;
extern ulong binlog_error_action;
extern ulong locked_account_connection_count;
enum enum_binlog_error_action
{
  /// Ignore the error and let server continue without binlogging
  IGNORE_ERROR= 0,
  /// Abort the server
  ABORT_SERVER= 1
};
extern const char *binlog_error_action_list[];

extern ulong stored_program_cache_size;
extern ulong back_log;
extern char language[FN_REFLEN];
extern "C" MYSQL_PLUGIN_IMPORT ulong server_id;
extern time_t server_start_time, flush_status_time;
extern char *opt_mysql_tmpdir, mysql_charsets_dir[];
extern size_t mysql_unpacked_real_data_home_len;
extern MYSQL_PLUGIN_IMPORT MY_TMPDIR mysql_tmpdir_list;
extern const char *show_comp_option_name[];
extern const char *first_keyword, *binary_keyword;
extern MYSQL_PLUGIN_IMPORT const char  *my_localhost;
extern const char *myisam_recover_options_str;
extern const char *in_left_expr_name, *in_additional_cond, *in_having_cond;
extern SHOW_VAR status_vars[];
extern struct system_variables max_system_variables;
extern struct system_status_var global_status_var;
extern struct rand_struct sql_rand;
extern const char *opt_date_time_formats[];
extern handlerton *myisam_hton;
extern handlerton *heap_hton;
extern handlerton *innodb_hton;
extern uint opt_server_id_bits;
extern ulong opt_server_id_mask;
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
/* engine specific hook, to be made generic */
extern int(*ndb_wait_setup_func)(ulong);
extern ulong opt_ndb_wait_setup;
#endif
extern const char *load_default_groups[];
extern struct my_option my_long_options[];
extern struct my_option my_long_early_options[];
extern bool mysqld_server_started;
extern "C" MYSQL_PLUGIN_IMPORT int orig_argc;
extern "C" MYSQL_PLUGIN_IMPORT char **orig_argv;
extern my_thread_attr_t connection_attrib;
extern my_bool old_mode;
extern my_bool avoid_temporal_upgrade;
extern LEX_STRING opt_init_connect, opt_init_slave;
extern char err_shared_dir[];
extern my_decimal decimal_zero;
#ifndef EMBEDDED_LIBRARY
extern ulong connection_errors_internal;
extern ulong connection_errors_peer_addr;
#endif
extern ulong log_warnings;
extern bool  opt_log_syslog_enable;
extern char *opt_log_syslog_tag;
#ifndef _WIN32
extern bool  opt_log_syslog_include_pid;
extern char *opt_log_syslog_facility;
#endif
/** The size of the host_cache. */
extern uint host_cache_size;
extern ulong log_error_verbosity;

extern bool opt_keyring_operations;
extern char *opt_keyring_migration_user;
extern char *opt_keyring_migration_host;
extern char *opt_keyring_migration_password;
extern char *opt_keyring_migration_socket;
extern char *opt_keyring_migration_source;
extern char *opt_keyring_migration_destination;
extern ulong opt_keyring_migration_port;
/**
  Variable to check if connection related options are set
  as part of keyring migration.
*/
extern bool migrate_connect_options;

/** System variable show_compatibility_56. */
extern my_bool show_compatibility_56;

#if defined(EMBEDDED_LIBRARY)
extern ulong max_allowed_packet;
extern ulong net_buffer_length;
#endif

extern LEX_CSTRING sql_statement_names[(uint) SQLCOM_END + 1];

/*
  THR_MALLOC is a key which will be used to set/get MEM_ROOT** for a thread,
  using my_set_thread_local()/my_get_thread_local().
*/
extern thread_local_key_t THR_MALLOC;
extern bool THR_MALLOC_initialized;

static inline MEM_ROOT ** my_thread_get_THR_MALLOC()
{
  DBUG_ASSERT(THR_MALLOC_initialized);
  return (MEM_ROOT**) my_get_thread_local(THR_MALLOC);
}

static inline int my_thread_set_THR_MALLOC(MEM_ROOT ** hdl)
{
  DBUG_ASSERT(THR_MALLOC_initialized);
  return my_set_thread_local(THR_MALLOC, hdl);
}

/*
  THR_THD is a key which will be used to set/get THD* for a thread,
  using my_set_thread_local()/my_get_thread_local().
*/
extern MYSQL_PLUGIN_IMPORT thread_local_key_t THR_THD;
extern bool THR_THD_initialized;

static inline THD * my_thread_get_THR_THD()
{
  DBUG_ASSERT(THR_THD_initialized);
  return (THD*)my_get_thread_local(THR_THD);
}

static inline int my_thread_set_THR_THD(THD *thd)
{
  DBUG_ASSERT(THR_THD_initialized);
  return my_set_thread_local(THR_THD, thd);
}

#ifdef HAVE_PSI_INTERFACE

C_MODE_START

extern PSI_mutex_key key_LOCK_tc;

#ifdef HAVE_OPENSSL
extern PSI_mutex_key key_LOCK_des_key_file;
#endif

extern PSI_mutex_key key_BINLOG_LOCK_commit;
extern PSI_mutex_key key_BINLOG_LOCK_commit_queue;
extern PSI_mutex_key key_BINLOG_LOCK_done;
extern PSI_mutex_key key_BINLOG_LOCK_flush_queue;
extern PSI_mutex_key key_BINLOG_LOCK_index;
extern PSI_mutex_key key_BINLOG_LOCK_log;
extern PSI_mutex_key key_BINLOG_LOCK_binlog_end_pos;
extern PSI_mutex_key key_BINLOG_LOCK_sync;
extern PSI_mutex_key key_BINLOG_LOCK_sync_queue;
extern PSI_mutex_key key_BINLOG_LOCK_xids;
extern PSI_mutex_key
  key_hash_filo_lock,
  key_LOCK_crypt, key_LOCK_error_log,
  key_LOCK_gdl, key_LOCK_global_system_variables,
  key_LOCK_lock_db, key_LOCK_logger, key_LOCK_manager,
  key_LOCK_prepared_stmt_count,
  key_LOCK_server_started, key_LOCK_status,
  key_LOCK_sql_slave_skip_counter,
  key_LOCK_slave_net_timeout,
  key_LOCK_table_share, key_LOCK_thd_data, key_LOCK_thd_sysvar,
  key_LOCK_user_conn, key_LOCK_uuid_generator, key_LOG_LOCK_log,
  key_master_info_data_lock, key_master_info_run_lock,
  key_master_info_sleep_lock, key_master_info_thd_lock,
  key_mutex_slave_reporting_capability_err_lock, key_relay_log_info_data_lock,
  key_relay_log_info_sleep_lock, key_relay_log_info_thd_lock,
  key_relay_log_info_log_space_lock, key_relay_log_info_run_lock,
  key_mutex_slave_parallel_pend_jobs, key_mutex_mts_temp_tables_lock,
  key_mutex_slave_parallel_worker,
  key_mutex_slave_parallel_worker_count,
  key_structure_guard_mutex, key_TABLE_SHARE_LOCK_ha_data,
  key_LOCK_error_messages,
  key_LOCK_log_throttle_qni, key_LOCK_query_plan, key_LOCK_thd_query,
  key_LOCK_cost_const, key_LOCK_current_cond,
  key_LOCK_keyring_operations;
extern PSI_mutex_key key_RELAYLOG_LOCK_commit;
extern PSI_mutex_key key_RELAYLOG_LOCK_commit_queue;
extern PSI_mutex_key key_RELAYLOG_LOCK_done;
extern PSI_mutex_key key_RELAYLOG_LOCK_flush_queue;
extern PSI_mutex_key key_RELAYLOG_LOCK_index;
extern PSI_mutex_key key_RELAYLOG_LOCK_log;
extern PSI_mutex_key key_RELAYLOG_LOCK_sync;
extern PSI_mutex_key key_RELAYLOG_LOCK_sync_queue;
extern PSI_mutex_key key_RELAYLOG_LOCK_xids;
extern PSI_mutex_key key_LOCK_sql_rand;
extern PSI_mutex_key key_gtid_ensure_index_mutex;
extern PSI_mutex_key key_mts_temp_table_LOCK;
extern PSI_mutex_key key_LOCK_reset_gtid_table;
extern PSI_mutex_key key_LOCK_compress_gtid_table;
extern PSI_mutex_key key_mts_gaq_LOCK;
extern PSI_mutex_key key_thd_timer_mutex;
extern PSI_mutex_key key_LOCK_offline_mode;
extern PSI_mutex_key key_LOCK_default_password_lifetime;

#ifdef HAVE_REPLICATION
extern PSI_mutex_key key_commit_order_manager_mutex;
extern PSI_mutex_key key_mutex_slave_worker_hash;
#endif

extern PSI_rwlock_key key_rwlock_LOCK_grant, key_rwlock_LOCK_logger,
  key_rwlock_LOCK_sys_init_connect, key_rwlock_LOCK_sys_init_slave,
  key_rwlock_LOCK_system_variables_hash, key_rwlock_query_cache_query_lock,
  key_rwlock_global_sid_lock, key_rwlock_gtid_mode_lock,
  key_rwlock_channel_map_lock, key_rwlock_channel_lock;

extern PSI_cond_key key_PAGE_cond, key_COND_active, key_COND_pool;
extern PSI_cond_key key_BINLOG_update_cond,
  key_COND_cache_status_changed, key_COND_manager,
  key_COND_server_started,
  key_item_func_sleep_cond, key_master_info_data_cond,
  key_master_info_start_cond, key_master_info_stop_cond,
  key_master_info_sleep_cond,
  key_relay_log_info_data_cond, key_relay_log_info_log_space_cond,
  key_relay_log_info_start_cond, key_relay_log_info_stop_cond,
  key_relay_log_info_sleep_cond, key_cond_slave_parallel_pend_jobs,
  key_cond_slave_parallel_worker, key_cond_mts_gaq,
  key_TABLE_SHARE_cond, key_user_level_lock_cond;
extern PSI_cond_key key_BINLOG_COND_done;
extern PSI_cond_key key_RELAYLOG_COND_done;
extern PSI_cond_key key_RELAYLOG_update_cond;
extern PSI_cond_key key_BINLOG_prep_xids_cond;
extern PSI_cond_key key_RELAYLOG_prep_xids_cond;
extern PSI_cond_key key_gtid_ensure_index_cond;
extern PSI_cond_key key_COND_compress_gtid_table;
extern PSI_cond_key key_COND_thr_lock;

#ifdef HAVE_REPLICATION
extern PSI_cond_key key_cond_slave_worker_hash;
extern PSI_cond_key key_commit_order_manager_cond;
#endif
extern PSI_thread_key key_thread_bootstrap,
  key_thread_handle_manager, key_thread_main,
  key_thread_one_connection, key_thread_signal_hand,
  key_thread_compress_gtid_table, key_thread_parser_service;
extern PSI_thread_key key_thread_timer_notifier;

extern PSI_file_key key_file_map;
extern PSI_file_key key_file_binlog, key_file_binlog_cache,
  key_file_binlog_index, key_file_binlog_index_cache, key_file_casetest,
  key_file_dbopt, key_file_des_key_file, key_file_ERRMSG, key_select_to_file,
  key_file_fileparser, key_file_frm, key_file_global_ddl_log, key_file_load,
  key_file_loadfile, key_file_log_event_data, key_file_log_event_info,
  key_file_master_info, key_file_misc, key_file_partition_ddl_log,
  key_file_pid, key_file_relay_log_info, key_file_send_file, key_file_tclog,
  key_file_trg, key_file_trn, key_file_init;
extern PSI_file_key key_file_general_log, key_file_slow_log;
extern PSI_file_key key_file_relaylog, key_file_relaylog_cache, key_file_relaylog_index, key_file_relaylog_index_cache;
extern PSI_socket_key key_socket_tcpip, key_socket_unix, key_socket_client_connection;

void init_server_psi_keys();

C_MODE_END

#endif /* HAVE_PSI_INTERFACE */

C_MODE_START

extern PSI_memory_key key_memory_locked_table_list;
extern PSI_memory_key key_memory_locked_thread_list;
extern PSI_memory_key key_memory_thd_transactions;
extern PSI_memory_key key_memory_delegate;
extern PSI_memory_key key_memory_acl_mem;
extern PSI_memory_key key_memory_acl_memex;
extern PSI_memory_key key_memory_acl_cache;
extern PSI_memory_key key_memory_thd_main_mem_root;
extern PSI_memory_key key_memory_help;
extern PSI_memory_key key_memory_frm;
extern PSI_memory_key key_memory_table_share;
extern PSI_memory_key key_memory_gdl;
extern PSI_memory_key key_memory_table_triggers_list;
extern PSI_memory_key key_memory_prepared_statement_map;
extern PSI_memory_key key_memory_prepared_statement_main_mem_root;
extern PSI_memory_key key_memory_protocol_rset_root;
extern PSI_memory_key key_memory_warning_info_warn_root;
extern PSI_memory_key key_memory_sp_cache;
extern PSI_memory_key key_memory_sp_head_main_root;
extern PSI_memory_key key_memory_sp_head_execute_root;
extern PSI_memory_key key_memory_sp_head_call_root;
extern PSI_memory_key key_memory_table_mapping_root;
extern PSI_memory_key key_memory_quick_range_select_root;
extern PSI_memory_key key_memory_quick_index_merge_root;
extern PSI_memory_key key_memory_quick_ror_intersect_select_root;
extern PSI_memory_key key_memory_quick_ror_union_select_root;
extern PSI_memory_key key_memory_quick_group_min_max_select_root;
extern PSI_memory_key key_memory_test_quick_select_exec;
extern PSI_memory_key key_memory_prune_partitions_exec;
extern PSI_memory_key key_memory_binlog_recover_exec;
extern PSI_memory_key key_memory_blob_mem_storage;

extern PSI_memory_key key_memory_Sys_var_charptr_value;
extern PSI_memory_key key_memory_THD_db;
extern PSI_memory_key key_memory_user_var_entry;
extern PSI_memory_key key_memory_user_var_entry_value;
extern PSI_memory_key key_memory_Slave_job_group_group_relay_log_name;
extern PSI_memory_key key_memory_Relay_log_info_group_relay_log_name;
extern PSI_memory_key key_memory_binlog_cache_mngr;
extern PSI_memory_key key_memory_Row_data_memory_memory;
extern PSI_memory_key key_memory_errmsgs;
extern PSI_memory_key key_memory_Event_queue_element_for_exec_names;
extern PSI_memory_key key_memory_Event_scheduler_scheduler_param;
extern PSI_memory_key key_memory_Gis_read_stream_err_msg;
extern PSI_memory_key key_memory_Geometry_objects_data;
extern PSI_memory_key key_memory_host_cache_hostname;
extern PSI_memory_key key_memory_User_level_lock;
extern PSI_memory_key key_memory_Filesort_info_record_pointers;
extern PSI_memory_key key_memory_Sort_param_tmp_buffer;
extern PSI_memory_key key_memory_Filesort_info_merge;
extern PSI_memory_key key_memory_Filesort_buffer_sort_keys;
extern PSI_memory_key key_memory_handler_errmsgs;
extern PSI_memory_key key_memory_handlerton;
extern PSI_memory_key key_memory_XID;
extern PSI_memory_key key_memory_MYSQL_LOCK;
extern PSI_memory_key key_memory_MYSQL_LOG_name;
extern PSI_memory_key key_memory_TC_LOG_MMAP_pages;
extern PSI_memory_key key_memory_my_str_malloc;
extern PSI_memory_key key_memory_MYSQL_BIN_LOG_basename;
extern PSI_memory_key key_memory_MYSQL_BIN_LOG_index;
extern PSI_memory_key key_memory_MYSQL_RELAY_LOG_basename;
extern PSI_memory_key key_memory_MYSQL_RELAY_LOG_index;
extern PSI_memory_key key_memory_rpl_filter;
extern PSI_memory_key key_memory_Security_context;
extern PSI_memory_key key_memory_NET_buff;
extern PSI_memory_key key_memory_NET_compress_packet;
extern PSI_memory_key key_memory_my_bitmap_map;
extern PSI_memory_key key_memory_QUICK_RANGE_SELECT_mrr_buf_desc;
extern PSI_memory_key key_memory_TABLE_RULE_ENT;
extern PSI_memory_key key_memory_Mutex_cond_array_Mutex_cond;
extern PSI_memory_key key_memory_Owned_gtids_sidno_to_hash;
extern PSI_memory_key key_memory_Sid_map_Node;
extern PSI_memory_key key_memory_bison_stack;
extern PSI_memory_key key_memory_TABLE_sort_io_cache;
extern PSI_memory_key key_memory_DATE_TIME_FORMAT;
extern PSI_memory_key key_memory_DDL_LOG_MEMORY_ENTRY;
extern PSI_memory_key key_memory_ST_SCHEMA_TABLE;
extern PSI_memory_key key_memory_ignored_db;
extern PSI_memory_key key_memory_SLAVE_INFO;
extern PSI_memory_key key_memory_log_event_old;
extern PSI_memory_key key_memory_HASH_ROW_ENTRY;
extern PSI_memory_key key_memory_table_def_memory;
extern PSI_memory_key key_memory_MPVIO_EXT_auth_info;
extern PSI_memory_key key_memory_LOG_POS_COORD;
extern PSI_memory_key key_memory_XID_STATE;
extern PSI_memory_key key_memory_Rpl_info_file_buffer;
extern PSI_memory_key key_memory_Rpl_info_table;
extern PSI_memory_key key_memory_binlog_pos;
extern PSI_memory_key key_memory_db_worker_hash_entry;
extern PSI_memory_key key_memory_rpl_slave_command_buffer;
extern PSI_memory_key key_memory_binlog_ver_1_event;
extern PSI_memory_key key_memory_rpl_slave_check_temp_dir;
extern PSI_memory_key key_memory_TABLE;
extern PSI_memory_key key_memory_binlog_statement_buffer;
extern PSI_memory_key key_memory_user_conn;
extern PSI_memory_key key_memory_dboptions_hash;
extern PSI_memory_key key_memory_hash_index_key_buffer;
extern PSI_memory_key key_memory_THD_handler_tables_hash;
extern PSI_memory_key key_memory_JOIN_CACHE;
extern PSI_memory_key key_memory_READ_INFO;
extern PSI_memory_key key_memory_partition_syntax_buffer;
extern PSI_memory_key key_memory_global_system_variables;
extern PSI_memory_key key_memory_THD_variables;
extern PSI_memory_key key_memory_PROFILE;
extern PSI_memory_key key_memory_LOG_name;
extern PSI_memory_key key_memory_string_iterator;
extern PSI_memory_key key_memory_frm_extra_segment_buff;
extern PSI_memory_key key_memory_frm_form_pos;
extern PSI_memory_key key_memory_frm_string;
extern PSI_memory_key key_memory_Unique_sort_buffer;
extern PSI_memory_key key_memory_Unique_merge_buffer;
extern PSI_memory_key key_memory_shared_memory_name;
extern PSI_memory_key key_memory_opt_bin_logname;
extern PSI_memory_key key_memory_Query_cache;
extern PSI_memory_key key_memory_READ_RECORD_cache;
extern PSI_memory_key key_memory_Quick_ranges;
extern PSI_memory_key key_memory_File_query_log_name;
extern PSI_memory_key key_memory_Table_trigger_dispatcher;
extern PSI_memory_key key_memory_show_slave_status_io_gtid_set;
extern PSI_memory_key key_memory_write_set_extraction;
extern PSI_memory_key key_memory_thd_timer;
extern PSI_memory_key key_memory_THD_Session_tracker;
extern PSI_memory_key key_memory_THD_Session_sysvar_resource_manager;
extern PSI_memory_key key_memory_get_all_tables;
extern PSI_memory_key key_memory_fill_schema_schemata;
extern PSI_memory_key key_memory_native_functions;
extern PSI_memory_key key_memory_JSON;

C_MODE_END

/*
  MAINTAINER: Please keep this list in order, to limit merge collisions.
  Hint: grep PSI_stage_info | sort -u
*/
extern PSI_stage_info stage_after_create;
extern PSI_stage_info stage_allocating_local_table;
extern PSI_stage_info stage_alter_inplace_prepare;
extern PSI_stage_info stage_alter_inplace;
extern PSI_stage_info stage_alter_inplace_commit;
extern PSI_stage_info stage_changing_master;
extern PSI_stage_info stage_checking_master_version;
extern PSI_stage_info stage_checking_permissions;
extern PSI_stage_info stage_checking_privileges_on_cached_query;
extern PSI_stage_info stage_checking_query_cache_for_query;
extern PSI_stage_info stage_cleaning_up;
extern PSI_stage_info stage_closing_tables;
extern PSI_stage_info stage_compressing_gtid_table;
extern PSI_stage_info stage_connecting_to_master;
extern PSI_stage_info stage_converting_heap_to_ondisk;
extern PSI_stage_info stage_copying_to_group_table;
extern PSI_stage_info stage_copying_to_tmp_table;
extern PSI_stage_info stage_copy_to_tmp_table;
extern PSI_stage_info stage_creating_sort_index;
extern PSI_stage_info stage_creating_table;
extern PSI_stage_info stage_creating_tmp_table;
extern PSI_stage_info stage_deleting_from_main_table;
extern PSI_stage_info stage_deleting_from_reference_tables;
extern PSI_stage_info stage_discard_or_import_tablespace;
extern PSI_stage_info stage_end;
extern PSI_stage_info stage_executing;
extern PSI_stage_info stage_execution_of_init_command;
extern PSI_stage_info stage_explaining;
extern PSI_stage_info stage_finished_reading_one_binlog_switching_to_next_binlog;
extern PSI_stage_info stage_flushing_relay_log_and_master_info_repository;
extern PSI_stage_info stage_flushing_relay_log_info_file;
extern PSI_stage_info stage_freeing_items;
extern PSI_stage_info stage_fulltext_initialization;
extern PSI_stage_info stage_got_handler_lock;
extern PSI_stage_info stage_got_old_table;
extern PSI_stage_info stage_init;
extern PSI_stage_info stage_insert;
extern PSI_stage_info stage_invalidating_query_cache_entries_table;
extern PSI_stage_info stage_invalidating_query_cache_entries_table_list;
extern PSI_stage_info stage_killing_slave;
extern PSI_stage_info stage_logging_slow_query;
extern PSI_stage_info stage_making_temp_file_append_before_load_data;
extern PSI_stage_info stage_making_temp_file_create_before_load_data;
extern PSI_stage_info stage_manage_keys;
extern PSI_stage_info stage_master_has_sent_all_binlog_to_slave;
extern PSI_stage_info stage_opening_tables;
extern PSI_stage_info stage_optimizing;
extern PSI_stage_info stage_preparing;
extern PSI_stage_info stage_purging_old_relay_logs;
extern PSI_stage_info stage_query_end;
extern PSI_stage_info stage_queueing_master_event_to_the_relay_log;
extern PSI_stage_info stage_reading_event_from_the_relay_log;
extern PSI_stage_info stage_registering_slave_on_master;
extern PSI_stage_info stage_removing_duplicates;
extern PSI_stage_info stage_removing_tmp_table;
extern PSI_stage_info stage_rename;
extern PSI_stage_info stage_rename_result_table;
extern PSI_stage_info stage_requesting_binlog_dump;
extern PSI_stage_info stage_reschedule;
extern PSI_stage_info stage_searching_rows_for_update;
extern PSI_stage_info stage_sending_binlog_event_to_slave;
extern PSI_stage_info stage_sending_cached_result_to_client;
extern PSI_stage_info stage_sending_data;
extern PSI_stage_info stage_setup;
extern PSI_stage_info stage_slave_has_read_all_relay_log;
extern PSI_stage_info stage_slave_waiting_event_from_coordinator;
extern PSI_stage_info stage_slave_waiting_for_workers_to_process_queue;
extern PSI_stage_info stage_slave_waiting_worker_queue;
extern PSI_stage_info stage_slave_waiting_worker_to_free_events;
extern PSI_stage_info stage_slave_waiting_worker_to_release_partition;
extern PSI_stage_info stage_slave_waiting_workers_to_exit;
extern PSI_stage_info stage_sorting_for_group;
extern PSI_stage_info stage_sorting_for_order;
extern PSI_stage_info stage_sorting_result;
extern PSI_stage_info stage_sql_thd_waiting_until_delay;
extern PSI_stage_info stage_statistics;
extern PSI_stage_info stage_storing_result_in_query_cache;
extern PSI_stage_info stage_storing_row_into_queue;
extern PSI_stage_info stage_system_lock;
extern PSI_stage_info stage_update;
extern PSI_stage_info stage_updating;
extern PSI_stage_info stage_updating_main_table;
extern PSI_stage_info stage_updating_reference_tables;
extern PSI_stage_info stage_upgrading_lock;
extern PSI_stage_info stage_user_sleep;
extern PSI_stage_info stage_verifying_table;
extern PSI_stage_info stage_waiting_for_gtid_to_be_committed;
extern PSI_stage_info stage_waiting_for_handler_insert;
extern PSI_stage_info stage_waiting_for_handler_lock;
extern PSI_stage_info stage_waiting_for_handler_open;
extern PSI_stage_info stage_waiting_for_insert;
extern PSI_stage_info stage_waiting_for_master_to_send_event;
extern PSI_stage_info stage_waiting_for_master_update;
extern PSI_stage_info stage_waiting_for_relay_log_space;
extern PSI_stage_info stage_waiting_for_slave_mutex_on_exit;
extern PSI_stage_info stage_waiting_for_slave_thread_to_start;
extern PSI_stage_info stage_waiting_for_query_cache_lock;
extern PSI_stage_info stage_waiting_for_table_flush;
extern PSI_stage_info stage_waiting_for_the_next_event_in_relay_log;
extern PSI_stage_info stage_waiting_for_the_slave_thread_to_advance_position;
extern PSI_stage_info stage_waiting_to_finalize_termination;
extern PSI_stage_info stage_worker_waiting_for_its_turn_to_commit;
extern PSI_stage_info stage_worker_waiting_for_commit_parent;
extern PSI_stage_info stage_suspending;
extern PSI_stage_info stage_starting;
extern PSI_stage_info stage_waiting_for_no_channel_reference;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
/**
  Statement instrumentation keys (sql).
  The last entry, at [SQLCOM_END], is for parsing errors.
*/
extern PSI_statement_info sql_statement_info[(uint) SQLCOM_END + 1];

/**
  Statement instrumentation keys (com).
  The last entry, at [COM_END], is for packet errors.
*/
extern PSI_statement_info com_statement_info[(uint) COM_END + 1];

/**
  Statement instrumentation key for replication.
*/
extern PSI_statement_info stmt_info_rpl;

void init_sql_statement_info();
void init_com_statement_info();
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

#ifndef _WIN32
extern my_thread_t signal_thread;
#endif

#ifdef HAVE_OPENSSL
extern struct st_VioSSLFd * ssl_acceptor_fd;
#endif /* HAVE_OPENSSL */

/*
  The following variables were under INNODB_COMPABILITY_HOOKS
 */
extern my_bool opt_large_pages;
extern uint opt_large_page_size;
extern char lc_messages_dir[FN_REFLEN];
extern char *lc_messages_dir_ptr;
extern const char *log_error_dest;
extern MYSQL_PLUGIN_IMPORT char reg_ext[FN_EXTLEN];
extern MYSQL_PLUGIN_IMPORT uint reg_ext_length;
extern MYSQL_PLUGIN_IMPORT uint lower_case_table_names;
extern MYSQL_PLUGIN_IMPORT bool mysqld_embedded;

extern long tc_heuristic_recover;

extern ulong specialflag;
extern size_t mysql_data_home_len;
extern size_t mysql_real_data_home_len;
extern const char *mysql_real_data_home_ptr;
extern MYSQL_PLUGIN_IMPORT char  *mysql_data_home;
extern "C" MYSQL_PLUGIN_IMPORT char server_version[SERVER_VERSION_LENGTH];
extern MYSQL_PLUGIN_IMPORT char mysql_real_data_home[];
extern char mysql_unpacked_real_data_home[];
extern MYSQL_PLUGIN_IMPORT struct system_variables global_system_variables;
extern char default_logfile_name[FN_REFLEN];

#define mysql_tmpdir (my_tmpdir(&mysql_tmpdir_list))

extern MYSQL_PLUGIN_IMPORT const key_map key_map_empty;
extern MYSQL_PLUGIN_IMPORT key_map key_map_full;          /* Should be threaded as const */

/*
  Server mutex locks and condition variables.
 */
extern mysql_mutex_t
       LOCK_item_func_sleep, LOCK_status,
       LOCK_uuid_generator,
       LOCK_crypt, LOCK_timezone,
       LOCK_slave_list, LOCK_manager,
       LOCK_global_system_variables, LOCK_user_conn, LOCK_log_throttle_qni,
       LOCK_prepared_stmt_count, LOCK_error_messages,
       LOCK_sql_slave_skip_counter, LOCK_slave_net_timeout,
       LOCK_offline_mode, LOCK_default_password_lifetime;
#ifdef HAVE_OPENSSL
extern char* des_key_file;
extern mysql_mutex_t LOCK_des_key_file;
#endif
extern mysql_mutex_t LOCK_server_started;
extern mysql_cond_t COND_server_started;
extern mysql_mutex_t LOCK_reset_gtid_table;
extern mysql_mutex_t LOCK_compress_gtid_table;
extern mysql_cond_t COND_compress_gtid_table;
extern mysql_rwlock_t LOCK_sys_init_connect, LOCK_sys_init_slave;
extern mysql_rwlock_t LOCK_system_variables_hash;
extern mysql_cond_t COND_manager;
extern int32 thread_running;
extern mysql_mutex_t LOCK_keyring_operations;

extern char *opt_ssl_ca, *opt_ssl_capath, *opt_ssl_cert, *opt_ssl_cipher,
            *opt_ssl_key, *opt_ssl_crl, *opt_ssl_crlpath, *opt_tls_version;


extern char *opt_disabled_storage_engines;
/**
  only options that need special treatment in get_one_option() deserve
  to be listed below
*/
enum options_mysqld
{
  OPT_to_set_the_start_number=256,
  OPT_BIND_ADDRESS,
  OPT_BINLOG_CHECKSUM,
  OPT_BINLOG_DO_DB,
  OPT_BINLOG_FORMAT,
  OPT_BINLOG_MAX_FLUSH_QUEUE_TIME,
  OPT_BINLOG_IGNORE_DB,
  OPT_BIN_LOG,
  OPT_BOOTSTRAP,
  OPT_CONSOLE,
  OPT_DEBUG_SYNC_TIMEOUT,
  OPT_DELAY_KEY_WRITE_ALL,
  OPT_ISAM_LOG,
  OPT_IGNORE_DB_DIRECTORY,
  OPT_KEY_BUFFER_SIZE,
  OPT_KEY_CACHE_AGE_THRESHOLD,
  OPT_KEY_CACHE_BLOCK_SIZE,
  OPT_KEY_CACHE_DIVISION_LIMIT,
  OPT_LC_MESSAGES_DIRECTORY,
  OPT_LOWER_CASE_TABLE_NAMES,
  OPT_MASTER_RETRY_COUNT,
  OPT_MASTER_VERIFY_CHECKSUM,
  OPT_POOL_OF_THREADS,
  OPT_REPLICATE_DO_DB,
  OPT_REPLICATE_DO_TABLE,
  OPT_REPLICATE_IGNORE_DB,
  OPT_REPLICATE_IGNORE_TABLE,
  OPT_REPLICATE_REWRITE_DB,
  OPT_REPLICATE_WILD_DO_TABLE,
  OPT_REPLICATE_WILD_IGNORE_TABLE,
  OPT_SERVER_ID,
  OPT_SKIP_HOST_CACHE,
  OPT_SKIP_LOCK,
  OPT_SKIP_NEW,
  OPT_SKIP_RESOLVE,
  OPT_SKIP_STACK_TRACE,
  OPT_SKIP_SYMLINKS,
  OPT_SLAVE_SQL_VERIFY_CHECKSUM,
  OPT_SSL_CA,
  OPT_SSL_CAPATH,
  OPT_SSL_CERT,
  OPT_SSL_CIPHER,
  OPT_TLS_VERSION,
  OPT_SSL_KEY,
  OPT_UPDATE_LOG,
  OPT_WANT_CORE,
  OPT_LOG_ERROR,
  OPT_MAX_LONG_DATA_SIZE,
  OPT_EARLY_PLUGIN_LOAD,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_LOAD_ADD,
  OPT_SSL_CRL,
  OPT_SSL_CRLPATH,
  OPT_PFS_INSTRUMENT,
  OPT_DEFAULT_AUTH,
  OPT_SECURE_AUTH,
  OPT_THREAD_CACHE_SIZE,
  OPT_HOST_CACHE_SIZE,
  OPT_TABLE_DEFINITION_CACHE,
  OPT_MDL_CACHE_SIZE,
  OPT_MDL_HASH_INSTANCES,
  OPT_SKIP_INNODB,
  OPT_AVOID_TEMPORAL_UPGRADE,
  OPT_SHOW_OLD_TEMPORALS,
  OPT_ENFORCE_GTID_CONSISTENCY,
  OPT_TRANSACTION_READ_ONLY,
  OPT_TRANSACTION_ISOLATION,
  OPT_KEYRING_MIGRATION_SOURCE,
  OPT_KEYRING_MIGRATION_DESTINATION,
  OPT_KEYRING_MIGRATION_USER,
  OPT_KEYRING_MIGRATION_HOST,
  OPT_KEYRING_MIGRATION_PASSWORD,
  OPT_KEYRING_MIGRATION_SOCKET,
  OPT_KEYRING_MIGRATION_PORT
};


/**
   Query type constants (usable as bitmap flags).
*/
enum enum_query_type
{
  /// Nothing specific, ordinary SQL query.
  QT_ORDINARY= 0,
  /// In utf8.
  QT_TO_SYSTEM_CHARSET= (1 << 0),
  /// Without character set introducers.
  QT_WITHOUT_INTRODUCERS= (1 << 1),
  /// When printing a SELECT, add its number (select_lex->number)
  QT_SHOW_SELECT_NUMBER= (1 << 2),
  /// Don't print a database if it's equal to the connection's database
  QT_NO_DEFAULT_DB= (1 << 3),
  /// When printing a derived table, don't print its expression, only alias
  QT_DERIVED_TABLE_ONLY_ALIAS= (1 << 4),
  /// Print in charset of Item::print() argument (typically thd->charset()).
  QT_TO_ARGUMENT_CHARSET= (1 << 5),
  /// Print identifiers without database's name
  QT_NO_DB= (1 << 6),
  /// Print identifiers without table's name
  QT_NO_TABLE= (1 << 7),
  /**
    Change all Item_basic_constant to ? (used by query rewrite to compute
    digest.)  Un-resolved hints will also be printed in this format.
  */
  QT_NORMALIZED_FORMAT= (1 << 8),
  /**
    If an expression is constant, print the expression, not the value
    it evaluates to. Should be used for error messages, so that they
    don't reveal values.
  */
  QT_NO_DATA_EXPANSION= (1 << 9),
};

/* query_id */
typedef int64 query_id_t;
extern query_id_t global_query_id;

/* increment query_id and return it.  */
inline MY_ATTRIBUTE((warn_unused_result)) query_id_t next_query_id()
{
  query_id_t id= my_atomic_add64(&global_query_id, 1);
  return (id+1);
}

/*
  TODO: Replace this with an inline function.
 */
#ifndef EMBEDDED_LIBRARY
extern "C" void unireg_abort(int exit_code) MY_ATTRIBUTE((noreturn));
#else
extern "C" void unireg_clear(int exit_code);
#define unireg_abort(exit_code) do { unireg_clear(exit_code); DBUG_RETURN(exit_code); } while(0)
#endif

#if defined(MYSQL_DYNAMIC_PLUGIN) && defined(_WIN32)
extern "C" THD *_current_thd_noinline();
#define _current_thd() _current_thd_noinline()
#else
static inline THD *_current_thd(void)
{
  return my_thread_get_THR_THD();
}
#endif
#define current_thd _current_thd()

#define ER(X)         ER_THD(current_thd,X)

#endif /* MYSQLD_INCLUDED */

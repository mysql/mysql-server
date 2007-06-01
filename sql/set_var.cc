/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Handling of MySQL SQL variables

  To add a new variable, one has to do the following:

  - Use one of the 'sys_var... classes from set_var.h or write a specific
    one for the variable type.
  - Define it in the 'variable definition list' in this file.
  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - If the variable should be changed from the command line, add a definition
    of it in the my_option structure list in mysqld.cc
  - Don't forget to initialize new fields in global_system_variables and
    max_system_variables!

  NOTES:
    - Be careful with var->save_result: sys_var::check() only updates
    ulonglong_value; so other members of the union are garbage then; to use
    them you must first assign a value to them (in specific ::check() for
    example).

  TODO:
    - Add full support for the variable character_set (for 4.1)

    - When updating myisam_delay_key_write, we should do a 'flush tables'
      of all MyISAM tables to ensure that they are reopen with the
      new attribute.
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <mysql.h>
#include "slave.h"
#include "rpl_mi.h"
#include <my_getopt.h>
#include <thr_alarm.h>
#include <myisam.h>
#include <my_dir.h>

#include "events.h"

/* WITH_NDBCLUSTER_STORAGE_ENGINE */
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
extern ulong ndb_cache_check_time;
extern char opt_ndb_constrbuf[];
extern ulong ndb_extra_logging;
#endif

#ifdef HAVE_NDB_BINLOG
extern ulong ndb_report_thresh_binlog_epoch_slip;
extern ulong ndb_report_thresh_binlog_mem_usage;
extern my_bool opt_ndb_log_update_as_write;
#endif

extern CHARSET_INFO *character_set_filesystem;


static DYNAMIC_ARRAY fixed_show_vars;
static HASH system_variable_hash;

const char *bool_type_names[]= { "OFF", "ON", NullS };
TYPELIB bool_typelib=
{
  array_elements(bool_type_names)-1, "", bool_type_names, NULL
};

const char *delay_key_write_type_names[]= { "OFF", "ON", "ALL", NullS };
TYPELIB delay_key_write_typelib=
{
  array_elements(delay_key_write_type_names)-1, "",
  delay_key_write_type_names, NULL
};

static int  sys_check_ftb_syntax(THD *thd,  set_var *var);
static bool sys_update_ftb_syntax(THD *thd, set_var * var);
static void sys_default_ftb_syntax(THD *thd, enum_var_type type);
static bool sys_update_init_connect(THD*, set_var*);
static void sys_default_init_connect(THD*, enum_var_type type);
static bool sys_update_init_slave(THD*, set_var*);
static void sys_default_init_slave(THD*, enum_var_type type);
static bool set_option_bit(THD *thd, set_var *var);
static bool set_option_autocommit(THD *thd, set_var *var);
static int  check_log_update(THD *thd, set_var *var);
static bool set_log_update(THD *thd, set_var *var);
static int  check_pseudo_thread_id(THD *thd, set_var *var);
void fix_binlog_format_after_update(THD *thd, enum_var_type type);
static void fix_low_priority_updates(THD *thd, enum_var_type type);
static int check_tx_isolation(THD *thd, set_var *var);
static void fix_tx_isolation(THD *thd, enum_var_type type);
static int check_completion_type(THD *thd, set_var *var);
static void fix_completion_type(THD *thd, enum_var_type type);
static void fix_net_read_timeout(THD *thd, enum_var_type type);
static void fix_net_write_timeout(THD *thd, enum_var_type type);
static void fix_net_retry_count(THD *thd, enum_var_type type);
static void fix_max_join_size(THD *thd, enum_var_type type);
static void fix_query_cache_size(THD *thd, enum_var_type type);
static void fix_query_cache_min_res_unit(THD *thd, enum_var_type type);
static void fix_myisam_max_sort_file_size(THD *thd, enum_var_type type);
static void fix_max_binlog_size(THD *thd, enum_var_type type);
static void fix_max_relay_log_size(THD *thd, enum_var_type type);
static void fix_max_connections(THD *thd, enum_var_type type);
static int check_max_delayed_threads(THD *thd, set_var *var);
static void fix_thd_mem_root(THD *thd, enum_var_type type);
static void fix_trans_mem_root(THD *thd, enum_var_type type);
static void fix_server_id(THD *thd, enum_var_type type);
static KEY_CACHE *create_key_cache(const char *name, uint length);
void fix_sql_mode_var(THD *thd, enum_var_type type);
static byte *get_error_count(THD *thd);
static byte *get_warning_count(THD *thd);
static byte *get_tmpdir(THD *thd);
static int  sys_check_log_path(THD *thd,  set_var *var);
static bool sys_update_general_log_path(THD *thd, set_var * var);
static void sys_default_general_log_path(THD *thd, enum_var_type type);
static bool sys_update_slow_log_path(THD *thd, set_var * var);
static void sys_default_slow_log_path(THD *thd, enum_var_type type);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order.

  The variables are linked into the list. A variable is added to
  it in the constructor (see sys_var class for details).
*/

static sys_var_chain vars = { NULL, NULL };

static sys_var_thd_ulong	sys_auto_increment_increment(&vars, "auto_increment_increment",
                                                     &SV::auto_increment_increment);
static sys_var_thd_ulong	sys_auto_increment_offset(&vars, "auto_increment_offset",
                                                  &SV::auto_increment_offset);

static sys_var_bool_ptr	sys_automatic_sp_privileges(&vars, "automatic_sp_privileges",
					      &sp_automatic_privileges);

static sys_var_const_str       sys_basedir(&vars, "basedir", mysql_home);
static sys_var_long_ptr	sys_binlog_cache_size(&vars, "binlog_cache_size",
					      &binlog_cache_size);
static sys_var_thd_binlog_format sys_binlog_format(&vars, "binlog_format",
                                            &SV::binlog_format);
static sys_var_thd_ulong	sys_bulk_insert_buff_size(&vars, "bulk_insert_buffer_size",
						  &SV::bulk_insert_buff_size);
static sys_var_character_set_sv	sys_character_set_server(&vars, "character_set_server",
                                        &SV::collation_server,
                                        &default_charset_info);
sys_var_const_str       sys_charset_system(&vars, "character_set_system",
                                           (char *)my_charset_utf8_general_ci.name);
static sys_var_character_set_database	sys_character_set_database(&vars, "character_set_database");
static sys_var_character_set_sv sys_character_set_client(&vars, "character_set_client",
                                        &SV::character_set_client,
                                        &default_charset_info);
static sys_var_character_set_sv sys_character_set_connection(&vars, "character_set_connection",
                                        &SV::collation_connection,
                                        &default_charset_info);
static sys_var_character_set_sv sys_character_set_results(&vars, "character_set_results",
                                        &SV::character_set_results,
                                        &default_charset_info, true);
static sys_var_character_set_sv sys_character_set_filesystem(&vars, "character_set_filesystem",
                                        &SV::character_set_filesystem,
                                        &character_set_filesystem);
static sys_var_thd_ulong	sys_completion_type(&vars, "completion_type",
					 &SV::completion_type,
					 check_completion_type,
					 fix_completion_type);
static sys_var_collation_sv sys_collation_connection(&vars, "collation_connection",
                                        &SV::collation_connection,
                                        &default_charset_info);
static sys_var_collation_sv sys_collation_database(&vars, "collation_database",
                                        &SV::collation_database,
                                        &default_charset_info);
static sys_var_collation_sv sys_collation_server(&vars, "collation_server",
                                        &SV::collation_server,
                                        &default_charset_info);
static sys_var_long_ptr	sys_concurrent_insert(&vars, "concurrent_insert",
                                              &myisam_concurrent_insert);
static sys_var_long_ptr	sys_connect_timeout(&vars, "connect_timeout",
					    &connect_timeout);
static sys_var_const_str       sys_datadir(&vars, "datadir", mysql_real_data_home);
#ifndef DBUG_OFF
static sys_var_thd_dbug        sys_dbug(&vars, "debug");
#endif
static sys_var_enum		sys_delay_key_write(&vars, "delay_key_write",
					    &delay_key_write_options,
					    &delay_key_write_typelib,
					    fix_delay_key_write);
static sys_var_long_ptr	sys_delayed_insert_limit(&vars, "delayed_insert_limit",
						 &delayed_insert_limit);
static sys_var_long_ptr	sys_delayed_insert_timeout(&vars, "delayed_insert_timeout",
						   &delayed_insert_timeout);
static sys_var_long_ptr	sys_delayed_queue_size(&vars, "delayed_queue_size",
					       &delayed_queue_size);

static sys_var_event_scheduler sys_event_scheduler(&vars, "event_scheduler");
static sys_var_long_ptr	sys_expire_logs_days(&vars, "expire_logs_days",
					     &expire_logs_days);
static sys_var_bool_ptr	sys_flush(&vars, "flush", &myisam_flush);
static sys_var_long_ptr	sys_flush_time(&vars, "flush_time", &flush_time);
static sys_var_str             sys_ft_boolean_syntax(&vars, "ft_boolean_syntax",
                                         sys_check_ftb_syntax,
                                         sys_update_ftb_syntax,
                                         sys_default_ftb_syntax,
                                         ft_boolean_syntax);
sys_var_str             sys_init_connect(&vars, "init_connect", 0,
                                         sys_update_init_connect,
                                         sys_default_init_connect,0);
sys_var_str             sys_init_slave(&vars, "init_slave", 0,
                                       sys_update_init_slave,
                                       sys_default_init_slave,0);
static sys_var_thd_ulong	sys_interactive_timeout(&vars, "interactive_timeout",
						&SV::net_interactive_timeout);
static sys_var_thd_ulong	sys_join_buffer_size(&vars, "join_buffer_size",
					     &SV::join_buff_size);
static sys_var_key_buffer_size	sys_key_buffer_size(&vars, "key_buffer_size");
static sys_var_key_cache_long  sys_key_cache_block_size(&vars, "key_cache_block_size",
						 offsetof(KEY_CACHE,
							  param_block_size));
static sys_var_key_cache_long	sys_key_cache_division_limit(&vars, "key_cache_division_limit",
						     offsetof(KEY_CACHE,
							      param_division_limit));
static sys_var_key_cache_long  sys_key_cache_age_threshold(&vars, "key_cache_age_threshold",
						     offsetof(KEY_CACHE,
							      param_age_threshold));
static sys_var_bool_ptr	sys_local_infile(&vars, "local_infile",
					 &opt_local_infile);
static sys_var_trust_routine_creators
sys_trust_routine_creators(&vars, "log_bin_trust_routine_creators",
                           &trust_function_creators);
static sys_var_bool_ptr       
sys_trust_function_creators(&vars, "log_bin_trust_function_creators",
                            &trust_function_creators);
static sys_var_bool_ptr
  sys_log_queries_not_using_indexes(&vars, "log_queries_not_using_indexes",
                                    &opt_log_queries_not_using_indexes);
static sys_var_thd_ulong	sys_log_warnings(&vars, "log_warnings", &SV::log_warnings);
static sys_var_thd_ulong	sys_long_query_time(&vars, "long_query_time",
					     &SV::long_query_time);
static sys_var_thd_bool	sys_low_priority_updates(&vars, "low_priority_updates",
						 &SV::low_priority_updates,
						 fix_low_priority_updates);
#ifndef TO_BE_DELETED	/* Alias for the low_priority_updates */
static sys_var_thd_bool	sys_sql_low_priority_updates(&vars, "sql_low_priority_updates",
						     &SV::low_priority_updates,
						     fix_low_priority_updates);
#endif
static sys_var_thd_ulong	sys_max_allowed_packet(&vars, "max_allowed_packet",
					       &SV::max_allowed_packet);
static sys_var_long_ptr	sys_max_binlog_cache_size(&vars, "max_binlog_cache_size",
						  &max_binlog_cache_size);
static sys_var_long_ptr	sys_max_binlog_size(&vars, "max_binlog_size",
					    &max_binlog_size,
                                            fix_max_binlog_size);
static sys_var_long_ptr	sys_max_connections(&vars, "max_connections",
					    &max_connections,
                                            fix_max_connections);
static sys_var_long_ptr	sys_max_connect_errors(&vars, "max_connect_errors",
					       &max_connect_errors);
static sys_var_thd_ulong       sys_max_insert_delayed_threads(&vars, "max_insert_delayed_threads",
						       &SV::max_insert_delayed_threads,
                                                       check_max_delayed_threads,
                                                       fix_max_connections);
static sys_var_thd_ulong	sys_max_delayed_threads(&vars, "max_delayed_threads",
						&SV::max_insert_delayed_threads,
                                                check_max_delayed_threads,
                                                fix_max_connections);
static sys_var_thd_ulong	sys_max_error_count(&vars, "max_error_count",
					    &SV::max_error_count);
static sys_var_thd_ulonglong	sys_max_heap_table_size(&vars, "max_heap_table_size",
						&SV::max_heap_table_size);
static sys_var_thd_ulong       sys_pseudo_thread_id(&vars, "pseudo_thread_id",
					     &SV::pseudo_thread_id,
                                             check_pseudo_thread_id, 0);
static sys_var_thd_ha_rows	sys_max_join_size(&vars, "max_join_size",
					  &SV::max_join_size,
					  fix_max_join_size);
static sys_var_thd_ulong	sys_max_seeks_for_key(&vars, "max_seeks_for_key",
					      &SV::max_seeks_for_key);
static sys_var_thd_ulong   sys_max_length_for_sort_data(&vars, "max_length_for_sort_data",
                                                 &SV::max_length_for_sort_data);
#ifndef TO_BE_DELETED	/* Alias for max_join_size */
static sys_var_thd_ha_rows	sys_sql_max_join_size(&vars, "sql_max_join_size",
					      &SV::max_join_size,
					      fix_max_join_size);
#endif
static sys_var_long_ptr_global
sys_max_prepared_stmt_count(&vars, "max_prepared_stmt_count",
                            &max_prepared_stmt_count,
                            &LOCK_prepared_stmt_count);
static sys_var_long_ptr	sys_max_relay_log_size(&vars, "max_relay_log_size",
                                               &max_relay_log_size,
                                               fix_max_relay_log_size);
static sys_var_thd_ulong	sys_max_sort_length(&vars, "max_sort_length",
					    &SV::max_sort_length);
static sys_var_thd_ulong	sys_max_sp_recursion_depth(&vars, "max_sp_recursion_depth",
                                                   &SV::max_sp_recursion_depth);
static sys_var_max_user_conn   sys_max_user_connections(&vars, "max_user_connections");
static sys_var_thd_ulong	sys_max_tmp_tables(&vars, "max_tmp_tables",
					   &SV::max_tmp_tables);
static sys_var_long_ptr	sys_max_write_lock_count(&vars, "max_write_lock_count",
						 &max_write_lock_count);
static sys_var_thd_ulong       sys_multi_range_count(&vars, "multi_range_count",
                                              &SV::multi_range_count);
static sys_var_long_ptr	sys_myisam_data_pointer_size(&vars, "myisam_data_pointer_size",
                                                    &myisam_data_pointer_size);
static sys_var_thd_ulonglong	sys_myisam_max_sort_file_size(&vars, "myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
static sys_var_thd_ulong       sys_myisam_repair_threads(&vars, "myisam_repair_threads", &SV::myisam_repair_threads);
static sys_var_thd_ulong	sys_myisam_sort_buffer_size(&vars, "myisam_sort_buffer_size", &SV::myisam_sort_buff_size);
static sys_var_bool_ptr	sys_myisam_use_mmap(&vars, "myisam_use_mmap",
                                            &opt_myisam_use_mmap);

static sys_var_thd_enum         sys_myisam_stats_method(&vars, "myisam_stats_method",
                                                &SV::myisam_stats_method,
                                                &myisam_stats_method_typelib,
                                                NULL);

static sys_var_thd_ulong	sys_net_buffer_length(&vars, "net_buffer_length",
					      &SV::net_buffer_length);
static sys_var_thd_ulong	sys_net_read_timeout(&vars, "net_read_timeout",
					     &SV::net_read_timeout,
					     0, fix_net_read_timeout);
static sys_var_thd_ulong	sys_net_write_timeout(&vars, "net_write_timeout",
					      &SV::net_write_timeout,
					      0, fix_net_write_timeout);
static sys_var_thd_ulong	sys_net_retry_count(&vars, "net_retry_count",
					    &SV::net_retry_count,
					    0, fix_net_retry_count);
static sys_var_thd_bool	sys_new_mode(&vars, "new", &SV::new_mode);
static sys_var_bool_ptr_readonly sys_old_mode(&vars, "old",
                                       &global_system_variables.old_mode);
/* these two cannot be static */
sys_var_thd_bool                sys_old_alter_table(&vars, "old_alter_table",
                                            &SV::old_alter_table);
sys_var_thd_bool                sys_old_passwords(&vars, "old_passwords", &SV::old_passwords);
static sys_var_thd_ulong        sys_optimizer_prune_level(&vars, "optimizer_prune_level",
                                                  &SV::optimizer_prune_level);
static sys_var_thd_ulong        sys_optimizer_search_depth(&vars, "optimizer_search_depth",
                                                   &SV::optimizer_search_depth);
static sys_var_thd_ulong        sys_preload_buff_size(&vars, "preload_buffer_size",
                                              &SV::preload_buff_size);
static sys_var_thd_ulong	sys_read_buff_size(&vars, "read_buffer_size",
					   &SV::read_buff_size);
static sys_var_opt_readonly	sys_readonly(&vars, "read_only", &opt_readonly);
static sys_var_thd_ulong	sys_read_rnd_buff_size(&vars, "read_rnd_buffer_size",
					       &SV::read_rnd_buff_size);
static sys_var_thd_ulong	sys_div_precincrement(&vars, "div_precision_increment",
                                              &SV::div_precincrement);
static sys_var_long_ptr	sys_rpl_recovery_rank(&vars, "rpl_recovery_rank",
					      &rpl_recovery_rank);
static sys_var_long_ptr	sys_query_cache_size(&vars, "query_cache_size",
					     &query_cache_size,
					     fix_query_cache_size);

static sys_var_thd_ulong	sys_range_alloc_block_size(&vars, "range_alloc_block_size",
						   &SV::range_alloc_block_size);
static sys_var_thd_ulong	sys_query_alloc_block_size(&vars, "query_alloc_block_size",
						   &SV::query_alloc_block_size,
						   0, fix_thd_mem_root);
static sys_var_thd_ulong	sys_query_prealloc_size(&vars, "query_prealloc_size",
						&SV::query_prealloc_size,
						0, fix_thd_mem_root);
static sys_var_readonly        sys_tmpdir(&vars, "tmpdir", OPT_GLOBAL, SHOW_CHAR, get_tmpdir);
static sys_var_thd_ulong	sys_trans_alloc_block_size(&vars, "transaction_alloc_block_size",
						   &SV::trans_alloc_block_size,
						   0, fix_trans_mem_root);
static sys_var_thd_ulong	sys_trans_prealloc_size(&vars, "transaction_prealloc_size",
						&SV::trans_prealloc_size,
						0, fix_trans_mem_root);
sys_var_thd_enum        sys_thread_handling(&vars, "thread_handling",
                                            &SV::thread_handling,
                                            &thread_handling_typelib,
                                            NULL);

#ifdef HAVE_QUERY_CACHE
static sys_var_long_ptr	sys_query_cache_limit(&vars, "query_cache_limit",
					      &query_cache.query_cache_limit);
static sys_var_long_ptr        sys_query_cache_min_res_unit(&vars, "query_cache_min_res_unit",
						     &query_cache_min_res_unit,
						     fix_query_cache_min_res_unit);
static sys_var_thd_enum	sys_query_cache_type(&vars, "query_cache_type",
					     &SV::query_cache_type,
					     &query_cache_type_typelib);
static sys_var_thd_bool
sys_query_cache_wlock_invalidate(&vars, "query_cache_wlock_invalidate",
				 &SV::query_cache_wlock_invalidate);
#endif /* HAVE_QUERY_CACHE */
static sys_var_bool_ptr	sys_secure_auth(&vars, "secure_auth", &opt_secure_auth);
static sys_var_const_str_ptr sys_secure_file_priv(&vars, "secure_file_priv",
                                             &opt_secure_file_priv);
static sys_var_long_ptr	sys_server_id(&vars, "server_id", &server_id, fix_server_id);
static sys_var_bool_ptr	sys_slave_compressed_protocol(&vars, "slave_compressed_protocol",
						      &opt_slave_compressed_protocol);
#ifdef HAVE_REPLICATION
static sys_var_bool_ptr         sys_slave_allow_batching(&vars, "slave_allow_batching",
                                                         &slave_allow_batching);
#endif
static sys_var_long_ptr	sys_slow_launch_time(&vars, "slow_launch_time",
					     &slow_launch_time);
static sys_var_thd_ulong	sys_sort_buffer(&vars, "sort_buffer_size",
					&SV::sortbuff_size);
static sys_var_thd_sql_mode    sys_sql_mode(&vars, "sql_mode",
                                     &SV::sql_mode);
#ifdef HAVE_OPENSSL
extern char *opt_ssl_ca, *opt_ssl_capath, *opt_ssl_cert, *opt_ssl_cipher,
            *opt_ssl_key;
static sys_var_const_str_ptr	sys_ssl_ca(&vars, "ssl_ca", &opt_ssl_ca);
static sys_var_const_str_ptr	sys_ssl_capath(&vars, "ssl_capath", &opt_ssl_capath);
static sys_var_const_str_ptr	sys_ssl_cert(&vars, "ssl_cert", &opt_ssl_cert);
static sys_var_const_str_ptr	sys_ssl_cipher(&vars, "ssl_cipher", &opt_ssl_cipher);
static sys_var_const_str_ptr	sys_ssl_key(&vars, "ssl_key", &opt_ssl_key);
#else
static sys_var_const_str	sys_ssl_ca(&vars, "ssl_ca", NULL);
static sys_var_const_str	sys_ssl_capath(&vars, "ssl_capath", NULL);
static sys_var_const_str	sys_ssl_cert(&vars, "ssl_cert", NULL);
static sys_var_const_str	sys_ssl_cipher(&vars, "ssl_cipher", NULL);
static sys_var_const_str	sys_ssl_key(&vars, "ssl_key", NULL);
#endif
static sys_var_thd_enum
sys_updatable_views_with_limit(&vars, "updatable_views_with_limit",
                               &SV::updatable_views_with_limit,
                               &updatable_views_with_limit_typelib);

static sys_var_thd_table_type  sys_table_type(&vars, "table_type",
				       &SV::table_plugin);
static sys_var_thd_storage_engine sys_storage_engine(&vars, "storage_engine",
				       &SV::table_plugin);
static sys_var_bool_ptr	sys_sync_frm(&vars, "sync_frm", &opt_sync_frm);
static sys_var_const_str	sys_system_time_zone(&vars, "system_time_zone",
                                             system_time_zone);
static sys_var_long_ptr	sys_table_def_size(&vars, "table_definition_cache",
                                           &table_def_size);
static sys_var_long_ptr	sys_table_cache_size(&vars, "table_open_cache",
					     &table_cache_size);
static sys_var_long_ptr	sys_table_lock_wait_timeout(&vars, "table_lock_wait_timeout",
                                                    &table_lock_wait_timeout);
static sys_var_long_ptr	sys_thread_cache_size(&vars, "thread_cache_size",
					      &thread_cache_size);
#if HAVE_POOL_OF_THREADS == 1
sys_var_long_ptr	sys_thread_pool_size(&vars, "thread_pool_size",
					      &thread_pool_size);
#endif
static sys_var_thd_enum	sys_tx_isolation(&vars, "tx_isolation",
					 &SV::tx_isolation,
					 &tx_isolation_typelib,
					 fix_tx_isolation,
					 check_tx_isolation);
static sys_var_thd_ulonglong	sys_tmp_table_size(&vars, "tmp_table_size",
					   &SV::tmp_table_size);
static sys_var_bool_ptr  sys_timed_mutexes(&vars, "timed_mutexes",
                                    &timed_mutexes);
static sys_var_const_str	sys_version(&vars, "version", server_version);
static sys_var_const_str	sys_version_comment(&vars, "version_comment",
                                            MYSQL_COMPILATION_COMMENT);
static sys_var_const_str	sys_version_compile_machine(&vars, "version_compile_machine",
                                                    MACHINE_TYPE);
static sys_var_const_str	sys_version_compile_os(&vars, "version_compile_os",
                                               SYSTEM_TYPE);
static sys_var_thd_ulong	sys_net_wait_timeout(&vars, "wait_timeout",
					     &SV::net_wait_timeout);

/* Condition pushdown to storage engine */
static sys_var_thd_bool
sys_engine_condition_pushdown(&vars, "engine_condition_pushdown",
			      &SV::engine_condition_pushdown);

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
/* ndb thread specific variable settings */
static sys_var_thd_ulong
sys_ndb_autoincrement_prefetch_sz(&vars, "ndb_autoincrement_prefetch_sz",
				  &SV::ndb_autoincrement_prefetch_sz);
static sys_var_thd_bool
sys_ndb_force_send(&vars, "ndb_force_send", &SV::ndb_force_send);
#ifdef HAVE_NDB_BINLOG
static sys_var_long_ptr
sys_ndb_report_thresh_binlog_epoch_slip(&vars, "ndb_report_thresh_binlog_epoch_slip",
                                        &ndb_report_thresh_binlog_epoch_slip);
static sys_var_long_ptr
sys_ndb_report_thresh_binlog_mem_usage(&vars, "ndb_report_thresh_binlog_mem_usage",
                                       &ndb_report_thresh_binlog_mem_usage);
static sys_var_bool_ptr
sys_ndb_log_update_as_write(&vars, "ndb_log_update_as_write", &opt_ndb_log_update_as_write);
#endif
static sys_var_thd_bool
sys_ndb_use_exact_count(&vars, "ndb_use_exact_count", &SV::ndb_use_exact_count);
static sys_var_thd_bool
sys_ndb_use_transactions(&vars, "ndb_use_transactions", &SV::ndb_use_transactions);
static sys_var_long_ptr
sys_ndb_cache_check_time(&vars, "ndb_cache_check_time", &ndb_cache_check_time);
static sys_var_const_str
sys_ndb_connectstring(&vars, "ndb_connectstring", opt_ndb_constrbuf);
static sys_var_thd_bool
sys_ndb_index_stat_enable(&vars, "ndb_index_stat_enable",
                          &SV::ndb_index_stat_enable);
static sys_var_thd_ulong
sys_ndb_index_stat_cache_entries(&vars, "ndb_index_stat_cache_entries",
                                 &SV::ndb_index_stat_cache_entries);
static sys_var_thd_ulong
sys_ndb_index_stat_update_freq(&vars, "ndb_index_stat_update_freq",
                               &SV::ndb_index_stat_update_freq);
static sys_var_long_ptr
sys_ndb_extra_logging(&vars, "ndb_extra_logging", &ndb_extra_logging);
static sys_var_thd_bool
sys_ndb_use_copying_alter_table(&vars, "ndb_use_copying_alter_table", &SV::ndb_use_copying_alter_table);
#endif //WITH_NDBCLUSTER_STORAGE_ENGINE

/* Time/date/datetime formats */

static sys_var_thd_date_time_format sys_time_format(&vars, "time_format",
					     &SV::time_format,
					     MYSQL_TIMESTAMP_TIME);
static sys_var_thd_date_time_format sys_date_format(&vars, "date_format",
					     &SV::date_format,
					     MYSQL_TIMESTAMP_DATE);
static sys_var_thd_date_time_format sys_datetime_format(&vars, "datetime_format",
						 &SV::datetime_format,
						 MYSQL_TIMESTAMP_DATETIME);

/* Variables that are bits in THD */

sys_var_thd_bit sys_autocommit(&vars, "autocommit", 0,
                               set_option_autocommit,
                               OPTION_NOT_AUTOCOMMIT,
                               1);
static sys_var_thd_bit	sys_big_tables(&vars, "big_tables", 0,
				       set_option_bit,
				       OPTION_BIG_TABLES);
#ifndef TO_BE_DELETED	/* Alias for big_tables */
static sys_var_thd_bit	sys_sql_big_tables(&vars, "sql_big_tables", 0,
					   set_option_bit,
					   OPTION_BIG_TABLES);
#endif
static sys_var_thd_bit	sys_big_selects(&vars, "sql_big_selects", 0,
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_thd_bit	sys_log_off(&vars, "sql_log_off",
				    check_log_update,
				    set_option_bit,
				    OPTION_LOG_OFF);
static sys_var_thd_bit	sys_log_update(&vars, "sql_log_update",
                                       check_log_update,
				       set_log_update,
				       OPTION_BIN_LOG);
static sys_var_thd_bit	sys_log_binlog(&vars, "sql_log_bin",
                                       check_log_update,
				       set_option_bit,
				       OPTION_BIN_LOG);
static sys_var_thd_bit	sys_sql_warnings(&vars, "sql_warnings", 0,
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_thd_bit	sys_sql_notes(&vars, "sql_notes", 0,
					 set_option_bit,
					 OPTION_SQL_NOTES);
static sys_var_thd_bit	sys_auto_is_null(&vars, "sql_auto_is_null", 0,
					 set_option_bit,
					 OPTION_AUTO_IS_NULL);
static sys_var_thd_bit	sys_safe_updates(&vars, "sql_safe_updates", 0,
					 set_option_bit,
					 OPTION_SAFE_UPDATES);
static sys_var_thd_bit	sys_buffer_results(&vars, "sql_buffer_result", 0,
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_thd_bit	sys_quote_show_create(&vars, "sql_quote_show_create", 0,
					      set_option_bit,
					      OPTION_QUOTE_SHOW_CREATE);
static sys_var_thd_bit	sys_foreign_key_checks(&vars, "foreign_key_checks", 0,
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS,
					       1);
static sys_var_thd_bit	sys_unique_checks(&vars, "unique_checks", 0,
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS,
					  1);

/* Local state variables */

static sys_var_thd_ha_rows	sys_select_limit(&vars, "sql_select_limit",
						 &SV::select_limit);
static sys_var_timestamp	sys_timestamp(&vars, "timestamp");
static sys_var_last_insert_id	sys_last_insert_id(&vars, "last_insert_id");
static sys_var_last_insert_id	sys_identity(&vars, "identity");

static sys_var_thd_lc_time_names       sys_lc_time_names(&vars, "lc_time_names");

static sys_var_insert_id	sys_insert_id(&vars, "insert_id");
static sys_var_readonly		sys_error_count(&vars, "error_count",
						OPT_SESSION,
						SHOW_LONG,
						get_error_count);
static sys_var_readonly		sys_warning_count(&vars, "warning_count",
						  OPT_SESSION,
						  SHOW_LONG,
						  get_warning_count);

/* alias for last_insert_id() to be compatible with Sybase */
static sys_var_rand_seed1	sys_rand_seed1(&vars, "rand_seed1");
static sys_var_rand_seed2	sys_rand_seed2(&vars, "rand_seed2");

static sys_var_thd_ulong        sys_default_week_format(&vars, "default_week_format",
					                &SV::default_week_format);

sys_var_thd_ulong               sys_group_concat_max_len(&vars, "group_concat_max_len",
                                                         &SV::group_concat_max_len);

sys_var_thd_time_zone            sys_time_zone(&vars, "time_zone");

/* Global read-only variable containing hostname */
static sys_var_const_str        sys_hostname(&vars, "hostname", glob_hostname);

/* Read only variables */

static sys_var_have_variable sys_have_compress(&vars, "have_compress", &have_compress);
static sys_var_have_variable sys_have_crypt(&vars, "have_crypt", &have_crypt);
static sys_var_have_plugin sys_have_csv(&vars, "have_csv", C_STRING_WITH_LEN("csv"), MYSQL_STORAGE_ENGINE_PLUGIN);
static sys_var_have_variable sys_have_dlopen(&vars, "have_dynamic_loading", &have_dlopen);
static sys_var_have_variable sys_have_geometry(&vars, "have_geometry", &have_geometry);
static sys_var_have_plugin sys_have_innodb(&vars, "have_innodb", C_STRING_WITH_LEN("innodb"), MYSQL_STORAGE_ENGINE_PLUGIN);
static sys_var_have_plugin sys_have_ndbcluster(&vars, "have_ndbcluster", C_STRING_WITH_LEN("ndbcluster"), MYSQL_STORAGE_ENGINE_PLUGIN);
static sys_var_have_variable sys_have_openssl(&vars, "have_openssl", &have_ssl);
static sys_var_have_variable sys_have_ssl(&vars, "have_ssl", &have_ssl);
static sys_var_have_plugin sys_have_partition_db(&vars, "have_partitioning", C_STRING_WITH_LEN("partition"), MYSQL_STORAGE_ENGINE_PLUGIN);
static sys_var_have_variable sys_have_query_cache(&vars, "have_query_cache",
                                           &have_query_cache);
static sys_var_have_variable sys_have_rtree_keys(&vars, "have_rtree_keys", &have_rtree_keys);
static sys_var_have_variable sys_have_symlink(&vars, "have_symlink", &have_symlink);
/* Global read-only variable describing server license */
static sys_var_const_str	sys_license(&vars, "license", STRINGIFY_ARG(LICENSE));
/* Global variables which enable|disable logging */
static sys_var_log_state sys_var_general_log(&vars, "general_log", &opt_log,
                                      QUERY_LOG_GENERAL);
static sys_var_log_state sys_var_slow_query_log(&vars, "slow_query_log", &opt_slow_log,
                                         QUERY_LOG_SLOW);
sys_var_str sys_var_general_log_path(&vars, "general_log_file", sys_check_log_path,
				     sys_update_general_log_path,
				     sys_default_general_log_path,
				     opt_logname);
sys_var_str sys_var_slow_log_path(&vars, "slow_query_log_file", sys_check_log_path,
				  sys_update_slow_log_path, 
				  sys_default_slow_log_path,
				  opt_slow_logname);
static sys_var_log_output sys_var_log_output_state(&vars, "log_output", &log_output_options,
					    &log_output_typelib, 0);


/*
  Additional variables (not derived from sys_var class, not accessible as
  @@varname in SELECT or SET). Sorted in alphabetical order to facilitate
  maintenance - SHOW VARIABLES will sort its output.
  TODO: remove this list completely
*/

#define FIXED_VARS_SIZE (sizeof(fixed_vars) / sizeof(SHOW_VAR))
static SHOW_VAR fixed_vars[]= {
  {"back_log",                (char*) &back_log,                    SHOW_LONG},
  {"character_sets_dir",      mysql_charsets_dir,                   SHOW_CHAR},
  {"ft_max_word_len",         (char*) &ft_max_word_len,             SHOW_LONG},
  {"ft_min_word_len",         (char*) &ft_min_word_len,             SHOW_LONG},
  {"ft_query_expansion_limit",(char*) &ft_query_expansion_limit,    SHOW_LONG},
  {"ft_stopword_file",        (char*) &ft_stopword_file,            SHOW_CHAR_PTR},
  {"init_file",               (char*) &opt_init_file,               SHOW_CHAR_PTR},
  {"language",                language,                             SHOW_CHAR},
  {"large_files_support",     (char*) &opt_large_files,             SHOW_BOOL},
  {"large_page_size",         (char*) &opt_large_page_size,         SHOW_INT},
  {"large_pages",             (char*) &opt_large_pages,             SHOW_MY_BOOL},
#ifdef HAVE_MLOCKALL
  {"locked_in_memory",	      (char*) &locked_in_memory,	    SHOW_MY_BOOL},
#endif
  {"log",                     (char*) &opt_log,                     SHOW_MY_BOOL},
  {"log_bin",                 (char*) &opt_bin_log,                 SHOW_BOOL},
  {"log_error",               (char*) log_error_file,               SHOW_CHAR},
  {"log_slow_queries",        (char*) &opt_slow_log,                SHOW_MY_BOOL},
  {"lower_case_file_system",  (char*) &lower_case_file_system,      SHOW_MY_BOOL},
  {"lower_case_table_names",  (char*) &lower_case_table_names,      SHOW_INT},
  {"myisam_recover_options",  (char*) &myisam_recover_options_str,  SHOW_CHAR_PTR},
#ifdef __NT__
  {"named_pipe",	      (char*) &opt_enable_named_pipe,       SHOW_MY_BOOL},
#endif
  {"open_files_limit",	      (char*) &open_files_limit,	    SHOW_LONG},
  {"pid_file",                (char*) pidfile_name,                 SHOW_CHAR},
  {"plugin_dir",              (char*) opt_plugin_dir,               SHOW_CHAR},
  {"port",                    (char*) &mysqld_port,                 SHOW_INT},
  {"protocol_version",        (char*) &protocol_version,            SHOW_INT},
#ifdef HAVE_SMEM
  {"shared_memory",           (char*) &opt_enable_shared_memory,    SHOW_MY_BOOL},
  {"shared_memory_base_name", (char*) &shared_memory_base_name,     SHOW_CHAR_PTR},
#endif
  {"skip_external_locking",   (char*) &my_disable_locking,          SHOW_MY_BOOL},
  {"skip_networking",         (char*) &opt_disable_networking,      SHOW_BOOL},
  {"skip_show_database",      (char*) &opt_skip_show_db,            SHOW_BOOL},
#ifdef HAVE_SYS_UN_H
  {"socket",                  (char*) &mysqld_unix_port,            SHOW_CHAR_PTR},
#endif
  {"table_definition_cache",  (char*) &table_def_size,              SHOW_LONG},
  {"table_lock_wait_timeout", (char*) &table_lock_wait_timeout,     SHOW_LONG },
#ifdef HAVE_THR_SETCONCURRENCY
  {"thread_concurrency",      (char*) &concurrency,                 SHOW_LONG},
#endif
  {"thread_stack",            (char*) &thread_stack,                SHOW_LONG},
};


bool sys_var::check(THD *thd, set_var *var)
{
  var->save_result.ulonglong_value= var->value->val_int();
  return 0;
}

bool sys_var_str::check(THD *thd, set_var *var)
{
  int res;
  if (!check_func)
    return 0;

  if ((res=(*check_func)(thd, var)) < 0)
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             name, var->value->str_value.ptr());
  return res;
}

/*
  Functions to check and update variables
*/


/*
  Update variables 'init_connect, init_slave'.

  In case of 'DEFAULT' value
  (for example: 'set GLOBAL init_connect=DEFAULT')
  'var' parameter is NULL pointer.
*/

bool update_sys_var_str(sys_var_str *var_str, rw_lock_t *var_mutex,
			set_var *var)
{
  char *res= 0, *old_value=(char *)(var ? var->value->str_value.ptr() : 0);
  uint new_length= (var ? var->value->str_value.length() : 0);
  if (!old_value)
    old_value= (char*) "";
  if (!(res= my_strndup(old_value, new_length, MYF(0))))
    return 1;
  /*
    Replace the old value in such a way that the any thread using
    the value will work.
  */
  rw_wrlock(var_mutex);
  old_value= var_str->value;
  var_str->value= res;
  var_str->value_length= new_length;
  rw_unlock(var_mutex);
  my_free(old_value, MYF(MY_ALLOW_ZERO_PTR));
  return 0;
}


static bool sys_update_init_connect(THD *thd, set_var *var)
{
  return update_sys_var_str(&sys_init_connect, &LOCK_sys_init_connect, var);
}


static void sys_default_init_connect(THD* thd, enum_var_type type)
{
  update_sys_var_str(&sys_init_connect, &LOCK_sys_init_connect, 0);
}


static bool sys_update_init_slave(THD *thd, set_var *var)
{
  return update_sys_var_str(&sys_init_slave, &LOCK_sys_init_slave, var);
}


static void sys_default_init_slave(THD* thd, enum_var_type type)
{
  update_sys_var_str(&sys_init_slave, &LOCK_sys_init_slave, 0);
}

static int sys_check_ftb_syntax(THD *thd,  set_var *var)
{
  if (thd->security_ctx->master_access & SUPER_ACL)
    return (ft_boolean_check_syntax_string((byte*)
                                           var->value->str_value.c_ptr()) ?
            -1 : 0);
  else
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return 1;
  }
}

static bool sys_update_ftb_syntax(THD *thd, set_var * var)
{
  strmake(ft_boolean_syntax, var->value->str_value.c_ptr(),
	  sizeof(ft_boolean_syntax)-1);

#ifdef HAVE_QUERY_CACHE
  query_cache.flush();
#endif /* HAVE_QUERY_CACHE */

  return 0;
}

static void sys_default_ftb_syntax(THD *thd, enum_var_type type)
{
  strmake(ft_boolean_syntax, def_ft_boolean_syntax,
	  sizeof(ft_boolean_syntax)-1);
}


/*
  If one sets the LOW_PRIORIY UPDATES flag, we also must change the
  used lock type
*/

static void fix_low_priority_updates(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->update_lock_default= (thd->variables.low_priority_updates ?
			       TL_WRITE_LOW_PRIORITY : TL_WRITE);
}


static void
fix_myisam_max_sort_file_size(THD *thd, enum_var_type type)
{
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;
}

/*
  Set the OPTION_BIG_SELECTS flag if max_join_size == HA_POS_ERROR
*/

static void fix_max_join_size(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    if (thd->variables.max_join_size == HA_POS_ERROR)
      thd->options|= OPTION_BIG_SELECTS;
    else
      thd->options&= ~OPTION_BIG_SELECTS;
  }
}


/*
  Can't change the 'next' tx_isolation while we are already in
  a transaction
*/
static int check_tx_isolation(THD *thd, set_var *var)
{
  if (var->type == OPT_DEFAULT && (thd->server_status & SERVER_STATUS_IN_TRANS))
  {
    my_error(ER_CANT_CHANGE_TX_ISOLATION, MYF(0));
    return 1;
  }
  return 0;
}

/*
  If one doesn't use the SESSION modifier, the isolation level
  is only active for the next command
*/
static void fix_tx_isolation(THD *thd, enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->session_tx_isolation= ((enum_tx_isolation)
				thd->variables.tx_isolation);
}

static void fix_completion_type(THD *thd __attribute__((unused)),
				enum_var_type type __attribute__((unused))) {}

static int check_completion_type(THD *thd, set_var *var)
{
  longlong val= var->value->val_int();
  if (val < 0 || val > 2)
  {
    char buf[64];
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->var->name, llstr(val, buf));
    return 1;
  }
  return 0;
}


/*
  If we are changing the thread variable, we have to copy it to NET too
*/

#ifdef HAVE_REPLICATION
static void fix_net_read_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    net_set_read_timeout(&thd->net, thd->variables.net_read_timeout);
}


static void fix_net_write_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    net_set_write_timeout(&thd->net, thd->variables.net_write_timeout);
}

static void fix_net_retry_count(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.retry_count=thd->variables.net_retry_count;
}
#else /* HAVE_REPLICATION */
static void fix_net_read_timeout(THD *thd __attribute__((unused)),
				 enum_var_type type __attribute__((unused)))
{}
static void fix_net_write_timeout(THD *thd __attribute__((unused)),
				  enum_var_type type __attribute__((unused)))
{}
static void fix_net_retry_count(THD *thd __attribute__((unused)),
				enum_var_type type __attribute__((unused)))
{}
#endif /* HAVE_REPLICATION */


static void fix_query_cache_size(THD *thd, enum_var_type type)
{
#ifdef HAVE_QUERY_CACHE
  ulong requested= query_cache_size;
  query_cache.resize(query_cache_size);
  if (requested != query_cache_size)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_QC_RESIZE, ER(ER_WARN_QC_RESIZE),
			requested, query_cache_size);
#endif
}


#ifdef HAVE_QUERY_CACHE
static void fix_query_cache_min_res_unit(THD *thd, enum_var_type type)
{
  query_cache_min_res_unit= 
    query_cache.set_min_res_unit(query_cache_min_res_unit);
}
#endif


extern void fix_delay_key_write(THD *thd, enum_var_type type)
{
  switch ((enum_delay_key_write) delay_key_write_options) {
  case DELAY_KEY_WRITE_NONE:
    myisam_delay_key_write=0;
    break;
  case DELAY_KEY_WRITE_ON:
    myisam_delay_key_write=1;
    break;
  case DELAY_KEY_WRITE_ALL:
    myisam_delay_key_write=1;
    ha_open_options|= HA_OPEN_DELAY_KEY_WRITE;
    break;
  }
}


bool sys_var_thd_binlog_format::is_readonly() const
{
  /*
    Under certain circumstances, the variable is read-only (unchangeable):
  */
  THD *thd= current_thd;
  /*
    If RBR and open temporary tables, their CREATE TABLE may not be in the
    binlog, so we can't toggle to SBR in this connection.
    The test below will also prevent SET GLOBAL, well it was not easy to test
    if global or not here.
    And this test will also prevent switching from RBR to RBR (a no-op which
    should not happen too often).

    If we don't have row-based replication compiled in, the variable
    is always read-only.
  */
  if ((thd->variables.binlog_format == BINLOG_FORMAT_ROW) &&
      thd->temporary_tables)
  {
    my_error(ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR, MYF(0));
    return 1;
  }
  /*
    if in a stored function/trigger, it's too late to change mode
  */
  if (thd->in_sub_stmt)
  {
    my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT, MYF(0));
    return 1;    
  }
#ifdef HAVE_NDB_BINLOG
  /*
    Cluster does not support changing the binlog format on the fly yet.
  */
  LEX_STRING ndb_name= {(char*)STRING_WITH_LEN("ndbcluster")};
  if (opt_bin_log && plugin_is_ready(&ndb_name, MYSQL_STORAGE_ENGINE_PLUGIN))
  {
    my_error(ER_NDB_CANT_SWITCH_BINLOG_FORMAT, MYF(0));
    return 1;
  }
#endif /* HAVE_NDB_BINLOG */
  return sys_var_thd_enum::is_readonly();
}


void fix_binlog_format_after_update(THD *thd, enum_var_type type)
{
  thd->reset_current_stmt_binlog_row_based();
}


static void fix_max_binlog_size(THD *thd, enum_var_type type)
{
  DBUG_ENTER("fix_max_binlog_size");
  DBUG_PRINT("info",("max_binlog_size=%lu max_relay_log_size=%lu",
                     max_binlog_size, max_relay_log_size));
  mysql_bin_log.set_max_size(max_binlog_size);
#ifdef HAVE_REPLICATION
  if (!max_relay_log_size)
    active_mi->rli.relay_log.set_max_size(max_binlog_size);
#endif
  DBUG_VOID_RETURN;
}

static void fix_max_relay_log_size(THD *thd, enum_var_type type)
{
  DBUG_ENTER("fix_max_relay_log_size");
  DBUG_PRINT("info",("max_binlog_size=%lu max_relay_log_size=%lu",
                     max_binlog_size, max_relay_log_size));
#ifdef HAVE_REPLICATION
  active_mi->rli.relay_log.set_max_size(max_relay_log_size ?
                                        max_relay_log_size: max_binlog_size);
#endif
  DBUG_VOID_RETURN;
}


static int check_max_delayed_threads(THD *thd, set_var *var)
{
  longlong val= var->value->val_int();
  if (var->type != OPT_GLOBAL && val != 0 &&
      val != (longlong) global_system_variables.max_insert_delayed_threads)
  {
    char buf[64];
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->var->name, llstr(val, buf));
    return 1;
  }
  return 0;
}

static void fix_max_connections(THD *thd, enum_var_type type)
{
#ifndef EMBEDDED_LIBRARY
  resize_thr_alarm(max_connections + 
		   global_system_variables.max_insert_delayed_threads + 10);
#endif
}


static void fix_thd_mem_root(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(thd->mem_root,
                        thd->variables.query_alloc_block_size,
                        thd->variables.query_prealloc_size);
}


static void fix_trans_mem_root(THD *thd, enum_var_type type)
{
#ifdef USING_TRANSACTIONS
  if (type != OPT_GLOBAL)
    reset_root_defaults(&thd->transaction.mem_root,
                        thd->variables.trans_alloc_block_size,
                        thd->variables.trans_prealloc_size);
#endif
}


static void fix_server_id(THD *thd, enum_var_type type)
{
  server_id_supplied = 1;
}


sys_var_long_ptr::
sys_var_long_ptr(sys_var_chain *chain, const char *name_arg, ulong *value_ptr_arg,
                 sys_after_update_func after_update_arg)
  :sys_var_long_ptr_global(chain, name_arg, value_ptr_arg,
                           &LOCK_global_system_variables, after_update_arg)
{}


bool sys_var_long_ptr_global::check(THD *thd, set_var *var)
{
  longlong v= var->value->val_int();
  var->save_result.ulonglong_value= v < 0 ? 0 : v;
  return 0;
}

bool sys_var_long_ptr_global::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(guard);
  if (option_limits)
    *value= (ulong) getopt_ull_limit_value(tmp, option_limits);
  else
    *value= (ulong) tmp;
  pthread_mutex_unlock(guard);
  return 0;
}


void sys_var_long_ptr_global::set_default(THD *thd, enum_var_type type)
{
  pthread_mutex_lock(guard);
  *value= (ulong) option_limits->def_value;
  pthread_mutex_unlock(guard);
}


bool sys_var_ulonglong_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (ulonglong) getopt_ull_limit_value(tmp, option_limits);
  else
    *value= (ulonglong) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_ulonglong_ptr::set_default(THD *thd, enum_var_type type)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= (ulonglong) option_limits->def_value;
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_bool_ptr::update(THD *thd, set_var *var)
{
  *value= (my_bool) var->save_result.ulong_value;
  return 0;
}


void sys_var_bool_ptr::set_default(THD *thd, enum_var_type type)
{
  *value= (my_bool) option_limits->def_value;
}


bool sys_var_enum::update(THD *thd, set_var *var)
{
  *value= (uint) var->save_result.ulong_value;
  return 0;
}


byte *sys_var_enum::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  return (byte*) enum_names->type_names[*value];
}

bool sys_var_thd_ulong::check(THD *thd, set_var *var)
{
  return (sys_var_thd::check(thd, var) ||
          (check_func && (*check_func)(thd, var)));
}

bool sys_var_thd_ulong::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ulong) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

#if SIZEOF_LONG == 4
  /* Avoid overflows on 32 bit systems */
  if (tmp > (ulonglong) ~(ulong) 0)
    tmp= ((ulonglong) ~(ulong) 0);
#endif

  if (option_limits)
    tmp= (ulong) getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) tmp;
  else
    thd->variables.*offset= (ulong) tmp;
  return 0;
}


void sys_var_thd_ulong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    /* We will not come here if option_limits is not set */
    global_system_variables.*offset= (ulong) option_limits->def_value;
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ulong::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_ha_rows::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ha_rows) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= (ha_rows) getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);    
    global_system_variables.*offset= (ha_rows) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ha_rows) tmp;
  return 0;
}


void sys_var_thd_ha_rows::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    /* We will not come here if option_limits is not set */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ha_rows) option_limits->def_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ha_rows::value_ptr(THD *thd, enum_var_type type,
				     LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}

bool sys_var_thd_ulonglong::update(THD *thd,  set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  if (tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ulonglong) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ulonglong) tmp;
  return 0;
}


void sys_var_thd_ulonglong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ulonglong) option_limits->def_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ulonglong::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_bool::update(THD *thd,  set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (my_bool) var->save_result.ulong_value;
  else
    thd->variables.*offset= (my_bool) var->save_result.ulong_value;
  return 0;
}


void sys_var_thd_bool::set_default(THD *thd,  enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (my_bool) option_limits->def_value;
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_bool::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var::check_enum(THD *thd, set_var *var, const TYPELIB *enum_names)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *value;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res=var->value->val_str(&str)) ||
	((long) (var->save_result.ulong_value=
		 (ulong) find_type(enum_names, res->ptr(),
				   res->length(),1)-1)) < 0)
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }
  }
  else
  {
    ulonglong tmp=var->value->val_int();
    if (tmp >= enum_names->count)
    {
      llstr(tmp,buff);
      value=buff;				// Wrong value is here
      goto err;
    }
    var->save_result.ulong_value= (ulong) tmp;	// Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, value);
  return 1;
}


bool sys_var::check_set(THD *thd, set_var *var, TYPELIB *enum_names)
{
  bool not_used;
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  uint error_len= 0;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res= var->value->val_str(&str)))
    {
      strmov(buff, "NULL");
      goto err;
    }
    var->save_result.ulong_value= ((ulong)
				   find_set(enum_names, res->c_ptr(),
					    res->length(),
                                            NULL,
                                            &error, &error_len,
					    &not_used));
    if (error_len)
    {
      strmake(buff, error, min(sizeof(buff), error_len));
      goto err;
    }
  }
  else
  {
    ulonglong tmp= var->value->val_int();
   /*
     For when the enum is made to contain 64 elements, as 1ULL<<64 is
     undefined, we guard with a "count<64" test.
   */
    if (unlikely((tmp >= ((ULL(1)) << enum_names->count)) &&
                 (enum_names->count < 64)))
    {
      llstr(tmp, buff);
      goto err;
    }
    var->save_result.ulong_value= (ulong) tmp;  // Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buff);
  return 1;
}


/*
  Return an Item for a variable.  Used with @@[global.]variable_name
  If type is not given, return local value if exists, else global
*/

Item *sys_var::item(THD *thd, enum_var_type var_type, LEX_STRING *base)
{
  if (check_type(var_type))
  {
    if (var_type != OPT_DEFAULT)
    {
      my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0),
               name, var_type == OPT_GLOBAL ? "SESSION" : "GLOBAL");
      return 0;
    }
    /* As there was no local variable, return the global value */
    var_type= OPT_GLOBAL;
  }
  switch (show_type()) {
  case SHOW_INT:
  {
    uint value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(uint*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_uint((ulonglong) value);
  }
  case SHOW_LONG:
  {
    ulong value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(ulong*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_uint((ulonglong) value);
  }
  case SHOW_LONGLONG:
  {
    longlong value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(longlong*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value);
  }
  case SHOW_HA_ROWS:
  {
    ha_rows value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(ha_rows*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int((longlong) value);
  }
  case SHOW_MY_BOOL:
  {
    int32 value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(my_bool*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value,1);
  }
  case SHOW_CHAR:
  {
    Item *tmp;
    pthread_mutex_lock(&LOCK_global_system_variables);
    char *str= (char*) value_ptr(thd, var_type, base);
    if (str)
      tmp= new Item_string(str, strlen(str),
                           system_charset_info, DERIVATION_SYSCONST);
    else
    {
      tmp= new Item_null();
      tmp->collation.set(system_charset_info, DERIVATION_SYSCONST);
    }
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return tmp;
  }
  default:
    my_error(ER_VAR_CANT_BE_READ, MYF(0), name);
  }
  return 0;
}


bool sys_var_thd_enum::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.ulong_value;
  else
    thd->variables.*offset= var->save_result.ulong_value;
  return 0;
}


void sys_var_thd_enum::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) option_limits->def_value;
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_enum::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  ulong tmp= ((type == OPT_GLOBAL) ?
	      global_system_variables.*offset :
	      thd->variables.*offset);
  return (byte*) enum_names->type_names[tmp];
}

bool sys_var_thd_bit::check(THD *thd, set_var *var)
{
  return (check_enum(thd, var, &bool_typelib) ||
          (check_func && (*check_func)(thd, var)));
}

bool sys_var_thd_bit::update(THD *thd, set_var *var)
{
  int res= (*update_func)(thd, var);
  return res;
}


byte *sys_var_thd_bit::value_ptr(THD *thd, enum_var_type type,
				 LEX_STRING *base)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  thd->sys_var_tmp.my_bool_value= ((thd->options & bit_flag) ?
				   !reverse : reverse);
  return (byte*) &thd->sys_var_tmp.my_bool_value;
}


/* Update a date_time format variable based on given value */

void sys_var_thd_date_time_format::update2(THD *thd, enum_var_type type,
					   DATE_TIME_FORMAT *new_value)
{
  DATE_TIME_FORMAT *old;
  DBUG_ENTER("sys_var_date_time_format::update2");
  DBUG_DUMP("positions",(char*) new_value->positions,
	    sizeof(new_value->positions));

  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    old= (global_system_variables.*offset);
    (global_system_variables.*offset)= new_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    old= (thd->variables.*offset);
    (thd->variables.*offset)= new_value;
  }
  my_free((char*) old, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_VOID_RETURN;
}


bool sys_var_thd_date_time_format::update(THD *thd, set_var *var)
{
  DATE_TIME_FORMAT *new_value;
  /* We must make a copy of the last value to get it into normal memory */
  new_value= date_time_format_copy((THD*) 0,
				   var->save_result.date_time_format);
  if (!new_value)
    return 1;					// Out of memory
  update2(thd, var->type, new_value);		// Can't fail
  return 0;
}


bool sys_var_thd_date_time_format::check(THD *thd, set_var *var)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  String str(buff,sizeof(buff), system_charset_info), *res;
  DATE_TIME_FORMAT *format;

  if (!(res=var->value->val_str(&str)))
    res= &my_empty_string;

  if (!(format= date_time_format_make(date_time_type,
				      res->ptr(), res->length())))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, res->c_ptr());
    return 1;
  }
  
  /*
    We must copy result to thread space to not get a memory leak if
    update is aborted
  */
  var->save_result.date_time_format= date_time_format_copy(thd, format);
  my_free((char*) format, MYF(0));
  return var->save_result.date_time_format == 0;
}


void sys_var_thd_date_time_format::set_default(THD *thd, enum_var_type type)
{
  DATE_TIME_FORMAT *res= 0;

  if (type == OPT_GLOBAL)
  {
    const char *format;
    if ((format= opt_date_time_formats[date_time_type]))
      res= date_time_format_make(date_time_type, format, strlen(format));
  }
  else
  {
    /* Make copy with malloc */
    res= date_time_format_copy((THD *) 0, global_system_variables.*offset);
  }

  if (res)					// Should always be true
    update2(thd, type, res);
}


byte *sys_var_thd_date_time_format::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
  {
    char *res;
    /*
      We do a copy here just to be sure things will work even if someone
      is modifying the original string while the copy is accessed
      (Can't happen now in SQL SHOW, but this is a good safety for the future)
    */
    res= thd->strmake((global_system_variables.*offset)->format.str,
		      (global_system_variables.*offset)->format.length);
    return (byte*) res;
  }
  return (byte*) (thd->variables.*offset)->format.str;
}


typedef struct old_names_map_st
{
  const char *old_name;
  const char *new_name;
} my_old_conv;

static my_old_conv old_conv[]= 
{
  {	"cp1251_koi8"		,	"cp1251"	},
  {	"cp1250_latin2"		,	"cp1250"	},
  {	"kam_latin2"		,	"keybcs2"	},
  {	"mac_latin2"		,	"MacRoman"	},
  {	"macce_latin2"		,	"MacCE"		},
  {	"pc2_latin2"		,	"pclatin2"	},
  {	"vga_latin2"		,	"pclatin1"	},
  {	"koi8_cp1251"		,	"koi8r"		},
  {	"win1251ukr_koi8_ukr"	,	"win1251ukr"	},
  {	"koi8_ukr_win1251ukr"	,	"koi8u"		},
  {	NULL			,	NULL		}
};

CHARSET_INFO *get_old_charset_by_name(const char *name)
{
  my_old_conv *conv;
 
  for (conv= old_conv; conv->old_name; conv++)
  {
    if (!my_strcasecmp(&my_charset_latin1, name, conv->old_name))
      return get_charset_by_csname(conv->new_name, MY_CS_PRIMARY, MYF(0));
  }
  return NULL;
}


bool sys_var_collation::check(THD *thd, set_var *var)
{
  CHARSET_INFO *tmp;
  LINT_INIT(tmp);

  if (var->value->result_type() == STRING_RESULT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;
    if (!(res=var->value->val_str(&str)))
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
      return 1;
    }
    if (!(tmp=get_charset_by_name(res->c_ptr(),MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), res->c_ptr());
      return 1;
    }
  }
  else // INT_RESULT
  {
    if (!(tmp=get_charset((int) var->value->val_int(),MYF(0))))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_error(ER_UNKNOWN_COLLATION, MYF(0), buf);
      return 1;
    }
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::check(THD *thd, set_var *var)
{
  CHARSET_INFO *tmp;
  LINT_INIT(tmp);

  if (var->value->result_type() == STRING_RESULT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;
    if (!(res=var->value->val_str(&str)))
    {
      if (!nullable)
      {
        my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
        return 1;
      }
      tmp= NULL;
    }
    else if (!(tmp=get_charset_by_csname(res->c_ptr(),MY_CS_PRIMARY,MYF(0))) &&
             !(tmp=get_old_charset_by_name(res->c_ptr())))
    {
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), res->c_ptr());
      return 1;
    }
  }
  else // INT_RESULT
  {
    if (!(tmp=get_charset((int) var->value->val_int(),MYF(0))))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), buf);
      return 1;
    }
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::update(THD *thd, set_var *var)
{
  ci_ptr(thd,var->type)[0]= var->save_result.charset;
  thd->update_charset();
  return 0;
}


byte *sys_var_character_set::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  CHARSET_INFO *cs= ci_ptr(thd,type)[0];
  return cs ? (byte*) cs->csname : (byte*) NULL;
}


void sys_var_character_set_sv::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= *global_default;
  else
  {
    thd->variables.*offset= global_system_variables.*offset;
    thd->update_charset();
  }
}
CHARSET_INFO **sys_var_character_set_sv::ci_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &(global_system_variables.*offset);
  else
    return &(thd->variables.*offset);
}


CHARSET_INFO ** sys_var_character_set_database::ci_ptr(THD *thd,
						       enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_database;
  else
    return &thd->variables.collation_database;
}


void sys_var_character_set_database::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
    global_system_variables.collation_database= default_charset_info;
  else
  {
    thd->variables.collation_database= thd->db_charset;
    thd->update_charset();
  }
}


bool sys_var_collation_sv::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.charset;
  else
  {
    thd->variables.*offset= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


void sys_var_collation_sv::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= *global_default;
  else
  {
    thd->variables.*offset= global_system_variables.*offset;
    thd->update_charset();
  }
}


byte *sys_var_collation_sv::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.*offset : thd->variables.*offset);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


LEX_STRING default_key_cache_base= {(char *) "default", 7 };

static KEY_CACHE zero_key_cache;

KEY_CACHE *get_key_cache(LEX_STRING *cache_name)
{
  safe_mutex_assert_owner(&LOCK_global_system_variables);
  if (!cache_name || ! cache_name->length)
    cache_name= &default_key_cache_base;
  return ((KEY_CACHE*) find_named(&key_caches,
                                      cache_name->str, cache_name->length, 0));
}


byte *sys_var_key_cache_param::value_ptr(THD *thd, enum_var_type type,
					 LEX_STRING *base)
{
  KEY_CACHE *key_cache= get_key_cache(base);
  if (!key_cache)
    key_cache= &zero_key_cache;
  return (byte*) key_cache + offset ;
}


bool sys_var_key_buffer_size::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  LEX_STRING *base_name= &var->base;
  KEY_CACHE *key_cache;
  bool error= 0;

  /* If no basename, assume it's for the key cache named 'default' */
  if (!base_name->length)
    base_name= &default_key_cache_base;

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache= get_key_cache(base_name);
                            
  if (!key_cache)
  {
    /* Key cache didn't exists */
    if (!tmp)					// Tried to delete cache
      goto end;					// Ok, nothing to do
    if (!(key_cache= create_key_cache(base_name->str, base_name->length)))
    {
      error= 1;
      goto end;
    }
  }

  /*
    Abort if some other thread is changing the key cache
    TODO: This should be changed so that we wait until the previous
    assignment is done and then do the new assign
  */
  if (key_cache->in_init)
    goto end;

  if (!tmp)					// Zero size means delete
  {
    if (key_cache == dflt_key_cache)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_CANT_DROP_DEFAULT_KEYCACHE,
                          ER(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE));
      goto end;					// Ignore default key cache
    }

    if (key_cache->key_cache_inited)		// If initied
    {
      /*
	Move tables using this key cache to the default key cache
	and clear the old key cache.
      */
      NAMED_LIST *list; 
      key_cache= (KEY_CACHE *) find_named(&key_caches, base_name->str,
					      base_name->length, &list);
      key_cache->in_init= 1;
      pthread_mutex_unlock(&LOCK_global_system_variables);
      error= reassign_keycache_tables(thd, key_cache, dflt_key_cache);
      pthread_mutex_lock(&LOCK_global_system_variables);
      key_cache->in_init= 0;
    }
    /*
      We don't delete the key cache as some running threads my still be
      in the key cache code with a pointer to the deleted (empty) key cache
    */
    goto end;
  }

  key_cache->param_buff_size=
    (ulonglong) getopt_ull_limit_value(tmp, option_limits);

  /* If key cache didn't existed initialize it, else resize it */
  key_cache->in_init= 1;
  pthread_mutex_unlock(&LOCK_global_system_variables);

  if (!key_cache->key_cache_inited)
    error= (bool) (ha_init_key_cache("", key_cache));
  else
    error= (bool)(ha_resize_key_cache(key_cache));

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;  

end:
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return error;
}


bool sys_var_key_cache_long::update(THD *thd, set_var *var)
{
  ulong tmp= (ulong) var->value->val_int();
  LEX_STRING *base_name= &var->base;
  bool error= 0;

  if (!base_name->length)
    base_name= &default_key_cache_base;

  pthread_mutex_lock(&LOCK_global_system_variables);
  KEY_CACHE *key_cache= get_key_cache(base_name);

  if (!key_cache && !(key_cache= create_key_cache(base_name->str,
				                  base_name->length)))
  {
    error= 1;
    goto end;
  }

  /*
    Abort if some other thread is changing the key cache
    TODO: This should be changed so that we wait until the previous
    assignment is done and then do the new assign
  */
  if (key_cache->in_init)
    goto end;

  *((ulong*) (((char*) key_cache) + offset))=
    (ulong) getopt_ull_limit_value(tmp, option_limits);

  /*
    Don't create a new key cache if it didn't exist
    (key_caches are created only when the user sets block_size)
  */
  key_cache->in_init= 1;

  pthread_mutex_unlock(&LOCK_global_system_variables);

  error= (bool) (ha_resize_key_cache(key_cache));

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;  

end:
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return error;
}


bool sys_var_log_state::update(THD *thd, set_var *var)
{
  bool res= 0;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!var->save_result.ulong_value)
    logger.deactivate_log_handler(thd, log_type);
  else
  {
    if ((res= logger.activate_log_handler(thd, log_type)))
    {
      my_error(ER_CANT_ACTIVATE_LOG, MYF(0),
               log_type == QUERY_LOG_GENERAL ? "general" :
               "slow query");
      goto err;
    }
  }
err:
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return res;
}

void sys_var_log_state::set_default(THD *thd, enum_var_type type)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.deactivate_log_handler(thd, log_type);
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


static int  sys_check_log_path(THD *thd,  set_var *var)
{
  char path[FN_REFLEN];
  MY_STAT f_stat;
  const char *var_path= var->value->str_value.ptr();
  bzero(&f_stat, sizeof(MY_STAT));

  (void) unpack_filename(path, var_path);
  if (my_stat(path, &f_stat, MYF(0)))
  {
    /* Check if argument is a file and we have 'write' permission */
    if (!MY_S_ISREG(f_stat.st_mode) ||
        !(f_stat.st_mode & MY_S_IWRITE))
      return -1;
  }
  else
  {
    /*
      Check if directory exists and 
      we have permission to create file & write to file
    */
    (void) dirname_part(path, var_path);
    if (my_access(path, (F_OK|W_OK)))
      return -1;
  }
  return 0;
}


bool update_sys_var_str_path(THD *thd, sys_var_str *var_str,
			     set_var *var, const char *log_ext,
			     bool log_state, uint log_type)
{
  MYSQL_QUERY_LOG *file_log;
  char buff[FN_REFLEN];
  char *res= 0, *old_value=(char *)(var ? var->value->str_value.ptr() : 0);
  bool result= 0;
  uint str_length= (var ? var->value->str_value.length() : 0);

  switch (log_type) {
  case QUERY_LOG_SLOW:
    file_log= logger.get_slow_log_file_handler();
    break;
  case QUERY_LOG_GENERAL:
    file_log= logger.get_log_file_handler();
    break;
  default:
    assert(0);                                  // Impossible
  }

  if (!old_value)
  {
    old_value= make_default_log_name(buff, log_ext);
    str_length= strlen(old_value);
  }
  if (!(res= my_strndup(old_value, str_length, MYF(MY_FAE+MY_WME))))
  {
    result= 1;
    goto err;
  }

  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock();

  if (file_log && log_state)
    file_log->close(0);
  old_value= var_str->value;
  var_str->value= res;
  var_str->value_length= str_length;
  my_free(old_value, MYF(MY_ALLOW_ZERO_PTR));
  if (file_log && log_state)
  {
    switch (log_type) {
    case QUERY_LOG_SLOW:
      file_log->open_slow_log(sys_var_slow_log_path.value);
      break;
    case QUERY_LOG_GENERAL:
      file_log->open_query_log(sys_var_general_log_path.value);
      break;
    default:
      DBUG_ASSERT(0);
    }
  }

  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);

err:
  return result;
}


static bool sys_update_general_log_path(THD *thd, set_var * var)
{
  return update_sys_var_str_path(thd, &sys_var_general_log_path, 
				 var, ".log", opt_log, QUERY_LOG_GENERAL);
}


static void sys_default_general_log_path(THD *thd, enum_var_type type)
{
  (void) update_sys_var_str_path(thd, &sys_var_general_log_path,
				 0, ".log", opt_log, QUERY_LOG_GENERAL);
}


static bool sys_update_slow_log_path(THD *thd, set_var * var)
{
  return update_sys_var_str_path(thd, &sys_var_slow_log_path,
				 var, "-slow.log", opt_slow_log,
                                 QUERY_LOG_SLOW);
}


static void sys_default_slow_log_path(THD *thd, enum_var_type type)
{
  (void) update_sys_var_str_path(thd, &sys_var_slow_log_path,
				 0, "-slow.log", opt_slow_log,
                                 QUERY_LOG_SLOW);
}


bool sys_var_log_output::update(THD *thd, set_var *var)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock();
  logger.init_slow_log(var->save_result.ulong_value);
  logger.init_general_log(var->save_result.ulong_value);
  *value= var->save_result.ulong_value;
  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_log_output::set_default(THD *thd, enum_var_type type)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock();
  logger.init_slow_log(LOG_TABLE);
  logger.init_general_log(LOG_TABLE);
  *value= LOG_TABLE;
  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


byte *sys_var_log_output::value_ptr(THD *thd, enum_var_type type,
                                    LEX_STRING *base)
{
  char buff[256];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  ulong length;
  ulong val= *value;

  tmp.length(0);
  for (uint i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(log_output_typelib.type_names[i],
                 log_output_typelib.type_lengths[i]);
      tmp.append(',');
    }
  }

  if ((length= tmp.length()))
    length--;
  return (byte*) thd->strmake(tmp.ptr(), length);
}


/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(THD *thd)
{
  return 0;
}

int set_var_collation_client::update(THD *thd)
{
  thd->variables.character_set_client= character_set_client;
  thd->variables.character_set_results= character_set_results;
  thd->variables.collation_connection= collation_connection;
  thd->update_charset();
  thd->protocol_text.init(thd);
  thd->protocol_binary.init(thd);
  return 0;
}

/****************************************************************************/

bool sys_var_timestamp::update(THD *thd,  set_var *var)
{
  thd->set_time((time_t) var->save_result.ulonglong_value);
  return 0;
}


void sys_var_timestamp::set_default(THD *thd, enum_var_type type)
{
  thd->user_time=0;
}


byte *sys_var_timestamp::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  thd->sys_var_tmp.long_value= (long) thd->start_time;
  return (byte*) &thd->sys_var_tmp.long_value;
}


bool sys_var_last_insert_id::update(THD *thd, set_var *var)
{
  thd->first_successful_insert_id_in_prev_stmt= 
    var->save_result.ulonglong_value;
  return 0;
}


byte *sys_var_last_insert_id::value_ptr(THD *thd, enum_var_type type,
					LEX_STRING *base)
{
  /*
    this tmp var makes it robust againt change of type of 
    read_first_successful_insert_id_in_prev_stmt().
  */
  thd->sys_var_tmp.ulonglong_value= 
    thd->read_first_successful_insert_id_in_prev_stmt();
  return (byte*) &thd->sys_var_tmp.ulonglong_value;
}


bool sys_var_insert_id::update(THD *thd, set_var *var)
{
  thd->force_one_auto_inc_interval(var->save_result.ulonglong_value);
  return 0;
}


byte *sys_var_insert_id::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  thd->sys_var_tmp.ulonglong_value= 
    thd->auto_inc_intervals_forced.minimum();
  return (byte*) &thd->sys_var_tmp.ulonglong_value;
}


bool sys_var_rand_seed1::update(THD *thd, set_var *var)
{
  thd->rand.seed1= (ulong) var->save_result.ulonglong_value;
  return 0;
}

bool sys_var_rand_seed2::update(THD *thd, set_var *var)
{
  thd->rand.seed2= (ulong) var->save_result.ulonglong_value;
  return 0;
}


bool sys_var_thd_time_zone::check(THD *thd, set_var *var)
{
  char buff[MAX_TIME_ZONE_NAME_LENGTH];
  String str(buff, sizeof(buff), &my_charset_latin1);
  String *res= var->value->val_str(&str);

  if (!(var->save_result.time_zone= my_tz_find(thd, res)))
  {
    my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), res ? res->c_ptr() : "NULL");
    return 1;
  }
  return 0;
}


bool sys_var_thd_time_zone::update(THD *thd, set_var *var)
{
  /* We are using Time_zone object found during check() phase. */
  if (var->type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.time_zone= var->save_result.time_zone;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.time_zone= var->save_result.time_zone;
  return 0;
}


byte *sys_var_thd_time_zone::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  /* 
    We can use ptr() instead of c_ptr() here because String contaning
    time zone name is guaranteed to be zero ended.
  */
  if (type == OPT_GLOBAL)
    return (byte *)(global_system_variables.time_zone->get_name()->ptr());
  else
  {
    /*
      This is an ugly fix for replication: we don't replicate properly queries
      invoking system variables' values to update tables; but
      CONVERT_TZ(,,@@session.time_zone) is so popular that we make it
      replicable (i.e. we tell the binlog code to store the session
      timezone). If it's the global value which was used we can't replicate
      (binlog code stores session value only).
    */
    thd->time_zone_used= 1;
    return (byte *)(thd->variables.time_zone->get_name()->ptr());
  }
}


void sys_var_thd_time_zone::set_default(THD *thd, enum_var_type type)
{
 pthread_mutex_lock(&LOCK_global_system_variables);
 if (type == OPT_GLOBAL)
 {
   if (default_tz_name)
   {
     String str(default_tz_name, &my_charset_latin1);
     /*
       We are guaranteed to find this time zone since its existence
       is checked during start-up.
     */
     global_system_variables.time_zone= my_tz_find(thd, &str);
   }
   else
     global_system_variables.time_zone= my_tz_SYSTEM;
 }
 else
   thd->variables.time_zone= global_system_variables.time_zone;
 pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_max_user_conn::check(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    return sys_var_thd::check(thd, var);
  else
  {
    /*
      Per-session values of max_user_connections can't be set directly.
      May be we should have a separate error message for this?
    */
    my_error(ER_GLOBAL_VARIABLE, MYF(0), name);
    return TRUE;
  }
}

bool sys_var_max_user_conn::update(THD *thd, set_var *var)
{
  DBUG_ASSERT(var->type == OPT_GLOBAL);
  pthread_mutex_lock(&LOCK_global_system_variables);
  max_user_connections= (uint)var->save_result.ulonglong_value;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_max_user_conn::set_default(THD *thd, enum_var_type type)
{
  DBUG_ASSERT(type == OPT_GLOBAL);
  pthread_mutex_lock(&LOCK_global_system_variables);
  max_user_connections= (ulong) option_limits->def_value;
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


byte *sys_var_max_user_conn::value_ptr(THD *thd, enum_var_type type,
                                       LEX_STRING *base)
{
  if (type != OPT_GLOBAL &&
      thd->user_connect && thd->user_connect->user_resources.user_conn)
    return (byte*) &(thd->user_connect->user_resources.user_conn);
  return (byte*) &(max_user_connections);
}


bool sys_var_thd_lc_time_names::check(THD *thd, set_var *var)
{
  MY_LOCALE *locale_match;

  if (var->value->result_type() == INT_RESULT)
  {
    if (!(locale_match= my_locale_by_number((uint) var->value->val_int())))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_printf_error(ER_UNKNOWN_ERROR, "Unknown locale: '%s'", MYF(0), buf);
      return 1;
    }
  }
  else // STRING_RESULT
  {
    char buff[6]; 
    String str(buff, sizeof(buff), &my_charset_latin1), *res;
    if (!(res=var->value->val_str(&str)))
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
      return 1;
    }
    const char *locale_str= res->c_ptr();
    if (!(locale_match= my_locale_by_name(locale_str)))
    {
      my_printf_error(ER_UNKNOWN_ERROR,
                      "Unknown locale: '%s'", MYF(0), locale_str);
      return 1;
    }
  }

  var->save_result.locale_value= locale_match;
  return 0;
}


bool sys_var_thd_lc_time_names::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.lc_time_names= var->save_result.locale_value;
  else
    thd->variables.lc_time_names= var->save_result.locale_value;
  return 0;
}


byte *sys_var_thd_lc_time_names::value_ptr(THD *thd, enum_var_type type,
					  LEX_STRING *base)
{
  return type == OPT_GLOBAL ?
                 (byte *) global_system_variables.lc_time_names->name :
                 (byte *) thd->variables.lc_time_names->name;
}


void sys_var_thd_lc_time_names::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.lc_time_names= my_default_lc_time_names;
  else
    thd->variables.lc_time_names= global_system_variables.lc_time_names;
}

/*
  Functions to update thd->options bits
*/

static bool set_option_bit(THD *thd, set_var *var)
{
  sys_var_thd_bit *sys_var= ((sys_var_thd_bit*) var->var);
  if ((var->save_result.ulong_value != 0) == sys_var->reverse)
    thd->options&= ~sys_var->bit_flag;
  else
    thd->options|= sys_var->bit_flag;
  return 0;
}


static bool set_option_autocommit(THD *thd, set_var *var)
{
  /* The test is negative as the flag we use is NOT autocommit */

  ulonglong org_options= thd->options;

  if (var->save_result.ulong_value != 0)
    thd->options&= ~((sys_var_thd_bit*) var->var)->bit_flag;
  else
    thd->options|= ((sys_var_thd_bit*) var->var)->bit_flag;

  if ((org_options ^ thd->options) & OPTION_NOT_AUTOCOMMIT)
  {
    if ((org_options & OPTION_NOT_AUTOCOMMIT))
    {
      /* We changed to auto_commit mode */
      thd->options&= ~(ulonglong) (OPTION_BEGIN | OPTION_KEEP_LOG);
      thd->no_trans_update.all= FALSE;
      thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
      if (ha_commit(thd))
	return 1;
    }
    else
    {
      thd->no_trans_update.all= FALSE;
      thd->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    }
  }
  return 0;
}

static int check_log_update(THD *thd, set_var *var)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!(thd->security_ctx->master_access & SUPER_ACL))
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return 1;
  }
#endif
  return 0;
}

static bool set_log_update(THD *thd, set_var *var)
{
  /*
    The update log is not supported anymore since 5.0.
    See sql/mysqld.cc/, comments in function init_server_components() for an
    explaination of the different warnings we send below
  */

  if (opt_sql_bin_update)
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_UPDATE_LOG_DEPRECATED_TRANSLATED,
                 ER(ER_UPDATE_LOG_DEPRECATED_TRANSLATED));
  }
  else
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_UPDATE_LOG_DEPRECATED_IGNORED,
                 ER(ER_UPDATE_LOG_DEPRECATED_IGNORED));
  set_option_bit(thd, var);
  return 0;
}


static int check_pseudo_thread_id(THD *thd, set_var *var)
{
  var->save_result.ulonglong_value= var->value->val_int();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (thd->security_ctx->master_access & SUPER_ACL)
    return 0;
  else
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return 1;
  }
#else
  return 0;
#endif
}

static byte *get_warning_count(THD *thd)
{
  thd->sys_var_tmp.long_value=
    (thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_NOTE] +
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR] +
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_WARN]);
  return (byte*) &thd->sys_var_tmp.long_value;
}

static byte *get_error_count(THD *thd)
{
  thd->sys_var_tmp.long_value= 
    thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR];
  return (byte*) &thd->sys_var_tmp.long_value;
}


/*
  Get the tmpdir that was specified or chosen by default

  SYNOPSIS
    get_tmpdir()
    thd		thread handle

  DESCRIPTION
    This is necessary because if the user does not specify a temporary
    directory via the command line, one is chosen based on the environment
    or system defaults.  But we can't just always use mysql_tmpdir, because
    that is actually a call to my_tmpdir() which cycles among possible
    temporary directories.

  RETURN VALUES
    ptr		pointer to NUL-terminated string
 */
static byte *get_tmpdir(THD *thd)
{
  if (opt_mysql_tmpdir)
    return (byte *)opt_mysql_tmpdir;
  return (byte*)mysql_tmpdir;
}

/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/*
  Find variable name in option my_getopt structure used for command line args

  SYNOPSIS
    find_option()
    opt		option structure array to search in
    name	variable name

  RETURN VALUES
    0		Error
    ptr		pointer to option structure
*/

static struct my_option *find_option(struct my_option *opt, const char *name) 
{
  uint length=strlen(name);
  for (; opt->name; opt++)
  {
    if (!getopt_compare_strings(opt->name, name, length) &&
	!opt->name[length])
    {
      /*
	Only accept the option if one can set values through it.
	If not, there is no default value or limits in the option.
      */
      return (opt->value) ? opt : 0;
    }
  }
  return 0;
}


/*
  Return variable name and length for hashing of variables
*/

static byte *get_sys_var_length(const sys_var *var, uint *length,
				my_bool first)
{
  *length= var->name_length;
  return (byte*) var->name;
}


/*
  Add variables to the dynamic hash of system variables
  
  SYNOPSIS
    mysql_add_sys_var_chain()
    first       Pointer to first system variable to add
    long_opt    (optional)command line arguments may be tied for limit checks.
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int mysql_add_sys_var_chain(sys_var *first, struct my_option *long_options)
{
  sys_var *var;
  
  /* A write lock should be held on LOCK_system_variables_hash */
  
  for (var= first; var; var= var->next)
  {
    var->name_length= strlen(var->name);
    /* this fails if there is a conflicting variable name. see HASH_UNIQUE */
    if (my_hash_insert(&system_variable_hash, (byte*) var))
      goto error;
    if (long_options)
      var->option_limits= find_option(long_options, var->name);
  }
  return 0;

error:
  for (; first != var; first= first->next)
    hash_delete(&system_variable_hash, (byte*) first);
  return 1;
}


/*
  Remove variables to the dynamic hash of system variables
  
  SYNOPSIS
    mysql_del_sys_var_chain()
    first       Pointer to first system variable to remove
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int mysql_del_sys_var_chain(sys_var *first)
{
  int result= 0;

  /* A write lock should be held on LOCK_system_variables_hash */
  
  for (sys_var *var= first; var; var= var->next)
    result|= hash_delete(&system_variable_hash, (byte*) var);

  return result;
}


static int show_cmp(SHOW_VAR *a, SHOW_VAR *b)
{
  return strcmp(a->name, b->name);
}


/*
  Constructs an array of system variables for display to the user.
  
  SYNOPSIS
    enumerate_sys_vars()
    thd         current thread
    sorted      If TRUE, the system variables should be sorted
  
  RETURN VALUES
    pointer     Array of SHOW_VAR elements for display
    NULL        FAILURE
*/

SHOW_VAR* enumerate_sys_vars(THD *thd, bool sorted)
{
  int count= system_variable_hash.records, i;
  int fixed_count= fixed_show_vars.elements;
  int size= sizeof(SHOW_VAR) * (count + fixed_count + 1);
  SHOW_VAR *result= (SHOW_VAR*) thd->alloc(size);

  if (result)
  {
    SHOW_VAR *show= result + fixed_count;
    memcpy(result, fixed_show_vars.buffer, fixed_count * sizeof(SHOW_VAR));

    for (i= 0; i < count; i++)
    {
      sys_var *var= (sys_var*) hash_element(&system_variable_hash, i);
      show->name= var->name;
      show->value= (char*) var;
      show->type= SHOW_SYS;
      show++;
    }

    /* sort into order */
    if (sorted)
      qsort(result, count + fixed_count, sizeof(SHOW_VAR), (qsort_cmp)show_cmp);
    
    /* make last element empty */
    bzero(show, sizeof(SHOW_VAR));
  }
  return result;
}


/*
  Initialize the system variables
  
  SYNOPSIS
    set_var_init()
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int set_var_init()
{
  uint count= 0;
  DBUG_ENTER("set_var_init");
  
  for (sys_var *var=vars.first; var; var= var->next, count++);

  if (my_init_dynamic_array(&fixed_show_vars, sizeof(SHOW_VAR),
                            FIXED_VARS_SIZE + 64, 64))
    goto error;

  fixed_show_vars.elements= FIXED_VARS_SIZE;
  memcpy(fixed_show_vars.buffer, fixed_vars, sizeof(fixed_vars));

  if (hash_init(&system_variable_hash, system_charset_info, count, 0,
                0, (hash_get_key) get_sys_var_length, 0, HASH_UNIQUE))
    goto error;

  vars.last->next= NULL;
  if (mysql_add_sys_var_chain(vars.first, my_long_options))
    goto error;

  /*
    Special cases
    Needed because MySQL can't find the limits for a variable it it has
    a different name than the command line option.
    As these variables are deprecated, this code will disappear soon...
  */
  sys_sql_max_join_size.option_limits= sys_max_join_size.option_limits;

  DBUG_RETURN(0);

error:
  fprintf(stderr, "failed to initialize system variables");
  DBUG_RETURN(1);
}


void set_var_free()
{
  hash_free(&system_variable_hash);
  delete_dynamic(&fixed_show_vars);
}


/*
  Add elements to the dynamic list of read-only system variables.
  
  SYNOPSIS
    mysql_append_static_vars()
    show_vars	Pointer to start of array
    count       Number of elements
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/
int mysql_append_static_vars(const SHOW_VAR *show_vars, uint count)
{
  for (; count > 0; count--, show_vars++)
    if (insert_dynamic(&fixed_show_vars, (char*) show_vars))
      return 1;
  return 0;
}


/*
  Find a user set-table variable

  SYNOPSIS
    intern_find_sys_var()
    str		Name of system variable to find
    length	Length of variable.  zero means that we should use strlen()
		on the variable

  RETURN VALUES
    pointer	pointer to variable definitions
    0		Unknown variable (error message is given)
*/

sys_var *intern_find_sys_var(const char *str, uint length, bool no_error)
{
  sys_var *var;

  /*
    This function is only called from the sql_plugin.cc.
    A lock on LOCK_system_variable_hash should be held
  */
  var= (sys_var*) hash_search(&system_variable_hash,
			      (byte*) str, length ? length : strlen(str));
  if (!(var || no_error))
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);

  return var;
}


/*
  Execute update of all variables

  SYNOPSIS

  sql_set
    THD		Thread id
    set_var	List of variables to update

  DESCRIPTION
    First run a check of all variables that all updates will go ok.
    If yes, then execute all updates, returning an error if any one failed.

    This should ensure that in all normal cases none all or variables are
    updated

    RETURN VALUE
    0	ok
    1	ERROR, message sent (normally no variables was updated)
    -1  ERROR, message not sent
*/

int sql_set_variables(THD *thd, List<set_var_base> *var_list)
{
  int error;
  List_iterator_fast<set_var_base> it(*var_list);
  DBUG_ENTER("sql_set_variables");

  set_var_base *var;
  while ((var=it++))
  {
    if ((error= var->check(thd)))
      goto err;
  }
  if (!(error= test(thd->net.report_error)))
  {
    it.rewind();
    while ((var= it++))
      error|= var->update(thd);         // Returns 0, -1 or 1
  }

err:
  free_underlaid_joins(thd, &thd->lex->select_lex);
  DBUG_RETURN(error);
}


/*
  Say if all variables set by a SET support the ONE_SHOT keyword (currently,
  only character set and collation do; later timezones will).

  SYNOPSIS

  not_all_support_one_shot
    set_var	List of variables to update

  NOTES
    It has a "not_" because it makes faster tests (no need to "!")

    RETURN VALUE
    0	all variables of the list support ONE_SHOT
    1	at least one does not support ONE_SHOT
*/

bool not_all_support_one_shot(List<set_var_base> *var_list)
{
  List_iterator_fast<set_var_base> it(*var_list);
  set_var_base *var;
  while ((var= it++))
  {
    if (var->no_support_one_shot())
      return 1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

int set_var::check(THD *thd)
{
  if (var->is_readonly())
  {
    my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name, "read only");
    return -1;
  }
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name);
    return -1;
  }
  if ((type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL)))
    return 1;
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (!value)
  {
    if (var->check_default(type))
    {
      my_error(ER_NO_DEFAULT, MYF(0), var->name);
      return -1;
    }
    return 0;
  }

  if ((!value->fixed &&
       value->fix_fields(thd, &value)) || value->check_cols(1))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name);
    return -1;
  }
  return var->check(thd, this) ? -1 : 0;
}


/*
  Check variable, but without assigning value (used by PS)

  SYNOPSIS
    set_var::light_check()
    thd		thread handler

  RETURN VALUE
    0	ok
    1	ERROR, message sent (normally no variables was updated)
    -1  ERROR, message not sent
*/
int set_var::light_check(THD *thd)
{
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name);
    return -1;
  }
  if (type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL))
    return 1;

  if (value && ((!value->fixed && value->fix_fields(thd, &value)) ||
                value->check_cols(1)))
    return -1;
  return 0;
}


int set_var::update(THD *thd)
{
  if (!value)
    var->set_default(thd, type);
  else if (var->update(thd, this))
    return -1;				// should never happen
  if (var->after_update)
    (*var->after_update)(thd, type);
  return 0;
}


/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::check(THD *thd)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(thd, (Item**) 0) ||
	  user_var_item->check(0)) ? -1 : 0;
}


/*
  Check variable, but without assigning value (used by PS)

  SYNOPSIS
    set_var_user::light_check()
    thd		thread handler

  RETURN VALUE
    0	ok
    1	ERROR, message sent (normally no variables was updated)
    -1  ERROR, message not sent
*/
int set_var_user::light_check(THD *thd)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(thd, (Item**) 0));
}


int set_var_user::update(THD *thd)
{
  if (user_var_item->update())
  {
    /* Give an error if it's not given already */
    my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY), MYF(0));
    return -1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

int set_var_password::check(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!user->host.str)
  {
    if (*thd->security_ctx->priv_host != 0)
    {
      user->host.str= (char *) thd->security_ctx->priv_host;
      user->host.length= strlen(thd->security_ctx->priv_host);
    }
    else
    {
      user->host.str= (char *)"%";
      user->host.length= 1;
    }
  }
  /* Returns 1 as the function sends error to client */
  return check_change_password(thd, user->host.str, user->user.str,
                               password, strlen(password)) ? 1 : 0;
#else
  return 0;
#endif
}

int set_var_password::update(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Returns 1 as the function sends error to client */
  return change_password(thd, user->host.str, user->user.str, password) ?
	  1 : 0;
#else
  return 0;
#endif
}

/****************************************************************************
 Functions to handle table_type
****************************************************************************/

/* Based upon sys_var::check_enum() */

bool sys_var_thd_storage_engine::check(THD *thd, set_var *var)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *value;
  String str(buff, sizeof(buff), &my_charset_latin1), *res;

  var->save_result.plugin= NULL;
  if (var->value->result_type() == STRING_RESULT)
  {
    LEX_STRING name;
    handlerton *hton;
    if (!(res=var->value->val_str(&str)) ||
        !(name.str= (char *)res->ptr()) || !(name.length= res->length()) ||
	!(var->save_result.plugin= ha_resolve_by_name(thd, &name)) ||
        !(hton= plugin_data(var->save_result.plugin, handlerton *)) ||
        ha_checktype(thd, ha_legacy_type(hton), 1, 0) != hton)
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }
    return 0;
  }
  value= "unknown";

err:
  my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), value);
  return 1;
}


byte *sys_var_thd_storage_engine::value_ptr(THD *thd, enum_var_type type,
					    LEX_STRING *base)
{
  byte* result;
  handlerton *hton;
  LEX_STRING *name;
  plugin_ref plugin= thd->variables.*offset;
  if (type == OPT_GLOBAL)
    plugin= my_plugin_lock(thd, &(global_system_variables.*offset));
  hton= plugin_data(plugin, handlerton*);
  name= &hton2plugin[hton->slot]->name;
  result= (byte *) thd->strmake(name->str, name->length);
  if (type == OPT_GLOBAL)
    plugin_unlock(thd, plugin);
  return result;
}


void sys_var_thd_storage_engine::set_default(THD *thd, enum_var_type type)
{
  plugin_ref old_value, new_value, *value;
  if (type == OPT_GLOBAL)
  {
    value= &(global_system_variables.*offset);
    new_value= ha_lock_engine(NULL, myisam_hton);
  }
  else
  {
    value= &(thd->variables.*offset);
    new_value= my_plugin_lock(NULL, &(global_system_variables.*offset));
  }
  DBUG_ASSERT(new_value);
  old_value= *value;
  *value= new_value;
  plugin_unlock(NULL, old_value);
}


bool sys_var_thd_storage_engine::update(THD *thd, set_var *var)
{
  plugin_ref *value= &(global_system_variables.*offset), old_value;
   if (var->type != OPT_GLOBAL)
     value= &(thd->variables.*offset);
  old_value= *value;
  if (old_value != var->save_result.plugin)
  {
    *value= my_plugin_lock(NULL, &var->save_result.plugin);
    plugin_unlock(NULL, old_value);
  }
  return 0;
}

void sys_var_thd_table_type::warn_deprecated(THD *thd)
{
  WARN_DEPRECATED(thd, "5.2", "table_type", "'storage_engine'");
}

void sys_var_thd_table_type::set_default(THD *thd, enum_var_type type)
{
  warn_deprecated(thd);
  sys_var_thd_storage_engine::set_default(thd, type);
}

bool sys_var_thd_table_type::update(THD *thd, set_var *var)
{
  warn_deprecated(thd);
  return sys_var_thd_storage_engine::update(thd, var);
}


/****************************************************************************
 Functions to handle sql_mode
****************************************************************************/

/*
  Make string representation of mode

  SYNOPSIS
    thd   in  thread handler
    val   in  sql_mode value
    rep   out pointer pointer to string with sql_mode representation
*/

bool
sys_var_thd_sql_mode::
symbolic_mode_representation(THD *thd, ulonglong val, LEX_STRING *rep)
{
  char buff[STRING_BUFFER_USUAL_SIZE*8];
  String tmp(buff, sizeof(buff), &my_charset_latin1);

  tmp.length(0);

  for (uint i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(sql_mode_typelib.type_names[i],
                 sql_mode_typelib.type_lengths[i]);
      tmp.append(',');
    }
  }

  if (tmp.length())
    tmp.length(tmp.length() - 1); /* trim the trailing comma */

  rep->str= thd->strmake(tmp.ptr(), tmp.length());

  rep->length= rep->str ? tmp.length() : 0;

  return rep->length != tmp.length();
}


byte *sys_var_thd_sql_mode::value_ptr(THD *thd, enum_var_type type,
				      LEX_STRING *base)
{
  LEX_STRING sql_mode;
  ulonglong val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
                  thd->variables.*offset);
  (void) symbolic_mode_representation(thd, val, &sql_mode);
  return (byte *) sql_mode.str;
}


void sys_var_thd_sql_mode::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= 0;
  else
    thd->variables.*offset= global_system_variables.*offset;
}


void fix_sql_mode_var(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.sql_mode=
      fix_sql_mode(global_system_variables.sql_mode);
  else
  {
    thd->variables.sql_mode= fix_sql_mode(thd->variables.sql_mode);
    /*
      Update thd->server_status
     */
    if (thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
      thd->server_status|= SERVER_STATUS_NO_BACKSLASH_ESCAPES;
    else
      thd->server_status&= ~SERVER_STATUS_NO_BACKSLASH_ESCAPES;
  }
}

/* Map database specific bits to function bits */

ulong fix_sql_mode(ulong sql_mode)
{
  /*
    Note that we dont set 
    MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS | MODE_NO_FIELD_OPTIONS
    to allow one to get full use of MySQL in this mode.
  */

  if (sql_mode & MODE_ANSI)
  {
    sql_mode|= (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE);
    /* 
      MODE_ONLY_FULL_GROUP_BY removed from ANSI mode because it is currently
      overly restrictive (see BUG#8510).
    */
  }
  if (sql_mode & MODE_ORACLE)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS | MODE_NO_AUTO_CREATE_USER);
  if (sql_mode & MODE_MSSQL)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_POSTGRESQL)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_DB2)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_MAXDB)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS | MODE_NO_AUTO_CREATE_USER);
  if (sql_mode & MODE_MYSQL40)
    sql_mode|= MODE_HIGH_NOT_PRECEDENCE;
  if (sql_mode & MODE_MYSQL323)
    sql_mode|= MODE_HIGH_NOT_PRECEDENCE;
  if (sql_mode & MODE_TRADITIONAL)
    sql_mode|= (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES |
                MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE |
                MODE_ERROR_FOR_DIVISION_BY_ZERO | MODE_NO_AUTO_CREATE_USER);
  return sql_mode;
}


/****************************************************************************
  Named list handling
****************************************************************************/

gptr find_named(I_List<NAMED_LIST> *list, const char *name, uint length,
		NAMED_LIST **found)
{
  I_List_iterator<NAMED_LIST> it(*list);
  NAMED_LIST *element;
  while ((element= it++))
  {
    if (element->cmp(name, length))
    {
      if (found)
        *found= element;
      return element->data;
    }
  }
  return 0;
}


void delete_elements(I_List<NAMED_LIST> *list,
		     void (*free_element)(const char *name, gptr))
{
  NAMED_LIST *element;
  DBUG_ENTER("delete_elements");
  while ((element= list->get()))
  {
    (*free_element)(element->name, element->data);
    delete element;
  }
  DBUG_VOID_RETURN;
}


/* Key cache functions */

static KEY_CACHE *create_key_cache(const char *name, uint length)
{
  KEY_CACHE *key_cache;
  DBUG_ENTER("create_key_cache");
  DBUG_PRINT("enter",("name: %.*s", length, name));
  
  if ((key_cache= (KEY_CACHE*) my_malloc(sizeof(KEY_CACHE),
					     MYF(MY_ZEROFILL | MY_WME))))
  {
    if (!new NAMED_LIST(&key_caches, name, length, (gptr) key_cache))
    {
      my_free((char*) key_cache, MYF(0));
      key_cache= 0;
    }
    else
    {
      /*
	Set default values for a key cache
	The values in dflt_key_cache_var is set by my_getopt() at startup

	We don't set 'buff_size' as this is used to enable the key cache
      */
      key_cache->param_block_size=     dflt_key_cache_var.param_block_size;
      key_cache->param_division_limit= dflt_key_cache_var.param_division_limit;
      key_cache->param_age_threshold=  dflt_key_cache_var.param_age_threshold;
    }
  }
  DBUG_RETURN(key_cache);
}


KEY_CACHE *get_or_create_key_cache(const char *name, uint length)
{
  LEX_STRING key_cache_name;
  KEY_CACHE *key_cache;

  key_cache_name.str= (char *) name;
  key_cache_name.length= length;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(&key_cache_name)))
    key_cache= create_key_cache(name, length);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return key_cache;
}


void free_key_cache(const char *name, KEY_CACHE *key_cache)
{
  ha_end_key_cache(key_cache);
  my_free((char*) key_cache, MYF(0));
}


bool process_key_caches(int (* func) (const char *name, KEY_CACHE *))
{
  I_List_iterator<NAMED_LIST> it(key_caches);
  NAMED_LIST *element;

  while ((element= it++))
  {
    KEY_CACHE *key_cache= (KEY_CACHE *) element->data;
    func(element->name, key_cache);
  }
  return 0;
}


void sys_var_trust_routine_creators::warn_deprecated(THD *thd)
{
  WARN_DEPRECATED(thd, "5.2", "log_bin_trust_routine_creators",
                      "'log_bin_trust_function_creators'");
}

void sys_var_trust_routine_creators::set_default(THD *thd, enum_var_type type)
{
  warn_deprecated(thd);
  sys_var_bool_ptr::set_default(thd, type);
}

bool sys_var_trust_routine_creators::update(THD *thd, set_var *var)
{
  warn_deprecated(thd);
  return sys_var_bool_ptr::update(thd, var);
}

bool sys_var_opt_readonly::update(THD *thd, set_var *var)
{
  bool result;

  DBUG_ENTER("sys_var_opt_readonly::update");

  /* Prevent self dead-lock */
  if (thd->locked_tables || thd->active_transaction())
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(true);
  }

  if (thd->global_read_lock)
  {
    /*
      This connection already holds the global read lock.
      This can be the case with:
      - FLUSH TABLES WITH READ LOCK
      - SET GLOBAL READ_ONLY = 1
    */
    result= sys_var_bool_ptr::update(thd, var);
    DBUG_RETURN(result);
  }

  /*
    Perform a 'FLUSH TABLES WITH READ LOCK'.
    This is a 3 step process:
    - [1] lock_global_read_lock()
    - [2] close_cached_tables()
    - [3] make_global_read_lock_block_commit()
    [1] prevents new connections from obtaining tables locked for write.
    [2] waits until all existing connections close their tables.
    [3] prevents transactions from being committed.
  */

  if (lock_global_read_lock(thd))
    DBUG_RETURN(true);

  /*
    This call will be blocked by any connection holding a READ or WRITE lock.
    Ideally, we want to wait only for pending WRITE locks, but since:
    con 1> LOCK TABLE T FOR READ;
    con 2> LOCK TABLE T FOR WRITE; (blocked by con 1)
    con 3> SET GLOBAL READ ONLY=1; (blocked by con 2)
    can cause to wait on a read lock, it's required for the client application
    to unlock everything, and acceptable for the server to wait on all locks.
  */
  if (result= close_cached_tables(thd, true, NULL, false))
    goto end_with_read_lock;

  if (result= make_global_read_lock_block_commit(thd))
    goto end_with_read_lock;

  /* Change the opt_readonly system variable, safe because the lock is held */
  result= sys_var_bool_ptr::update(thd, var);

end_with_read_lock:
  /* Release the lock */
  unlock_global_read_lock(thd);
  DBUG_RETURN(result);
}


/* even session variable here requires SUPER, because of -#o,file */
bool sys_var_thd_dbug::check(THD *thd, set_var *var)
{
  return check_global_access(thd, SUPER_ACL);
}

bool sys_var_thd_dbug::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    DBUG_SET_INITIAL(var ? var->value->str_value.c_ptr() : "");
  else
  {
    DBUG_POP();
    DBUG_PUSH(var ? var->value->str_value.c_ptr() : "");
  }
  return 0;
}


byte *sys_var_thd_dbug::value_ptr(THD *thd, enum_var_type type, LEX_STRING *b)
{
  char buf[256];
  if (type == OPT_GLOBAL)
    DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
  else
    DBUG_EXPLAIN(buf, sizeof(buf));
  return (byte*) thd->strdup(buf);
}


bool sys_var_event_scheduler::check(THD *thd, set_var *var)
{
  return check_enum(thd, var, &Events::var_typelib);
}


/*
   The update method of the global variable event_scheduler.
   If event_scheduler is switched from 0 to 1 then the scheduler main
   thread is resumed and if from 1 to 0 the scheduler thread is suspended

   SYNOPSIS
     sys_var_event_scheduler::update()
       thd  Thread context (unused)
       var  The new value

   Returns
     FALSE  OK
     TRUE   Error
*/

bool
sys_var_event_scheduler::update(THD *thd, set_var *var)
{
  int res;
  /* here start the thread if not running. */
  DBUG_ENTER("sys_var_event_scheduler::update");
  DBUG_PRINT("info", ("new_value: %d", (int) var->save_result.ulong_value));

  enum Events::enum_opt_event_scheduler
    new_state=
    (enum Events::enum_opt_event_scheduler) var->save_result.ulong_value;

  res= Events::switch_event_scheduler_state(new_state);

  DBUG_RETURN((bool) res);
}


byte *sys_var_event_scheduler::value_ptr(THD *thd, enum_var_type type,
                                         LEX_STRING *base)
{
  return (byte *) Events::get_opt_event_scheduler_str();
}


/****************************************************************************
  Used templates
****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<set_var_base>;
template class List_iterator_fast<set_var_base>;
template class I_List_iterator<NAMED_LIST>;
#endif

/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
  - If the variable should be changeable or one should be able to access it
    with @@variable_name, it should be added to the 'list of all variables'
    list (sys_variables) in this file.
  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - If the variable should be changed from the command line, add a definition
    of it in the my_option structure list in mysqld.cc
  - Don't forget to initialize new fields in global_system_variables and
    max_system_variables!
  - If the variable should show up in 'show variables' add it to the
    init_vars[] struct in this file

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
#include <my_getopt.h>
#include <thr_alarm.h>
#include <myisam.h>
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif
#ifdef HAVE_NDBCLUSTER_DB
#include "ha_ndbcluster.h"
#endif

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

static int sys_check_charset(THD *thd, set_var *var);
static bool sys_update_charset(THD *thd, set_var *var);
static void sys_set_default_charset(THD *thd, enum_var_type type);
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
static bool set_log_bin(THD *thd, set_var *var);
static void fix_low_priority_updates(THD *thd, enum_var_type type);
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
static byte *get_have_innodb(THD *thd);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order
*/

sys_var_thd_ulong	sys_auto_increment_increment("auto_increment_increment",
                                                     &SV::auto_increment_increment);
sys_var_thd_ulong	sys_auto_increment_offset("auto_increment_offset",
                                                  &SV::auto_increment_offset);

sys_var_bool_ptr	sys_automatic_sp_privileges("automatic_sp_privileges",
					      &sp_automatic_privileges);

sys_var_long_ptr	sys_binlog_cache_size("binlog_cache_size",
					      &binlog_cache_size);
sys_var_thd_ulong	sys_bulk_insert_buff_size("bulk_insert_buffer_size",
						  &SV::bulk_insert_buff_size);
sys_var_character_set_server	sys_character_set_server("character_set_server");
sys_var_const_str       sys_charset_system("character_set_system",
                                           (char *)my_charset_utf8_general_ci.name);
sys_var_character_set_database	sys_character_set_database("character_set_database");
sys_var_character_set_client  sys_character_set_client("character_set_client");
sys_var_character_set_connection  sys_character_set_connection("character_set_connection");
sys_var_character_set_results sys_character_set_results("character_set_results");
sys_var_thd_ulong	sys_completion_type("completion_type",
					 &SV::completion_type,
					 check_completion_type,
					 fix_completion_type);
sys_var_collation_connection sys_collation_connection("collation_connection");
sys_var_collation_database sys_collation_database("collation_database");
sys_var_collation_server sys_collation_server("collation_server");
sys_var_long_ptr	sys_concurrent_insert("concurrent_insert",
                                              &myisam_concurrent_insert);
sys_var_long_ptr	sys_connect_timeout("connect_timeout",
					    &connect_timeout);
sys_var_enum		sys_delay_key_write("delay_key_write",
					    &delay_key_write_options,
					    &delay_key_write_typelib,
					    fix_delay_key_write);
sys_var_long_ptr	sys_delayed_insert_limit("delayed_insert_limit",
						 &delayed_insert_limit);
sys_var_long_ptr	sys_delayed_insert_timeout("delayed_insert_timeout",
						   &delayed_insert_timeout);
sys_var_long_ptr	sys_delayed_queue_size("delayed_queue_size",
					       &delayed_queue_size);
sys_var_long_ptr	sys_expire_logs_days("expire_logs_days",
					     &expire_logs_days);
sys_var_bool_ptr	sys_flush("flush", &myisam_flush);
sys_var_long_ptr	sys_flush_time("flush_time", &flush_time);
sys_var_str             sys_ft_boolean_syntax("ft_boolean_syntax",
                                         sys_check_ftb_syntax,
                                         sys_update_ftb_syntax,
                                         sys_default_ftb_syntax,
                                         ft_boolean_syntax);
sys_var_str             sys_init_connect("init_connect", 0,
                                         sys_update_init_connect,
                                         sys_default_init_connect,0);
sys_var_str             sys_init_slave("init_slave", 0,
                                       sys_update_init_slave,
                                       sys_default_init_slave,0);
sys_var_thd_ulong	sys_interactive_timeout("interactive_timeout",
						&SV::net_interactive_timeout);
sys_var_thd_ulong	sys_join_buffer_size("join_buffer_size",
					     &SV::join_buff_size);
sys_var_key_buffer_size	sys_key_buffer_size("key_buffer_size");
sys_var_key_cache_long  sys_key_cache_block_size("key_cache_block_size",
						 offsetof(KEY_CACHE,
							  param_block_size));
sys_var_key_cache_long	sys_key_cache_division_limit("key_cache_division_limit",
						     offsetof(KEY_CACHE,
							      param_division_limit));
sys_var_key_cache_long  sys_key_cache_age_threshold("key_cache_age_threshold",
						     offsetof(KEY_CACHE,
							      param_age_threshold));
sys_var_bool_ptr	sys_local_infile("local_infile",
					 &opt_local_infile);
sys_var_bool_ptr       
sys_trust_routine_creators("log_bin_trust_routine_creators",
                           &trust_routine_creators);
sys_var_thd_ulong	sys_log_warnings("log_warnings", &SV::log_warnings);
sys_var_thd_ulong	sys_long_query_time("long_query_time",
					     &SV::long_query_time);
sys_var_thd_bool	sys_low_priority_updates("low_priority_updates",
						 &SV::low_priority_updates,
						 fix_low_priority_updates);
#ifndef TO_BE_DELETED	/* Alias for the low_priority_updates */
sys_var_thd_bool	sys_sql_low_priority_updates("sql_low_priority_updates",
						     &SV::low_priority_updates,
						     fix_low_priority_updates);
#endif
sys_var_thd_ulong	sys_max_allowed_packet("max_allowed_packet",
					       &SV::max_allowed_packet);
sys_var_long_ptr	sys_max_binlog_cache_size("max_binlog_cache_size",
						  &max_binlog_cache_size);
sys_var_long_ptr	sys_max_binlog_size("max_binlog_size",
					    &max_binlog_size,
                                            fix_max_binlog_size);
sys_var_long_ptr	sys_max_connections("max_connections",
					    &max_connections,
                                            fix_max_connections);
sys_var_long_ptr	sys_max_connect_errors("max_connect_errors",
					       &max_connect_errors);
sys_var_thd_ulong       sys_max_insert_delayed_threads("max_insert_delayed_threads",
						       &SV::max_insert_delayed_threads,
                                                       check_max_delayed_threads,
                                                       fix_max_connections);
sys_var_thd_ulong	sys_max_delayed_threads("max_delayed_threads",
						&SV::max_insert_delayed_threads,
                                                check_max_delayed_threads,
                                                fix_max_connections);
sys_var_thd_ulong	sys_max_error_count("max_error_count",
					    &SV::max_error_count);
sys_var_thd_ulong	sys_max_heap_table_size("max_heap_table_size",
						&SV::max_heap_table_size);
sys_var_thd_ulong       sys_pseudo_thread_id("pseudo_thread_id",
					     &SV::pseudo_thread_id,
                                             check_pseudo_thread_id, 0);
sys_var_thd_ha_rows	sys_max_join_size("max_join_size",
					  &SV::max_join_size,
					  fix_max_join_size);
sys_var_thd_ulong	sys_max_seeks_for_key("max_seeks_for_key",
					      &SV::max_seeks_for_key);
sys_var_thd_ulong   sys_max_length_for_sort_data("max_length_for_sort_data",
                                                 &SV::max_length_for_sort_data);
#ifndef TO_BE_DELETED	/* Alias for max_join_size */
sys_var_thd_ha_rows	sys_sql_max_join_size("sql_max_join_size",
					      &SV::max_join_size,
					      fix_max_join_size);
#endif
sys_var_long_ptr	sys_max_relay_log_size("max_relay_log_size",
                                               &max_relay_log_size,
                                               fix_max_relay_log_size);
sys_var_thd_ulong	sys_max_sort_length("max_sort_length",
					    &SV::max_sort_length);
sys_var_max_user_conn   sys_max_user_connections("max_user_connections");
sys_var_thd_ulong	sys_max_tmp_tables("max_tmp_tables",
					   &SV::max_tmp_tables);
sys_var_long_ptr	sys_max_write_lock_count("max_write_lock_count",
						 &max_write_lock_count);
sys_var_thd_ulong       sys_multi_range_count("multi_range_count",
                                              &SV::multi_range_count);
sys_var_long_ptr	sys_myisam_data_pointer_size("myisam_data_pointer_size",
                                                    &myisam_data_pointer_size);
sys_var_thd_ulonglong	sys_myisam_max_sort_file_size("myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
sys_var_thd_ulong       sys_myisam_repair_threads("myisam_repair_threads", &SV::myisam_repair_threads);
sys_var_thd_ulong	sys_myisam_sort_buffer_size("myisam_sort_buffer_size", &SV::myisam_sort_buff_size);

sys_var_thd_enum        sys_myisam_stats_method("myisam_stats_method",
                                                &SV::myisam_stats_method,
                                                &myisam_stats_method_typelib,
                                                NULL);

sys_var_thd_ulong	sys_net_buffer_length("net_buffer_length",
					      &SV::net_buffer_length);
sys_var_thd_ulong	sys_net_read_timeout("net_read_timeout",
					     &SV::net_read_timeout,
					     0, fix_net_read_timeout);
sys_var_thd_ulong	sys_net_write_timeout("net_write_timeout",
					      &SV::net_write_timeout,
					      0, fix_net_write_timeout);
sys_var_thd_ulong	sys_net_retry_count("net_retry_count",
					    &SV::net_retry_count,
					    0, fix_net_retry_count);
sys_var_thd_bool	sys_new_mode("new", &SV::new_mode);
sys_var_thd_bool	sys_old_alter_table("old_alter_table",
					    &SV::old_alter_table);
sys_var_thd_bool	sys_old_passwords("old_passwords", &SV::old_passwords);
sys_var_thd_ulong       sys_optimizer_prune_level("optimizer_prune_level",
                                                  &SV::optimizer_prune_level);
sys_var_thd_ulong       sys_optimizer_search_depth("optimizer_search_depth",
                                                   &SV::optimizer_search_depth);
sys_var_thd_ulong       sys_preload_buff_size("preload_buffer_size",
                                              &SV::preload_buff_size);
sys_var_thd_ulong	sys_read_buff_size("read_buffer_size",
					   &SV::read_buff_size);
sys_var_bool_ptr	sys_readonly("read_only", &opt_readonly);
sys_var_thd_ulong	sys_read_rnd_buff_size("read_rnd_buffer_size",
					       &SV::read_rnd_buff_size);
sys_var_thd_ulong	sys_div_precincrement("div_precision_increment",
                                              &SV::div_precincrement);
#ifdef HAVE_REPLICATION
sys_var_bool_ptr	sys_relay_log_purge("relay_log_purge",
                                            &relay_log_purge);
#endif
sys_var_long_ptr	sys_rpl_recovery_rank("rpl_recovery_rank",
					      &rpl_recovery_rank);
sys_var_long_ptr	sys_query_cache_size("query_cache_size",
					     &query_cache_size,
					     fix_query_cache_size);

sys_var_thd_ulong	sys_range_alloc_block_size("range_alloc_block_size",
						   &SV::range_alloc_block_size);
sys_var_thd_ulong	sys_query_alloc_block_size("query_alloc_block_size",
						   &SV::query_alloc_block_size,
						   0, fix_thd_mem_root);
sys_var_thd_ulong	sys_query_prealloc_size("query_prealloc_size",
						&SV::query_prealloc_size,
						0, fix_thd_mem_root);
sys_var_thd_ulong	sys_trans_alloc_block_size("transaction_alloc_block_size",
						   &SV::trans_alloc_block_size,
						   0, fix_trans_mem_root);
sys_var_thd_ulong	sys_trans_prealloc_size("transaction_prealloc_size",
						&SV::trans_prealloc_size,
						0, fix_trans_mem_root);

#ifdef HAVE_QUERY_CACHE
sys_var_long_ptr	sys_query_cache_limit("query_cache_limit",
					      &query_cache.query_cache_limit);
sys_var_long_ptr        sys_query_cache_min_res_unit("query_cache_min_res_unit",
						     &query_cache_min_res_unit,
						     fix_query_cache_min_res_unit);
sys_var_thd_enum	sys_query_cache_type("query_cache_type",
					     &SV::query_cache_type,
					     &query_cache_type_typelib);
sys_var_thd_bool
sys_query_cache_wlock_invalidate("query_cache_wlock_invalidate",
				 &SV::query_cache_wlock_invalidate);
#endif /* HAVE_QUERY_CACHE */
sys_var_bool_ptr	sys_secure_auth("secure_auth", &opt_secure_auth);
sys_var_long_ptr	sys_server_id("server_id", &server_id, fix_server_id);
sys_var_bool_ptr	sys_slave_compressed_protocol("slave_compressed_protocol",
						      &opt_slave_compressed_protocol);
#ifdef HAVE_REPLICATION
sys_var_long_ptr	sys_slave_net_timeout("slave_net_timeout",
					      &slave_net_timeout);
sys_var_long_ptr	sys_slave_trans_retries("slave_transaction_retries",
                                                &slave_trans_retries);
#endif
sys_var_long_ptr	sys_slow_launch_time("slow_launch_time",
					     &slow_launch_time);
sys_var_thd_ulong	sys_sort_buffer("sort_buffer_size",
					&SV::sortbuff_size);
sys_var_thd_sql_mode    sys_sql_mode("sql_mode",
                                     &SV::sql_mode);
sys_var_thd_enum
sys_updatable_views_with_limit("updatable_views_with_limit",
                               &SV::updatable_views_with_limit,
                               &updatable_views_with_limit_typelib);

sys_var_thd_table_type  sys_table_type("table_type",
				       &SV::table_type);
sys_var_thd_storage_engine sys_storage_engine("storage_engine",
				       &SV::table_type);
#ifdef HAVE_REPLICATION
sys_var_sync_binlog_period sys_sync_binlog_period("sync_binlog", &sync_binlog_period);
sys_var_thd_ulong	sys_sync_replication("sync_replication",
                                               &SV::sync_replication);
sys_var_thd_ulong	sys_sync_replication_slave_id(
						"sync_replication_slave_id",
                                               &SV::sync_replication_slave_id);
sys_var_thd_ulong	sys_sync_replication_timeout(
						"sync_replication_timeout",
                                               &SV::sync_replication_timeout);
#endif
sys_var_bool_ptr	sys_sync_frm("sync_frm", &opt_sync_frm);
sys_var_long_ptr	sys_table_cache_size("table_cache",
					     &table_cache_size);
sys_var_long_ptr	sys_table_lock_wait_timeout("table_lock_wait_timeout",
                                                    &table_lock_wait_timeout);
sys_var_long_ptr	sys_thread_cache_size("thread_cache_size",
					      &thread_cache_size);
sys_var_thd_enum	sys_tx_isolation("tx_isolation",
					 &SV::tx_isolation,
					 &tx_isolation_typelib,
					 fix_tx_isolation);
sys_var_thd_ulong	sys_tmp_table_size("tmp_table_size",
					   &SV::tmp_table_size);
sys_var_bool_ptr  sys_timed_mutexes("timed_mutexes",
                                    &timed_mutexes);
sys_var_thd_ulong	sys_net_wait_timeout("wait_timeout",
					     &SV::net_wait_timeout);

#ifdef HAVE_INNOBASE_DB
sys_var_long_ptr	sys_innodb_fast_shutdown("innodb_fast_shutdown",
						 &innobase_fast_shutdown);
sys_var_long_ptr        sys_innodb_max_dirty_pages_pct("innodb_max_dirty_pages_pct",
                                                        &srv_max_buf_pool_modified_pct);
sys_var_long_ptr	sys_innodb_max_purge_lag("innodb_max_purge_lag",
							&srv_max_purge_lag);
sys_var_thd_bool	sys_innodb_table_locks("innodb_table_locks",
                                               &SV::innodb_table_locks);
sys_var_thd_bool	sys_innodb_support_xa("innodb_support_xa",
                                               &SV::innodb_support_xa);
sys_var_long_ptr	sys_innodb_autoextend_increment("innodb_autoextend_increment",
							&srv_auto_extend_increment);
sys_var_long_ptr	sys_innodb_sync_spin_loops("innodb_sync_spin_loops",
                                             &srv_n_spin_wait_rounds);
sys_var_long_ptr  sys_innodb_concurrency_tickets("innodb_concurrency_tickets",
                                             &srv_n_free_tickets_to_enter);
sys_var_long_ptr  sys_innodb_thread_sleep_delay("innodb_thread_sleep_delay",
                                                &srv_thread_sleep_delay);
sys_var_long_ptr  sys_innodb_thread_concurrency("innodb_thread_concurrency",
                                                &srv_thread_concurrency);
sys_var_long_ptr  sys_innodb_commit_concurrency("innodb_commit_concurrency",
                                                &srv_commit_concurrency);
#endif

/* Condition pushdown to storage engine */
sys_var_thd_bool
sys_engine_condition_pushdown("engine_condition_pushdown",
			      &SV::engine_condition_pushdown);

#ifdef HAVE_NDBCLUSTER_DB
/* ndb thread specific variable settings */
sys_var_thd_ulong
sys_ndb_autoincrement_prefetch_sz("ndb_autoincrement_prefetch_sz",
				  &SV::ndb_autoincrement_prefetch_sz);
sys_var_thd_bool
sys_ndb_force_send("ndb_force_send", &SV::ndb_force_send);
sys_var_thd_bool
sys_ndb_use_exact_count("ndb_use_exact_count", &SV::ndb_use_exact_count);
sys_var_thd_bool
sys_ndb_use_transactions("ndb_use_transactions", &SV::ndb_use_transactions);
sys_var_long_ptr
sys_ndb_cache_check_time("ndb_cache_check_time", &ndb_cache_check_time);
sys_var_thd_bool
sys_ndb_index_stat_enable("ndb_index_stat_enable",
                          &SV::ndb_index_stat_enable);
sys_var_thd_ulong
sys_ndb_index_stat_cache_entries("ndb_index_stat_cache_entries",
                                 &SV::ndb_index_stat_cache_entries);
sys_var_thd_ulong
sys_ndb_index_stat_update_freq("ndb_index_stat_update_freq",
                               &SV::ndb_index_stat_update_freq);
#endif

/* Time/date/datetime formats */

sys_var_thd_date_time_format sys_time_format("time_format",
					     &SV::time_format,
					     MYSQL_TIMESTAMP_TIME);
sys_var_thd_date_time_format sys_date_format("date_format",
					     &SV::date_format,
					     MYSQL_TIMESTAMP_DATE);
sys_var_thd_date_time_format sys_datetime_format("datetime_format",
						 &SV::datetime_format,
						 MYSQL_TIMESTAMP_DATETIME);

/* Variables that are bits in THD */

sys_var_thd_bit sys_autocommit("autocommit", 0,
                               set_option_autocommit,
                               OPTION_NOT_AUTOCOMMIT,
                               1);
static sys_var_thd_bit	sys_big_tables("big_tables", 0,
				       set_option_bit,
				       OPTION_BIG_TABLES);
#ifndef TO_BE_DELETED	/* Alias for big_tables */
static sys_var_thd_bit	sys_sql_big_tables("sql_big_tables", 0,
					   set_option_bit,
					   OPTION_BIG_TABLES);
#endif
static sys_var_thd_bit	sys_big_selects("sql_big_selects", 0,
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_thd_bit	sys_log_off("sql_log_off", 0,
				    set_option_bit,
				    OPTION_LOG_OFF);
static sys_var_thd_bit	sys_log_update("sql_log_update",
                                       check_log_update,
				       set_log_update,
				       OPTION_UPDATE_LOG);
static sys_var_thd_bit	sys_log_binlog("sql_log_bin",
                                       check_log_update,
				       set_log_bin,
				       OPTION_BIN_LOG);
static sys_var_thd_bit	sys_sql_warnings("sql_warnings", 0,
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_thd_bit	sys_sql_notes("sql_notes", 0,
					 set_option_bit,
					 OPTION_SQL_NOTES);
static sys_var_thd_bit	sys_auto_is_null("sql_auto_is_null", 0,
					 set_option_bit,
					 OPTION_AUTO_IS_NULL);
static sys_var_thd_bit	sys_safe_updates("sql_safe_updates", 0,
					 set_option_bit,
					 OPTION_SAFE_UPDATES);
static sys_var_thd_bit	sys_buffer_results("sql_buffer_result", 0,
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_thd_bit	sys_quote_show_create("sql_quote_show_create", 0,
					      set_option_bit,
					      OPTION_QUOTE_SHOW_CREATE);
static sys_var_thd_bit	sys_foreign_key_checks("foreign_key_checks", 0,
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS,
					       1);
static sys_var_thd_bit	sys_unique_checks("unique_checks", 0,
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS,
					  1);

/* Local state variables */

static sys_var_thd_ha_rows	sys_select_limit("sql_select_limit",
						 &SV::select_limit);
static sys_var_timestamp	sys_timestamp("timestamp");
static sys_var_last_insert_id	sys_last_insert_id("last_insert_id");
static sys_var_last_insert_id	sys_identity("identity");
static sys_var_insert_id	sys_insert_id("insert_id");
static sys_var_readonly		sys_error_count("error_count",
						OPT_SESSION,
						SHOW_LONG,
						get_error_count);
static sys_var_readonly		sys_warning_count("warning_count",
						  OPT_SESSION,
						  SHOW_LONG,
						  get_warning_count);

/* alias for last_insert_id() to be compatible with Sybase */
#ifdef HAVE_REPLICATION
static sys_var_slave_skip_counter sys_slave_skip_counter("sql_slave_skip_counter");
#endif
static sys_var_rand_seed1	sys_rand_seed1("rand_seed1");
static sys_var_rand_seed2	sys_rand_seed2("rand_seed2");

static sys_var_thd_ulong        sys_default_week_format("default_week_format",
					                &SV::default_week_format);

sys_var_thd_ulong               sys_group_concat_max_len("group_concat_max_len",
                                                         &SV::group_concat_max_len);

sys_var_thd_time_zone            sys_time_zone("time_zone");

/* Read only variables */

sys_var_const_str		sys_os("version_compile_os", SYSTEM_TYPE);
sys_var_readonly                sys_have_innodb("have_innodb", OPT_GLOBAL,
                                                SHOW_CHAR, get_have_innodb);
/* Global read-only variable describing server license */
sys_var_const_str		sys_license("license", STRINGIFY_ARG(LICENSE));


/*
  List of all variables for initialisation and storage in hash
  This is sorted in alphabetical order to make it easy to add new variables

  If the variable is not in this list, it can't be changed with
  SET variable_name=
*/

sys_var *sys_variables[]=
{
  &sys_auto_is_null,
  &sys_auto_increment_increment,
  &sys_auto_increment_offset,
  &sys_autocommit,
  &sys_automatic_sp_privileges,
  &sys_big_tables,
  &sys_big_selects,
  &sys_binlog_cache_size,
  &sys_buffer_results,
  &sys_bulk_insert_buff_size,
  &sys_character_set_server,
  &sys_character_set_database,
  &sys_character_set_client,
  &sys_character_set_connection,
  &sys_character_set_results,
  &sys_charset_system,
  &sys_collation_connection,
  &sys_collation_database,
  &sys_collation_server,
  &sys_completion_type,
  &sys_concurrent_insert,
  &sys_connect_timeout,
  &sys_date_format,
  &sys_datetime_format,
  &sys_div_precincrement,
  &sys_default_week_format,
  &sys_delay_key_write,
  &sys_delayed_insert_limit,
  &sys_delayed_insert_timeout,
  &sys_delayed_queue_size,
  &sys_error_count,
  &sys_expire_logs_days,
  &sys_flush,
  &sys_flush_time,
  &sys_ft_boolean_syntax,
  &sys_foreign_key_checks,
  &sys_group_concat_max_len,
  &sys_have_innodb,
  &sys_identity,
  &sys_init_connect,
  &sys_init_slave,
  &sys_insert_id,
  &sys_interactive_timeout,
  &sys_join_buffer_size,
  &sys_key_buffer_size,
  &sys_key_cache_block_size,
  &sys_key_cache_division_limit,
  &sys_key_cache_age_threshold,
  &sys_last_insert_id,
  &sys_license,
  &sys_local_infile,
  &sys_log_binlog,
  &sys_log_off,
  &sys_log_update,
  &sys_log_warnings,
  &sys_long_query_time,
  &sys_low_priority_updates,
  &sys_max_allowed_packet,
  &sys_max_binlog_cache_size,
  &sys_max_binlog_size,
  &sys_max_connect_errors,
  &sys_max_connections,
  &sys_max_delayed_threads,
  &sys_max_error_count,
  &sys_max_insert_delayed_threads,
  &sys_max_heap_table_size,
  &sys_max_join_size,
  &sys_max_length_for_sort_data,
  &sys_max_relay_log_size,
  &sys_max_seeks_for_key,
  &sys_max_sort_length,
  &sys_max_tmp_tables,
  &sys_max_user_connections,
  &sys_max_write_lock_count,
  &sys_multi_range_count,
  &sys_myisam_data_pointer_size,
  &sys_myisam_max_sort_file_size,
  &sys_myisam_repair_threads,
  &sys_myisam_sort_buffer_size,
  &sys_myisam_stats_method,
  &sys_net_buffer_length,
  &sys_net_read_timeout,
  &sys_net_retry_count,
  &sys_net_wait_timeout,
  &sys_net_write_timeout,
  &sys_new_mode,
  &sys_old_alter_table,
  &sys_old_passwords,
  &sys_optimizer_prune_level,
  &sys_optimizer_search_depth,
  &sys_preload_buff_size,
  &sys_pseudo_thread_id,
  &sys_query_alloc_block_size,
  &sys_query_cache_size,
  &sys_query_prealloc_size,
#ifdef HAVE_QUERY_CACHE
  &sys_query_cache_limit,
  &sys_query_cache_min_res_unit,
  &sys_query_cache_type,
  &sys_query_cache_wlock_invalidate,
#endif /* HAVE_QUERY_CACHE */
  &sys_quote_show_create,
  &sys_rand_seed1,
  &sys_rand_seed2,
  &sys_range_alloc_block_size,
  &sys_readonly,
  &sys_read_buff_size,
  &sys_read_rnd_buff_size,
#ifdef HAVE_REPLICATION
  &sys_relay_log_purge,
#endif
  &sys_rpl_recovery_rank,
  &sys_safe_updates,
  &sys_secure_auth,
  &sys_select_limit,
  &sys_server_id,
#ifdef HAVE_REPLICATION
  &sys_slave_compressed_protocol,
  &sys_slave_net_timeout,
  &sys_slave_trans_retries,
  &sys_slave_skip_counter,
#endif
  &sys_slow_launch_time,
  &sys_sort_buffer,
  &sys_sql_big_tables,
  &sys_sql_low_priority_updates,
  &sys_sql_max_join_size,
  &sys_sql_mode,
  &sys_sql_warnings,
  &sys_sql_notes,
  &sys_storage_engine,
#ifdef HAVE_REPLICATION
  &sys_sync_binlog_period,
  &sys_sync_replication,
  &sys_sync_replication_slave_id,
  &sys_sync_replication_timeout,
#endif
  &sys_sync_frm,
  &sys_table_cache_size,
  &sys_table_lock_wait_timeout,
  &sys_table_type,
  &sys_thread_cache_size,
  &sys_time_format,
  &sys_timed_mutexes,
  &sys_timestamp,
  &sys_time_zone,
  &sys_tmp_table_size,
  &sys_trans_alloc_block_size,
  &sys_trans_prealloc_size,
  &sys_tx_isolation,
  &sys_os,
#ifdef HAVE_INNOBASE_DB
  &sys_innodb_fast_shutdown,
  &sys_innodb_max_dirty_pages_pct,
  &sys_innodb_max_purge_lag,
  &sys_innodb_table_locks,
  &sys_innodb_support_xa,
  &sys_innodb_max_purge_lag,
  &sys_innodb_autoextend_increment,
  &sys_innodb_sync_spin_loops,
  &sys_innodb_concurrency_tickets,
  &sys_innodb_thread_sleep_delay,
  &sys_innodb_thread_concurrency,
  &sys_innodb_commit_concurrency,
#endif  
  &sys_trust_routine_creators,
  &sys_engine_condition_pushdown,
#ifdef HAVE_NDBCLUSTER_DB
  &sys_ndb_autoincrement_prefetch_sz,
  &sys_ndb_cache_check_time,
  &sys_ndb_force_send,
  &sys_ndb_use_exact_count,
  &sys_ndb_use_transactions,
  &sys_ndb_index_stat_enable,
  &sys_ndb_index_stat_cache_entries,
  &sys_ndb_index_stat_update_freq,
#endif
  &sys_unique_checks,
  &sys_updatable_views_with_limit,
  &sys_warning_count
};


/*
  Variables shown by SHOW variables in alphabetical order
*/

struct show_var_st init_vars[]= {
  {"auto_increment_increment", (char*) &sys_auto_increment_increment, SHOW_SYS},
  {"auto_increment_offset",   (char*) &sys_auto_increment_offset, SHOW_SYS},
  {sys_automatic_sp_privileges.name,(char*) &sys_automatic_sp_privileges,       SHOW_SYS},
  {"back_log",                (char*) &back_log,                    SHOW_LONG},
  {"basedir",                 mysql_home,                           SHOW_CHAR},
#ifdef HAVE_BERKELEY_DB
  {"bdb_cache_size",          (char*) &berkeley_cache_size,         SHOW_LONG},
  {"bdb_home",                (char*) &berkeley_home,               SHOW_CHAR_PTR},
  {"bdb_log_buffer_size",     (char*) &berkeley_log_buffer_size,    SHOW_LONG},
  {"bdb_logdir",              (char*) &berkeley_logdir,             SHOW_CHAR_PTR},
  {"bdb_max_lock",            (char*) &berkeley_max_lock,	    SHOW_LONG},
  {"bdb_shared_data",	      (char*) &berkeley_shared_data,	    SHOW_BOOL},
  {"bdb_tmpdir",              (char*) &berkeley_tmpdir,             SHOW_CHAR_PTR},
#endif
  {sys_binlog_cache_size.name,(char*) &sys_binlog_cache_size,	    SHOW_SYS},
  {sys_bulk_insert_buff_size.name,(char*) &sys_bulk_insert_buff_size,SHOW_SYS},
  {sys_character_set_client.name,(char*) &sys_character_set_client, SHOW_SYS},
  {sys_character_set_connection.name,(char*) &sys_character_set_connection,SHOW_SYS},
  {sys_character_set_database.name, (char*) &sys_character_set_database,SHOW_SYS},
  {sys_character_set_results.name,(char*) &sys_character_set_results, SHOW_SYS},
  {sys_character_set_server.name, (char*) &sys_character_set_server,SHOW_SYS},
  {sys_charset_system.name,   (char*) &sys_charset_system,          SHOW_SYS},
  {"character_sets_dir",      mysql_charsets_dir,                   SHOW_CHAR},
  {sys_collation_connection.name,(char*) &sys_collation_connection, SHOW_SYS},
  {sys_collation_database.name,(char*) &sys_collation_database,     SHOW_SYS},
  {sys_collation_server.name,(char*) &sys_collation_server,         SHOW_SYS},
  {sys_completion_type.name,  (char*) &sys_completion_type,	    SHOW_SYS},
  {sys_concurrent_insert.name,(char*) &sys_concurrent_insert,       SHOW_SYS},
  {sys_connect_timeout.name,  (char*) &sys_connect_timeout,         SHOW_SYS},
  {"datadir",                 mysql_real_data_home,                 SHOW_CHAR},
  {sys_date_format.name,      (char*) &sys_date_format,		    SHOW_SYS},
  {sys_datetime_format.name,  (char*) &sys_datetime_format,	    SHOW_SYS},
  {sys_default_week_format.name, (char*) &sys_default_week_format,  SHOW_SYS},
  {sys_delay_key_write.name,  (char*) &sys_delay_key_write,         SHOW_SYS},
  {sys_delayed_insert_limit.name, (char*) &sys_delayed_insert_limit,SHOW_SYS},
  {sys_delayed_insert_timeout.name, (char*) &sys_delayed_insert_timeout, SHOW_SYS},
  {sys_delayed_queue_size.name,(char*) &sys_delayed_queue_size,     SHOW_SYS},
  {sys_div_precincrement.name,(char*) &sys_div_precincrement,SHOW_SYS},
  {sys_engine_condition_pushdown.name, 
   (char*) &sys_engine_condition_pushdown,                          SHOW_SYS},
  {sys_expire_logs_days.name, (char*) &sys_expire_logs_days,        SHOW_SYS},
  {sys_flush.name,             (char*) &sys_flush,                  SHOW_SYS},
  {sys_flush_time.name,        (char*) &sys_flush_time,             SHOW_SYS},
  {sys_ft_boolean_syntax.name,(char*) &ft_boolean_syntax,	    SHOW_CHAR},
  {"ft_max_word_len",         (char*) &ft_max_word_len,             SHOW_LONG},
  {"ft_min_word_len",         (char*) &ft_min_word_len,             SHOW_LONG},
  {"ft_query_expansion_limit",(char*) &ft_query_expansion_limit,    SHOW_LONG},
  {"ft_stopword_file",        (char*) &ft_stopword_file,            SHOW_CHAR_PTR},
  {sys_group_concat_max_len.name, (char*) &sys_group_concat_max_len,  SHOW_SYS},
  {"have_archive",	      (char*) &have_archive_db,	            SHOW_HAVE},
  {"have_bdb",		      (char*) &have_berkeley_db,	    SHOW_HAVE},
  {"have_blackhole_engine",   (char*) &have_blackhole_db,	    SHOW_HAVE},
  {"have_compress",	      (char*) &have_compress,		    SHOW_HAVE},
  {"have_crypt",	      (char*) &have_crypt,		    SHOW_HAVE},
  {"have_csv",	              (char*) &have_csv_db,	            SHOW_HAVE},
  {"have_example_engine",     (char*) &have_example_db,	            SHOW_HAVE},
  {"have_federated_engine",   (char*) &have_federated_db,           SHOW_HAVE},
  {"have_geometry",           (char*) &have_geometry,               SHOW_HAVE},
  {"have_innodb",	      (char*) &have_innodb,		    SHOW_HAVE},
  {"have_isam",		      (char*) &have_isam,		    SHOW_HAVE},
  {"have_ndbcluster",         (char*) &have_ndbcluster,             SHOW_HAVE},
  {"have_openssl",	      (char*) &have_openssl,		    SHOW_HAVE},
  {"have_partition_engine",   (char*) &have_partition_db,	    SHOW_HAVE},
  {"have_query_cache",        (char*) &have_query_cache,            SHOW_HAVE},
  {"have_raid",		      (char*) &have_raid,		    SHOW_HAVE},
  {"have_rtree_keys",         (char*) &have_rtree_keys,             SHOW_HAVE},
  {"have_symlink",            (char*) &have_symlink,                SHOW_HAVE},
  {"init_connect",            (char*) &sys_init_connect,            SHOW_SYS},
  {"init_file",               (char*) &opt_init_file,               SHOW_CHAR_PTR},
  {"init_slave",              (char*) &sys_init_slave,              SHOW_SYS},
#ifdef HAVE_INNOBASE_DB
  {"innodb_additional_mem_pool_size", (char*) &innobase_additional_mem_pool_size, SHOW_LONG },
  {sys_innodb_autoextend_increment.name, (char*) &sys_innodb_autoextend_increment, SHOW_SYS},
  {"innodb_buffer_pool_awe_mem_mb", (char*) &innobase_buffer_pool_awe_mem_mb, SHOW_LONG },
  {"innodb_buffer_pool_size", (char*) &innobase_buffer_pool_size, SHOW_LONG },
  {"innodb_checksums", (char*) &innobase_use_checksums, SHOW_MY_BOOL},
  {sys_innodb_commit_concurrency.name, (char*) &sys_innodb_commit_concurrency, SHOW_SYS},
  {sys_innodb_concurrency_tickets.name, (char*) &sys_innodb_concurrency_tickets, SHOW_SYS},
  {"innodb_data_file_path", (char*) &innobase_data_file_path,	    SHOW_CHAR_PTR},
  {"innodb_data_home_dir",  (char*) &innobase_data_home_dir,	    SHOW_CHAR_PTR},
  {"innodb_doublewrite", (char*) &innobase_use_doublewrite, SHOW_MY_BOOL},
  {sys_innodb_fast_shutdown.name,(char*) &sys_innodb_fast_shutdown, SHOW_SYS},
  {"innodb_file_io_threads", (char*) &innobase_file_io_threads, SHOW_LONG },
  {"innodb_file_per_table", (char*) &innobase_file_per_table, SHOW_MY_BOOL},
  {"innodb_flush_log_at_trx_commit", (char*) &innobase_flush_log_at_trx_commit, SHOW_INT},
  {"innodb_flush_method",    (char*) &innobase_unix_file_flush_method, SHOW_CHAR_PTR},
  {"innodb_force_recovery", (char*) &innobase_force_recovery, SHOW_LONG },
  {"innodb_lock_wait_timeout", (char*) &innobase_lock_wait_timeout, SHOW_LONG },
  {"innodb_locks_unsafe_for_binlog", (char*) &innobase_locks_unsafe_for_binlog, SHOW_MY_BOOL},
  {"innodb_log_arch_dir",   (char*) &innobase_log_arch_dir, 	    SHOW_CHAR_PTR},
  {"innodb_log_archive",    (char*) &innobase_log_archive, 	    SHOW_MY_BOOL},
  {"innodb_log_buffer_size", (char*) &innobase_log_buffer_size, SHOW_LONG },
  {"innodb_log_file_size", (char*) &innobase_log_file_size, SHOW_LONG},
  {"innodb_log_files_in_group", (char*) &innobase_log_files_in_group,	SHOW_LONG},
  {"innodb_log_group_home_dir", (char*) &innobase_log_group_home_dir, SHOW_CHAR_PTR},
  {sys_innodb_max_dirty_pages_pct.name, (char*) &sys_innodb_max_dirty_pages_pct, SHOW_SYS},
  {sys_innodb_max_purge_lag.name, (char*) &sys_innodb_max_purge_lag, SHOW_SYS},
  {"innodb_mirrored_log_groups", (char*) &innobase_mirrored_log_groups, SHOW_LONG},
  {"innodb_open_files", (char*) &innobase_open_files, SHOW_LONG },
  {sys_innodb_support_xa.name, (char*) &sys_innodb_support_xa, SHOW_SYS},
  {sys_innodb_sync_spin_loops.name, (char*) &sys_innodb_sync_spin_loops, SHOW_SYS},
  {sys_innodb_table_locks.name, (char*) &sys_innodb_table_locks, SHOW_SYS},
  {sys_innodb_thread_concurrency.name, (char*) &sys_innodb_thread_concurrency, SHOW_SYS},
  {sys_innodb_thread_sleep_delay.name, (char*) &sys_innodb_thread_sleep_delay, SHOW_SYS},
#endif
  {sys_interactive_timeout.name,(char*) &sys_interactive_timeout,   SHOW_SYS},
  {sys_join_buffer_size.name,   (char*) &sys_join_buffer_size,	    SHOW_SYS},
  {sys_key_buffer_size.name,	(char*) &sys_key_buffer_size,	    SHOW_SYS},
  {sys_key_cache_age_threshold.name,   (char*) &sys_key_cache_age_threshold,
                                                                    SHOW_SYS},
  {sys_key_cache_block_size.name,   (char*) &sys_key_cache_block_size,
                                                                    SHOW_SYS},
  {sys_key_cache_division_limit.name,   (char*) &sys_key_cache_division_limit,
                                                                    SHOW_SYS},
  {"language",                language,                             SHOW_CHAR},
  {"large_files_support",     (char*) &opt_large_files,             SHOW_BOOL},
  {"large_page_size",         (char*) &opt_large_page_size,         SHOW_INT},
  {"large_pages",             (char*) &opt_large_pages,             SHOW_MY_BOOL},
  {sys_license.name,	      (char*) &sys_license,                 SHOW_SYS},
  {sys_local_infile.name,     (char*) &sys_local_infile,	    SHOW_SYS},
#ifdef HAVE_MLOCKALL
  {"locked_in_memory",	      (char*) &locked_in_memory,	    SHOW_BOOL},
#endif
  {"log",                     (char*) &opt_log,                     SHOW_BOOL},
  {"log_bin",                 (char*) &opt_bin_log,                 SHOW_BOOL},
  {sys_trust_routine_creators.name,(char*) &sys_trust_routine_creators, SHOW_SYS},
  {"log_error",               (char*) log_error_file,               SHOW_CHAR},
#ifdef HAVE_REPLICATION
  {"log_slave_updates",       (char*) &opt_log_slave_updates,       SHOW_MY_BOOL},
#endif
  {"log_slow_queries",        (char*) &opt_slow_log,                SHOW_BOOL},
  {sys_log_warnings.name,     (char*) &sys_log_warnings,	    SHOW_SYS},
  {sys_long_query_time.name,  (char*) &sys_long_query_time, 	    SHOW_SYS},
  {sys_low_priority_updates.name, (char*) &sys_low_priority_updates, SHOW_SYS},
  {"lower_case_file_system",  (char*) &lower_case_file_system,      SHOW_MY_BOOL},
  {"lower_case_table_names",  (char*) &lower_case_table_names,      SHOW_INT},
  {sys_max_allowed_packet.name,(char*) &sys_max_allowed_packet,	    SHOW_SYS},
  {sys_max_binlog_cache_size.name,(char*) &sys_max_binlog_cache_size, SHOW_SYS},
  {sys_max_binlog_size.name,    (char*) &sys_max_binlog_size,	    SHOW_SYS},
  {sys_max_connect_errors.name, (char*) &sys_max_connect_errors,    SHOW_SYS},
  {sys_max_connections.name,    (char*) &sys_max_connections,	    SHOW_SYS},
  {sys_max_delayed_threads.name,(char*) &sys_max_delayed_threads,   SHOW_SYS},
  {sys_max_error_count.name,	(char*) &sys_max_error_count,	    SHOW_SYS},
  {sys_max_heap_table_size.name,(char*) &sys_max_heap_table_size,   SHOW_SYS},
  {sys_max_insert_delayed_threads.name,
   (char*) &sys_max_insert_delayed_threads,   SHOW_SYS},
  {sys_max_join_size.name,	(char*) &sys_max_join_size,	    SHOW_SYS},
  {sys_max_length_for_sort_data.name, (char*) &sys_max_length_for_sort_data,
   SHOW_SYS},
  {sys_max_relay_log_size.name, (char*) &sys_max_relay_log_size,    SHOW_SYS},
  {sys_max_seeks_for_key.name,  (char*) &sys_max_seeks_for_key,	    SHOW_SYS},
  {sys_max_sort_length.name,	(char*) &sys_max_sort_length,	    SHOW_SYS},
  {sys_max_tmp_tables.name,	(char*) &sys_max_tmp_tables,	    SHOW_SYS},
  {sys_max_user_connections.name,(char*) &sys_max_user_connections, SHOW_SYS},
  {sys_max_write_lock_count.name, (char*) &sys_max_write_lock_count,SHOW_SYS},
  {sys_multi_range_count.name,  (char*) &sys_multi_range_count,     SHOW_SYS},
  {sys_myisam_data_pointer_size.name, (char*) &sys_myisam_data_pointer_size, SHOW_SYS},
  {sys_myisam_max_sort_file_size.name, (char*) &sys_myisam_max_sort_file_size,
   SHOW_SYS},
  {"myisam_recover_options",  (char*) &myisam_recover_options_str,  SHOW_CHAR_PTR},
  {sys_myisam_repair_threads.name, (char*) &sys_myisam_repair_threads,
   SHOW_SYS},
  {sys_myisam_sort_buffer_size.name, (char*) &sys_myisam_sort_buffer_size, SHOW_SYS},
  
  {sys_myisam_stats_method.name, (char*) &sys_myisam_stats_method, SHOW_SYS},
  
#ifdef __NT__
  {"named_pipe",	      (char*) &opt_enable_named_pipe,       SHOW_MY_BOOL},
#endif
#ifdef HAVE_NDBCLUSTER_DB
  {sys_ndb_autoincrement_prefetch_sz.name,
   (char*) &sys_ndb_autoincrement_prefetch_sz,                      SHOW_SYS},
  {sys_ndb_cache_check_time.name,(char*) &sys_ndb_cache_check_time, SHOW_SYS},
  {sys_ndb_force_send.name,   (char*) &sys_ndb_force_send,          SHOW_SYS},
  {sys_ndb_index_stat_cache_entries.name, (char*) &sys_ndb_index_stat_cache_entries, SHOW_SYS},
  {sys_ndb_index_stat_enable.name, (char*) &sys_ndb_index_stat_enable, SHOW_SYS},
  {sys_ndb_index_stat_update_freq.name, (char*) &sys_ndb_index_stat_update_freq, SHOW_SYS},
  {sys_ndb_use_exact_count.name,(char*) &sys_ndb_use_exact_count,   SHOW_SYS},
  {sys_ndb_use_transactions.name,(char*) &sys_ndb_use_transactions, SHOW_SYS},
#endif
  {sys_net_buffer_length.name,(char*) &sys_net_buffer_length,       SHOW_SYS},
  {sys_net_read_timeout.name, (char*) &sys_net_read_timeout,        SHOW_SYS},
  {sys_net_retry_count.name,  (char*) &sys_net_retry_count,	    SHOW_SYS},
  {sys_net_write_timeout.name,(char*) &sys_net_write_timeout,       SHOW_SYS},
  {sys_new_mode.name,         (char*) &sys_new_mode,                SHOW_SYS},
  {sys_old_alter_table.name,  (char*) &sys_old_alter_table,         SHOW_SYS},
  {sys_old_passwords.name,    (char*) &sys_old_passwords,           SHOW_SYS},
  {"open_files_limit",	      (char*) &open_files_limit,	    SHOW_LONG},
  {sys_optimizer_prune_level.name, (char*) &sys_optimizer_prune_level,
   SHOW_SYS},
  {sys_optimizer_search_depth.name,(char*) &sys_optimizer_search_depth,
   SHOW_SYS},
  {"pid_file",                (char*) pidfile_name,                 SHOW_CHAR},
  {"port",                    (char*) &mysqld_port,                  SHOW_INT},
  {sys_preload_buff_size.name, (char*) &sys_preload_buff_size,      SHOW_SYS},
  {"protocol_version",        (char*) &protocol_version,            SHOW_INT},
  {sys_query_alloc_block_size.name, (char*) &sys_query_alloc_block_size,
   SHOW_SYS},
#ifdef HAVE_QUERY_CACHE
  {sys_query_cache_limit.name,(char*) &sys_query_cache_limit,	    SHOW_SYS},
  {sys_query_cache_min_res_unit.name, (char*) &sys_query_cache_min_res_unit,
   SHOW_SYS},
  {sys_query_cache_size.name, (char*) &sys_query_cache_size,	    SHOW_SYS},
  {sys_query_cache_type.name, (char*) &sys_query_cache_type,        SHOW_SYS},
  {sys_query_cache_wlock_invalidate.name,
   (char *) &sys_query_cache_wlock_invalidate, SHOW_SYS},
#endif /* HAVE_QUERY_CACHE */
  {sys_query_prealloc_size.name, (char*) &sys_query_prealloc_size,  SHOW_SYS},
  {sys_range_alloc_block_size.name, (char*) &sys_range_alloc_block_size,
   SHOW_SYS},
  {sys_read_buff_size.name,   (char*) &sys_read_buff_size,	    SHOW_SYS},
  {sys_readonly.name,         (char*) &sys_readonly,                SHOW_SYS},
  {sys_read_rnd_buff_size.name,(char*) &sys_read_rnd_buff_size,	    SHOW_SYS},
#ifdef HAVE_REPLICATION
  {sys_relay_log_purge.name,  (char*) &sys_relay_log_purge,         SHOW_SYS},
  {"relay_log_space_limit",  (char*) &relay_log_space_limit,        SHOW_LONGLONG},
#endif
  {sys_rpl_recovery_rank.name,(char*) &sys_rpl_recovery_rank,       SHOW_SYS},
  {"secure_auth",             (char*) &sys_secure_auth,             SHOW_SYS},
#ifdef HAVE_SMEM
  {"shared_memory",           (char*) &opt_enable_shared_memory,    SHOW_MY_BOOL},
  {"shared_memory_base_name", (char*) &shared_memory_base_name,     SHOW_CHAR_PTR},
#endif
  {sys_server_id.name,	      (char*) &sys_server_id,		    SHOW_SYS},
  {"skip_external_locking",   (char*) &my_disable_locking,          SHOW_MY_BOOL},
  {"skip_networking",         (char*) &opt_disable_networking,      SHOW_BOOL},
  {"skip_show_database",      (char*) &opt_skip_show_db,            SHOW_BOOL},
#ifdef HAVE_REPLICATION
  {sys_slave_compressed_protocol.name,
    (char*) &sys_slave_compressed_protocol,           SHOW_SYS},
  {"slave_load_tmpdir",       (char*) &slave_load_tmpdir,           SHOW_CHAR_PTR},
  {sys_slave_net_timeout.name,(char*) &sys_slave_net_timeout,	    SHOW_SYS},
  {"slave_skip_errors",       (char*) &slave_error_mask,            SHOW_SLAVE_SKIP_ERRORS},
  {sys_slave_trans_retries.name,(char*) &sys_slave_trans_retries,   SHOW_SYS},
#endif
  {sys_slow_launch_time.name, (char*) &sys_slow_launch_time,        SHOW_SYS},
#ifdef HAVE_SYS_UN_H
  {"socket",                  (char*) &mysqld_unix_port,             SHOW_CHAR_PTR},
#endif
  {sys_sort_buffer.name,      (char*) &sys_sort_buffer, 	    SHOW_SYS},
  {sys_sql_mode.name,         (char*) &sys_sql_mode,                SHOW_SYS},
  {"sql_notes",               (char*) &sys_sql_notes,               SHOW_BOOL},
  {"sql_warnings",            (char*) &sys_sql_warnings,            SHOW_BOOL},
  {sys_storage_engine.name,   (char*) &sys_storage_engine,          SHOW_SYS},
#ifdef HAVE_REPLICATION
  {sys_sync_binlog_period.name,(char*) &sys_sync_binlog_period,     SHOW_SYS},
#endif
  {sys_sync_frm.name,         (char*) &sys_sync_frm,               SHOW_SYS},
#ifdef HAVE_REPLICATION
  {sys_sync_replication.name, (char*) &sys_sync_replication,        SHOW_SYS},
  {sys_sync_replication_slave_id.name, (char*) &sys_sync_replication_slave_id,SHOW_SYS},
  {sys_sync_replication_timeout.name, (char*) &sys_sync_replication_timeout,SHOW_SYS},
#endif
#ifdef HAVE_TZNAME
  {"system_time_zone",        system_time_zone,                     SHOW_CHAR},
#endif
  {"table_cache",             (char*) &table_cache_size,            SHOW_LONG},
  {"table_lock_wait_timeout", (char*) &table_lock_wait_timeout,     SHOW_LONG },
  {sys_table_type.name,	      (char*) &sys_table_type,	            SHOW_SYS},
  {sys_thread_cache_size.name,(char*) &sys_thread_cache_size,       SHOW_SYS},
#ifdef HAVE_THR_SETCONCURRENCY
  {"thread_concurrency",      (char*) &concurrency,                 SHOW_LONG},
#endif
  {"thread_stack",            (char*) &thread_stack,                SHOW_LONG},
  {sys_time_format.name,      (char*) &sys_time_format,		    SHOW_SYS},
  {"time_zone",               (char*) &sys_time_zone,               SHOW_SYS},
  {sys_timed_mutexes.name,    (char*) &sys_timed_mutexes,       SHOW_SYS},
  {sys_tmp_table_size.name,   (char*) &sys_tmp_table_size,	    SHOW_SYS},
  {"tmpdir",                  (char*) &opt_mysql_tmpdir,            SHOW_CHAR_PTR},
  {sys_trans_alloc_block_size.name, (char*) &sys_trans_alloc_block_size,
   SHOW_SYS},
  {sys_trans_prealloc_size.name, (char*) &sys_trans_prealloc_size,  SHOW_SYS},
  {sys_tx_isolation.name,     (char*) &sys_tx_isolation,	    SHOW_SYS},
  {sys_updatable_views_with_limit.name,
                              (char*) &sys_updatable_views_with_limit,SHOW_SYS},
  {"version",                 server_version,                       SHOW_CHAR},
#ifdef HAVE_BERKELEY_DB
  {"version_bdb",             (char*) DB_VERSION_STRING,            SHOW_CHAR},
#endif
  {"version_comment",         (char*) MYSQL_COMPILATION_COMMENT,    SHOW_CHAR},
  {"version_compile_machine", (char*) MACHINE_TYPE,		    SHOW_CHAR},
  {sys_os.name,		      (char*) &sys_os,			    SHOW_SYS},
  {sys_net_wait_timeout.name, (char*) &sys_net_wait_timeout,	    SHOW_SYS},
  {NullS, NullS, SHOW_LONG}
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
  if (!(res= my_strdup_with_length((byte*)old_value, new_length, MYF(0))))
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
  If one doesn't use the SESSION modifier, the isolation level
  is only active for the next command
*/

static void fix_tx_isolation(THD *thd, enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->session_tx_isolation= ((enum_tx_isolation)
				thd->variables.tx_isolation);
}

static void fix_completion_type(THD *thd __attribute__(unused), 
				enum_var_type type __attribute__(unused)) {}

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
    thd->net.read_timeout=thd->variables.net_read_timeout;
}


static void fix_net_write_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.write_timeout=thd->variables.net_write_timeout;
}

static void fix_net_retry_count(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.retry_count=thd->variables.net_retry_count;
}
#else /* HAVE_REPLICATION */
static void fix_net_read_timeout(THD *thd __attribute__(unused),
				 enum_var_type type __attribute__(unused))
{}
static void fix_net_write_timeout(THD *thd __attribute__(unused),
				  enum_var_type type __attribute__(unused))
{}
static void fix_net_retry_count(THD *thd __attribute__(unused),
				enum_var_type type __attribute__(unused))
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

bool sys_var_long_ptr::check(THD *thd, set_var *var)
{
  longlong v= var->value->val_int();
  var->save_result.ulonglong_value= v < 0 ? 0 : v;
  return 0;
}

bool sys_var_long_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (ulong) getopt_ull_limit_value(tmp, option_limits);
  else
    *value= (ulong) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_long_ptr::set_default(THD *thd, enum_var_type type)
{
  *value= (ulong) option_limits->def_value;
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


bool sys_var::check_enum(THD *thd, set_var *var, TYPELIB *enum_names)
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
    if (tmp >= enum_names->count)
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
  switch (type()) {
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
    return new Item_int((int32) *(my_bool*) value_ptr(thd, var_type, base),1);
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


CHARSET_INFO ** sys_var_character_set_connection::ci_ptr(THD *thd,
							 enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_connection;
  else
    return &thd->variables.collation_connection;
}


void sys_var_character_set_connection::set_default(THD *thd,
						   enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_connection= default_charset_info;
 else
 {
   thd->variables.collation_connection= global_system_variables.collation_connection;
   thd->update_charset();
 }
}


CHARSET_INFO ** sys_var_character_set_client::ci_ptr(THD *thd,
						     enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.character_set_client;
  else
    return &thd->variables.character_set_client;
}


void sys_var_character_set_client::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.character_set_client= default_charset_info;
 else
 {
   thd->variables.character_set_client= (global_system_variables.
					 character_set_client);
   thd->update_charset();
 }
}


CHARSET_INFO **
sys_var_character_set_results::ci_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.character_set_results;
  else
    return &thd->variables.character_set_results;
}


void sys_var_character_set_results::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.character_set_results= default_charset_info;
 else
 {
   thd->variables.character_set_results= (global_system_variables.
					  character_set_results);
   thd->update_charset();
 }
}


CHARSET_INFO **
sys_var_character_set_server::ci_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_server;
  else
    return &thd->variables.collation_server;
}


void sys_var_character_set_server::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_server= default_charset_info;
 else
 {
   thd->variables.collation_server= global_system_variables.collation_server;
   thd->update_charset();
 }
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


bool sys_var_collation_connection::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_connection= var->save_result.charset;
  else
  {
    thd->variables.collation_connection= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_connection::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_connection :
		  thd->variables.collation_connection);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_connection::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_connection= default_charset_info;
 else
 {
   thd->variables.collation_connection= (global_system_variables.
					 collation_connection);
   thd->update_charset();
 }
}

bool sys_var_collation_database::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_database= var->save_result.charset;
  else
  {
    thd->variables.collation_database= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_database::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_database :
		  thd->variables.collation_database);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_database::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_database= default_charset_info;
 else
 {
   thd->variables.collation_database= (global_system_variables.
					 collation_database);
   thd->update_charset();
 }
}


bool sys_var_collation_server::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_server= var->save_result.charset;
  else
  {
    thd->variables.collation_server= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_server::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_server :
		  thd->variables.collation_server);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_server::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_server= default_charset_info;
 else
 {
   thd->variables.collation_server= (global_system_variables.
					 collation_server);
   thd->update_charset();
 }
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
  thd->protocol_simple.init(thd);
  thd->protocol_prep.init(thd);
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
  thd->insert_id(var->save_result.ulonglong_value);
  return 0;
}


byte *sys_var_last_insert_id::value_ptr(THD *thd, enum_var_type type,
					LEX_STRING *base)
{
  thd->sys_var_tmp.long_value= (long) thd->insert_id();
  return (byte*) &thd->last_insert_id;
}


bool sys_var_insert_id::update(THD *thd, set_var *var)
{
  thd->next_insert_id= var->save_result.ulonglong_value;
  return 0;
}


byte *sys_var_insert_id::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  return (byte*) &thd->current_insert_id;
}


#ifdef HAVE_REPLICATION
bool sys_var_slave_skip_counter::check(THD *thd, set_var *var)
{
  int result= 0;
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  if (active_mi->rli.slave_running)
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    result=1;
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  var->save_result.ulong_value= (ulong) var->value->val_int();
  return result;
}


bool sys_var_slave_skip_counter::update(THD *thd, set_var *var)
{
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  /*
    The following test should normally never be true as we test this
    in the check function;  To be safe against multiple
    SQL_SLAVE_SKIP_COUNTER request, we do the check anyway
  */
  if (!active_mi->rli.slave_running)
  {
    pthread_mutex_lock(&active_mi->rli.data_lock);
    active_mi->rli.slave_skip_counter= var->save_result.ulong_value;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}


bool sys_var_sync_binlog_period::update(THD *thd, set_var *var)
{
  pthread_mutex_t *lock_log= mysql_bin_log.get_log_lock();
  sync_binlog_period= (ulong) var->save_result.ulonglong_value;
  return 0;
}
#endif /* HAVE_REPLICATION */

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

  if (!(var->save_result.time_zone=
        my_tz_find(res, thd->lex->time_zone_tables_used)))
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
     global_system_variables.time_zone=
       my_tz_find(&str, thd->lex->time_zone_tables_used);
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
      QQ: May be we should have a separate error message for this?
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

  ulong org_options=thd->options;

  if (var->save_result.ulong_value != 0)
    thd->options&= ~((sys_var_thd_bit*) var->var)->bit_flag;
  else
    thd->options|= ((sys_var_thd_bit*) var->var)->bit_flag;

  if ((org_options ^ thd->options) & OPTION_NOT_AUTOCOMMIT)
  {
    if ((org_options & OPTION_NOT_AUTOCOMMIT))
    {
      /* We changed to auto_commit mode */
      thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_STATUS_NO_TRANS_UPDATE);
      thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
      if (ha_commit(thd))
	return 1;
    }
    else
    {
      thd->options&= ~(ulong) (OPTION_STATUS_NO_TRANS_UPDATE);
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
    ((sys_var_thd_bit*) var->var)->bit_flag|= (OPTION_BIN_LOG |
					       OPTION_UPDATE_LOG);
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

static bool set_log_bin(THD *thd, set_var *var)
{
  if (opt_sql_bin_update)
    ((sys_var_thd_bit*) var->var)->bit_flag|= (OPTION_BIN_LOG |
					       OPTION_UPDATE_LOG);
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
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_WARN]);
  return (byte*) &thd->sys_var_tmp.long_value;
}

static byte *get_error_count(THD *thd)
{
  thd->sys_var_tmp.long_value= 
    thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR];
  return (byte*) &thd->sys_var_tmp.long_value;
}


static byte *get_have_innodb(THD *thd)
{
  return (byte*) show_comp_option_name[have_innodb];
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
  Initialises sys variables and put them in system_variable_hash
*/


void set_var_init()
{
  hash_init(&system_variable_hash, system_charset_info,
	    array_elements(sys_variables),0,0,
	    (hash_get_key) get_sys_var_length,0,0);
  sys_var **var, **end;
  for (var= sys_variables, end= sys_variables+array_elements(sys_variables) ;
       var < end;
       var++)
  {
    (*var)->name_length= strlen((*var)->name);
    (*var)->option_limits= find_option(my_long_options, (*var)->name);
    my_hash_insert(&system_variable_hash, (byte*) *var);
  }
  /*
    Special cases
    Needed because MySQL can't find the limits for a variable it it has
    a different name than the command line option.
    As these variables are deprecated, this code will disappear soon...
  */
  sys_sql_max_join_size.option_limits= sys_max_join_size.option_limits;
}


void set_var_free()
{
  hash_free(&system_variable_hash);
}


/*
  Find a user set-table variable

  SYNOPSIS
    find_sys_var()
    str		Name of system variable to find
    length	Length of variable.  zero means that we should use strlen()
		on the variable

  RETURN VALUES
    pointer	pointer to variable definitions
    0		Unknown variable (error message is given)
*/

sys_var *find_sys_var(const char *str, uint length)
{
  sys_var *var= (sys_var*) hash_search(&system_variable_hash,
				       (byte*) str,
				       length ? length :
				       strlen(str));
  if (!var)
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
	  user_var_item->check()) ? -1 : 0;
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

  if (var->value->result_type() == STRING_RESULT)
  {
    enum db_type db_type;
    if (!(res=var->value->val_str(&str)) ||
	!(var->save_result.ulong_value=
          (ulong) (db_type= ha_resolve_by_name(res->ptr(), res->length()))) ||
        ha_checktype(thd, db_type, 1, 0) != db_type)
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
  ulong val;
  val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
        thd->variables.*offset);
  const char *table_type= ha_get_storage_engine((enum db_type)val);
  return (byte *) table_type;
}


void sys_var_thd_storage_engine::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) DB_TYPE_MYISAM;
  else
    thd->variables.*offset= (ulong) (global_system_variables.*offset);
}


bool sys_var_thd_storage_engine::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.ulong_value;
  else
    thd->variables.*offset= var->save_result.ulong_value;
  return 0;
}

void sys_var_thd_table_type::warn_deprecated(THD *thd)
{
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		      ER_WARN_DEPRECATED_SYNTAX,
		      ER(ER_WARN_DEPRECATED_SYNTAX), "table_type",
                      "storage_engine"); 
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
    len   out pointer on length of string

  RETURN
    pointer to string with sql_mode representation
*/

byte *sys_var_thd_sql_mode::symbolic_mode_representation(THD *thd, ulong val,
                                                         ulong *len)
{
  char buff[256];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  ulong length;

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

  if ((length= tmp.length()))
    length--;
  *len= length;
  return (byte*) thd->strmake(tmp.ptr(), length);
}


byte *sys_var_thd_sql_mode::value_ptr(THD *thd, enum_var_type type,
				      LEX_STRING *base)
{
  ulong val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
              thd->variables.*offset);
  ulong length_unused;
  return symbolic_mode_representation(thd, val, &length_unused);
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
    sql_mode|= MODE_NO_FIELD_OPTIONS | MODE_HIGH_NOT_PRECEDENCE;
  if (sql_mode & MODE_MYSQL323)
    sql_mode|= MODE_NO_FIELD_OPTIONS | MODE_HIGH_NOT_PRECEDENCE;
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


/****************************************************************************
  Used templates
****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<set_var_base>;
template class List_iterator_fast<set_var_base>;
template class I_List_iterator<NAMED_LIST>;
#endif

/*
   Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file

  @brief
  Handling of MySQL SQL variables

  @details
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

  @todo
    Add full support for the variable character_set (for 4.1)

  @todo
    When updating myisam_delay_key_write, we should do a 'flush tables'
    of all MyISAM tables to ensure that they are reopen with the
    new attribute.

  @note
    Be careful with var->save_result: sys_var::check() only updates
    ulonglong_value; so other members of the union are garbage then; to use
    them you must first assign a value to them (in specific ::check() for
    example).
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
#endif

extern CHARSET_INFO *character_set_filesystem;


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

static const char *slave_exec_mode_names[]= { "STRICT", "IDEMPOTENT", NullS };
static unsigned int slave_exec_mode_names_len[]= { sizeof("STRICT") - 1,
                                                   sizeof("IDEMPOTENT") - 1, 0 };
TYPELIB slave_exec_mode_typelib=
{
  array_elements(slave_exec_mode_names)-1, "",
  slave_exec_mode_names, slave_exec_mode_names_len
};

static int  sys_check_ftb_syntax(THD *thd,  set_var *var);
static bool sys_update_ftb_syntax(THD *thd, set_var * var);
static void sys_default_ftb_syntax(THD *thd, enum_var_type type);
static bool sys_update_init_connect(THD*, set_var*);
static void sys_default_init_connect(THD*, enum_var_type type);
static bool sys_update_init_slave(THD*, set_var*);
static void sys_default_init_slave(THD*, enum_var_type type);
static bool set_option_bit(THD *thd, set_var *var);
static bool set_option_log_bin_bit(THD *thd, set_var *var);
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
bool throw_bounds_warning(THD *thd, bool fixed, bool unsignd,
                          const char *name, longlong val);
static KEY_CACHE *create_key_cache(const char *name, uint length);
void fix_sql_mode_var(THD *thd, enum_var_type type);
static uchar *get_error_count(THD *thd);
static uchar *get_warning_count(THD *thd);
static uchar *get_tmpdir(THD *thd);
static int  sys_check_log_path(THD *thd,  set_var *var);
static bool sys_update_general_log_path(THD *thd, set_var * var);
static void sys_default_general_log_path(THD *thd, enum_var_type type);
static bool sys_update_slow_log_path(THD *thd, set_var * var);
static void sys_default_slow_log_path(THD *thd, enum_var_type type);
static uchar *get_myisam_mmap_size(THD *thd);
static int check_max_allowed_packet(THD *thd,  set_var *var);
static int check_net_buffer_length(THD *thd,  set_var *var);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order.

  The variables are linked into the list. A variable is added to
  it in the constructor (see sys_var class for details).
*/

static sys_var_chain vars = { NULL, NULL };

static sys_var_thd_ulong
sys_auto_increment_increment(&vars, "auto_increment_increment",
                             &SV::auto_increment_increment, NULL, NULL,
                             sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_thd_ulong
sys_auto_increment_offset(&vars, "auto_increment_offset",
                          &SV::auto_increment_offset, NULL, NULL,
                          sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_bool_ptr	sys_automatic_sp_privileges(&vars, "automatic_sp_privileges",
					      &sp_automatic_privileges);

static sys_var_const            sys_back_log(&vars, "back_log",
                                             OPT_GLOBAL, SHOW_LONG,
                                             (uchar*) &back_log);
static sys_var_const_os_str       sys_basedir(&vars, "basedir", mysql_home);
static sys_var_long_ptr	sys_binlog_cache_size(&vars, "binlog_cache_size",
					      &binlog_cache_size);
static sys_var_thd_binlog_format sys_binlog_format(&vars, "binlog_format",
                                            &SV::binlog_format);
static sys_var_thd_bool sys_binlog_direct_non_trans_update(&vars, "binlog_direct_non_transactional_updates",
                                                           &SV::binlog_direct_non_trans_update);
static sys_var_thd_ulong	sys_bulk_insert_buff_size(&vars, "bulk_insert_buffer_size",
						  &SV::bulk_insert_buff_size);
static sys_var_const_os         sys_character_sets_dir(&vars,
                                                       "character_sets_dir",
                                                       OPT_GLOBAL, SHOW_CHAR,
                                                       (uchar*)
                                                       mysql_charsets_dir);
static sys_var_character_set_sv
sys_character_set_server(&vars, "character_set_server",
                         &SV::collation_server, &default_charset_info, 0,
                         sys_var::SESSION_VARIABLE_IN_BINLOG);
sys_var_const_str       sys_charset_system(&vars, "character_set_system",
                                           (char *)my_charset_utf8_general_ci.name);
static sys_var_character_set_database
sys_character_set_database(&vars, "character_set_database",
                           sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_character_set_client
sys_character_set_client(&vars, "character_set_client",
                         &SV::character_set_client,
                         &default_charset_info,
                         sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_character_set_sv
sys_character_set_connection(&vars, "character_set_connection",
                             &SV::collation_connection,
                             &default_charset_info, 0,
                             sys_var::SESSION_VARIABLE_IN_BINLOG);
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
static sys_var_collation_sv
sys_collation_connection(&vars, "collation_connection",
                         &SV::collation_connection, &default_charset_info,
                         sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_collation_sv
sys_collation_database(&vars, "collation_database", &SV::collation_database,
                       &default_charset_info,
                       sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_collation_sv
sys_collation_server(&vars, "collation_server", &SV::collation_server,
                     &default_charset_info,
                     sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_long_ptr	sys_concurrent_insert(&vars, "concurrent_insert",
                                              &myisam_concurrent_insert);
static sys_var_long_ptr	sys_connect_timeout(&vars, "connect_timeout",
					    &connect_timeout);
static sys_var_const_os_str       sys_datadir(&vars, "datadir", mysql_real_data_home);
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

#ifdef HAVE_EVENT_SCHEDULER
static sys_var_event_scheduler sys_event_scheduler(&vars, "event_scheduler");
#endif

static sys_var_long_ptr	sys_expire_logs_days(&vars, "expire_logs_days",
					     &expire_logs_days);
static sys_var_bool_ptr	sys_flush(&vars, "flush", &myisam_flush);
static sys_var_long_ptr	sys_flush_time(&vars, "flush_time", &flush_time);
static sys_var_str      sys_ft_boolean_syntax(&vars, "ft_boolean_syntax",
                                              sys_check_ftb_syntax,
                                              sys_update_ftb_syntax,
                                              sys_default_ftb_syntax,
                                              ft_boolean_syntax);
static sys_var_const    sys_ft_max_word_len(&vars, "ft_max_word_len",
                                            OPT_GLOBAL, SHOW_LONG,
                                            (uchar*) &ft_max_word_len);
static sys_var_const    sys_ft_min_word_len(&vars, "ft_min_word_len",
                                            OPT_GLOBAL, SHOW_LONG,
                                            (uchar*) &ft_min_word_len);
static sys_var_const    sys_ft_query_expansion_limit(&vars,
                                                     "ft_query_expansion_limit",
                                                     OPT_GLOBAL, SHOW_LONG,
                                                     (uchar*)
                                                     &ft_query_expansion_limit);
static sys_var_const    sys_ft_stopword_file(&vars, "ft_stopword_file",
                                             OPT_GLOBAL, SHOW_CHAR_PTR,
                                             (uchar*) &ft_stopword_file);

static sys_var_const    sys_ignore_builtin_innodb(&vars, "ignore_builtin_innodb",
                                                  OPT_GLOBAL, SHOW_BOOL,
                                                  (uchar*) &opt_ignore_builtin_innodb);

sys_var_str             sys_init_connect(&vars, "init_connect", 0,
                                         sys_update_init_connect,
                                         sys_default_init_connect,0);
static sys_var_const    sys_init_file(&vars, "init_file",
                                      OPT_GLOBAL, SHOW_CHAR_PTR,
                                      (uchar*) &opt_init_file);
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
static sys_var_const    sys_language(&vars, "language",
                                     OPT_GLOBAL, SHOW_CHAR,
                                     (uchar*) language);
static sys_var_const    sys_large_files_support(&vars, "large_files_support",
                                                OPT_GLOBAL, SHOW_BOOL,
                                                (uchar*) &opt_large_files);
static sys_var_const    sys_large_page_size(&vars, "large_page_size",
                                            OPT_GLOBAL, SHOW_INT,
                                            (uchar*) &opt_large_page_size);
static sys_var_const    sys_large_pages(&vars, "large_pages",
                                        OPT_GLOBAL, SHOW_MY_BOOL,
                                        (uchar*) &opt_large_pages);
static sys_var_bool_ptr	sys_local_infile(&vars, "local_infile",
					 &opt_local_infile);
#ifdef HAVE_MLOCKALL
static sys_var_const    sys_locked_in_memory(&vars, "locked_in_memory",
                                             OPT_GLOBAL, SHOW_MY_BOOL,
                                             (uchar*) &locked_in_memory);
#endif
static sys_var_const    sys_log_bin(&vars, "log_bin",
                                    OPT_GLOBAL, SHOW_BOOL,
                                    (uchar*) &opt_bin_log);
static sys_var_trust_routine_creators
sys_trust_routine_creators(&vars, "log_bin_trust_routine_creators",
                           &trust_function_creators);
static sys_var_bool_ptr       
sys_trust_function_creators(&vars, "log_bin_trust_function_creators",
                            &trust_function_creators);
static sys_var_const    sys_log_error(&vars, "log_error",
                                      OPT_GLOBAL, SHOW_CHAR,
                                      (uchar*) log_error_file);
static sys_var_bool_ptr
  sys_log_queries_not_using_indexes(&vars, "log_queries_not_using_indexes",
                                    &opt_log_queries_not_using_indexes);
static sys_var_thd_ulong	sys_log_warnings(&vars, "log_warnings", &SV::log_warnings);
static sys_var_microseconds	sys_var_long_query_time(&vars, "long_query_time",
                                                        &SV::long_query_time);
static sys_var_thd_bool	sys_low_priority_updates(&vars, "low_priority_updates",
						 &SV::low_priority_updates,
						 fix_low_priority_updates);
#ifndef TO_BE_DELETED	/* Alias for the low_priority_updates */
static sys_var_thd_bool	sys_sql_low_priority_updates(&vars, "sql_low_priority_updates",
						     &SV::low_priority_updates,
						     fix_low_priority_updates);
#endif
static sys_var_const    sys_lower_case_file_system(&vars,
                                                   "lower_case_file_system",
                                                   OPT_GLOBAL, SHOW_MY_BOOL,
                                                   (uchar*)
                                                   &lower_case_file_system);
static sys_var_const    sys_lower_case_table_names(&vars,
                                                   "lower_case_table_names",
                                                   OPT_GLOBAL, SHOW_INT,
                                                   (uchar*)
                                                   &lower_case_table_names);
static sys_var_thd_ulong_session_readonly sys_max_allowed_packet(&vars, "max_allowed_packet",
					       &SV::max_allowed_packet,
                                               check_max_allowed_packet);
static sys_var_long_ptr sys_slave_max_allowed_packet(&vars, "slave_max_allowed_packet",
                                              &slave_max_allowed_packet); 
static sys_var_ulonglong_ptr sys_max_binlog_cache_size(&vars, "max_binlog_cache_size",
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
static sys_var_thd_ulong sys_pseudo_thread_id(&vars, "pseudo_thread_id",
                                              &SV::pseudo_thread_id,
                                              check_pseudo_thread_id, 0,
                                              sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_thd_ha_rows	sys_max_join_size(&vars, "max_join_size",
					  &SV::max_join_size,
					  fix_max_join_size);
static sys_var_thd_ulong	sys_max_seeks_for_key(&vars, "max_seeks_for_key",
					      &SV::max_seeks_for_key);
static sys_var_thd_ulong   sys_max_length_for_sort_data(&vars, "max_length_for_sort_data",
                                                 &SV::max_length_for_sort_data);
static sys_var_const    sys_max_long_data_size(&vars,
                                               "max_long_data_size",
                                               OPT_GLOBAL, SHOW_LONG,
                                               (uchar*)
                                               &max_long_data_size);

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
static sys_var_thd_ulong       sys_min_examined_row_limit(&vars, "min_examined_row_limit",
                                                          &SV::min_examined_row_limit);
static sys_var_thd_ulong       sys_multi_range_count(&vars, "multi_range_count",
                                              &SV::multi_range_count);
static sys_var_long_ptr	sys_myisam_data_pointer_size(&vars, "myisam_data_pointer_size",
                                                    &myisam_data_pointer_size);
static sys_var_thd_ulonglong	sys_myisam_max_sort_file_size(&vars, "myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
static sys_var_const sys_myisam_recover_options(&vars, "myisam_recover_options",
                                                OPT_GLOBAL, SHOW_CHAR_PTR,
                                                (uchar*)
                                                &myisam_recover_options_str);
static sys_var_thd_ulong       sys_myisam_repair_threads(&vars, "myisam_repair_threads", &SV::myisam_repair_threads);
static sys_var_thd_ulong	sys_myisam_sort_buffer_size(&vars, "myisam_sort_buffer_size", &SV::myisam_sort_buff_size);
static sys_var_bool_ptr	sys_myisam_use_mmap(&vars, "myisam_use_mmap",
                                            &opt_myisam_use_mmap);

static sys_var_thd_enum         sys_myisam_stats_method(&vars, "myisam_stats_method",
                                                &SV::myisam_stats_method,
                                                &myisam_stats_method_typelib,
                                                NULL);

#ifdef __NT__
/* purecov: begin inspected */
static sys_var_const            sys_named_pipe(&vars, "named_pipe",
                                               OPT_GLOBAL, SHOW_MY_BOOL,
                                               (uchar*) &opt_enable_named_pipe);
/* purecov: end */
#endif
static sys_var_thd_ulong_session_readonly sys_net_buffer_length(&vars, "net_buffer_length",
					      &SV::net_buffer_length,
                                              check_net_buffer_length);
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
static sys_var_const            sys_open_files_limit(&vars, "open_files_limit",
                                                     OPT_GLOBAL, SHOW_LONG,
                                                     (uchar*)
                                                     &open_files_limit);
static sys_var_thd_ulong        sys_optimizer_prune_level(&vars, "optimizer_prune_level",
                                                  &SV::optimizer_prune_level);
static sys_var_thd_ulong        sys_optimizer_search_depth(&vars, "optimizer_search_depth",
                                                   &SV::optimizer_search_depth);
static sys_var_thd_optimizer_switch   sys_optimizer_switch(&vars, "optimizer_switch",
                                     &SV::optimizer_switch);
static sys_var_const            sys_pid_file(&vars, "pid_file",
                                             OPT_GLOBAL, SHOW_CHAR,
                                             (uchar*) pidfile_name);
static sys_var_const_os         sys_plugin_dir(&vars, "plugin_dir",
                                               OPT_GLOBAL, SHOW_CHAR,
                                               (uchar*) opt_plugin_dir);
static sys_var_const            sys_port(&vars, "port",
                                         OPT_GLOBAL, SHOW_INT,
                                         (uchar*) &mysqld_port);
static sys_var_thd_ulong        sys_preload_buff_size(&vars, "preload_buffer_size",
                                              &SV::preload_buff_size);
static sys_var_const            sys_protocol_version(&vars, "protocol_version",
                                                     OPT_GLOBAL, SHOW_INT,
                                                     (uchar*)
                                                     &protocol_version);
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
#ifdef HAVE_SMEM
/* purecov: begin tested */
static sys_var_const    sys_shared_memory(&vars, "shared_memory",
                                          OPT_GLOBAL, SHOW_MY_BOOL,
                                          (uchar*)
                                          &opt_enable_shared_memory);
static sys_var_const    sys_shared_memory_base_name(&vars,
                                                    "shared_memory_base_name",
                                                    OPT_GLOBAL, SHOW_CHAR_PTR,
                                                    (uchar*)
                                                    &shared_memory_base_name);
/* purecov: end */
#endif
static sys_var_const    sys_skip_external_locking(&vars,
                                                  "skip_external_locking",
                                                  OPT_GLOBAL, SHOW_MY_BOOL,
                                                  (uchar*)
                                                  &my_disable_locking);
static sys_var_const    sys_skip_networking(&vars, "skip_networking",
                                            OPT_GLOBAL, SHOW_BOOL,
                                            (uchar*) &opt_disable_networking);
static sys_var_const    sys_skip_show_database(&vars, "skip_show_database",
                                            OPT_GLOBAL, SHOW_BOOL,
                                            (uchar*) &opt_skip_show_db);

static sys_var_const    sys_skip_name_resolve(&vars, "skip_name_resolve",
                                            OPT_GLOBAL, SHOW_BOOL,
                                            (uchar*) &opt_skip_name_resolve);

static sys_var_const    sys_socket(&vars, "socket",
                                   OPT_GLOBAL, SHOW_CHAR_PTR,
                                   (uchar*) &mysqld_unix_port);

#ifdef HAVE_THR_SETCONCURRENCY
/* purecov: begin tested */
static sys_var_const    sys_thread_concurrency(&vars, "thread_concurrency",
                                               OPT_GLOBAL, SHOW_LONG,
                                               (uchar*) &concurrency);
/* purecov: end */
#endif
static sys_var_const    sys_thread_stack(&vars, "thread_stack",
                                         OPT_GLOBAL, SHOW_LONG,
                                         (uchar*) &my_thread_stack_size);
static sys_var_readonly_os      sys_tmpdir(&vars, "tmpdir", OPT_GLOBAL, SHOW_CHAR, get_tmpdir);
static sys_var_thd_ulong	sys_trans_alloc_block_size(&vars, "transaction_alloc_block_size",
						   &SV::trans_alloc_block_size,
						   0, fix_trans_mem_root);
static sys_var_thd_ulong	sys_trans_prealloc_size(&vars, "transaction_prealloc_size",
						&SV::trans_prealloc_size,
						0, fix_trans_mem_root);
sys_var_enum_const      sys_thread_handling(&vars, "thread_handling",
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
static sys_var_set_slave_mode slave_exec_mode(&vars,
                                              "slave_exec_mode",
                                              &slave_exec_mode_options,
                                              &slave_exec_mode_typelib,
                                              0);
static sys_var_long_ptr	sys_slow_launch_time(&vars, "slow_launch_time",
					     &slow_launch_time);
static sys_var_thd_ulong	sys_sort_buffer(&vars, "sort_buffer_size",
					&SV::sortbuff_size);
/*
  sql_mode should *not* have binlog_mode=SESSION_VARIABLE_IN_BINLOG:
  even though it is written to the binlog, the slave ignores the
  MODE_NO_DIR_IN_CREATE variable, so slave's value differs from
  master's (see log_event.cc: Query_log_event::do_apply_event()).
*/
static sys_var_thd_sql_mode    sys_sql_mode(&vars, "sql_mode",
                                            &SV::sql_mode);
#ifdef HAVE_OPENSSL
extern char *opt_ssl_ca, *opt_ssl_capath, *opt_ssl_cert, *opt_ssl_cipher,
            *opt_ssl_key;
static sys_var_const_os_str_ptr	sys_ssl_ca(&vars, "ssl_ca", &opt_ssl_ca);
static sys_var_const_os_str_ptr	sys_ssl_capath(&vars, "ssl_capath", &opt_ssl_capath);
static sys_var_const_os_str_ptr	sys_ssl_cert(&vars, "ssl_cert", &opt_ssl_cert);
static sys_var_const_os_str_ptr	sys_ssl_cipher(&vars, "ssl_cipher", &opt_ssl_cipher);
static sys_var_const_os_str_ptr	sys_ssl_key(&vars, "ssl_key", &opt_ssl_key);
#else
static sys_var_const_os_str	sys_ssl_ca(&vars, "ssl_ca", NULL);
static sys_var_const_os_str	sys_ssl_capath(&vars, "ssl_capath", NULL);
static sys_var_const_os_str	sys_ssl_cert(&vars, "ssl_cert", NULL);
static sys_var_const_os_str	sys_ssl_cipher(&vars, "ssl_cipher", NULL);
static sys_var_const_os_str	sys_ssl_key(&vars, "ssl_key", NULL);
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

#if defined(ENABLED_DEBUG_SYNC)
/* Debug Sync Facility. Implemented in debug_sync.cc. */
static sys_var_debug_sync sys_debug_sync(&vars, "debug_sync");
#endif /* defined(ENABLED_DEBUG_SYNC) */

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
                                       set_option_log_bin_bit,
				       OPTION_BIN_LOG);
static sys_var_thd_bit	sys_sql_warnings(&vars, "sql_warnings", 0,
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_thd_bit	sys_sql_notes(&vars, "sql_notes", 0,
					 set_option_bit,
					 OPTION_SQL_NOTES);
static sys_var_thd_bit	sys_auto_is_null(&vars, "sql_auto_is_null", 0,
					 set_option_bit,
                                         OPTION_AUTO_IS_NULL, 0,
                                         sys_var::SESSION_VARIABLE_IN_BINLOG);
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
                                               1, sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_thd_bit	sys_unique_checks(&vars, "unique_checks", 0,
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS,
                                          1,
                                          sys_var::SESSION_VARIABLE_IN_BINLOG);
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
static sys_var_thd_bit  sys_profiling(&vars, "profiling", NULL, 
                                      set_option_bit,
                                      ulonglong(OPTION_PROFILING));
static sys_var_thd_ulong	sys_profiling_history_size(&vars, "profiling_history_size",
					      &SV::profiling_history_size);
#endif

/* Local state variables */

static sys_var_thd_ha_rows	sys_select_limit(&vars, "sql_select_limit",
						 &SV::select_limit);
static sys_var_timestamp sys_timestamp(&vars, "timestamp",
                                       sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_last_insert_id
sys_last_insert_id(&vars, "last_insert_id",
                   sys_var::SESSION_VARIABLE_IN_BINLOG);
/*
  identity is an alias for last_insert_id(), so that we are compatible
  with Sybase
*/
static sys_var_last_insert_id
sys_identity(&vars, "identity", sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_thd_lc_time_names
sys_lc_time_names(&vars, "lc_time_names", sys_var::SESSION_VARIABLE_IN_BINLOG);

/*
  insert_id should *not* be marked as written to the binlog (i.e., it
  should *not* have binlog_status==SESSION_VARIABLE_IN_BINLOG),
  because we want any statement that refers to insert_id explicitly to
  be unsafe.  (By "explicitly", we mean using @@session.insert_id,
  whereas insert_id is used "implicitly" when NULL value is inserted
  into an auto_increment column).

  We want statements referring explicitly to @@session.insert_id to be
  unsafe, because insert_id is modified internally by the slave sql
  thread when NULL values are inserted in an AUTO_INCREMENT column.
  This modification interfers with the value of the
  @@session.insert_id variable if @@session.insert_id is referred
  explicitly by an insert statement (as is seen by executing "SET
  @@session.insert_id=0; CREATE TABLE t (a INT, b INT KEY
  AUTO_INCREMENT); INSERT INTO t(a) VALUES (@@session.insert_id);" in
  statement-based logging mode: t will be different on master and
  slave).
*/
static sys_var_insert_id sys_insert_id(&vars, "insert_id");
static sys_var_readonly		sys_error_count(&vars, "error_count",
						OPT_SESSION,
						SHOW_LONG,
						get_error_count);
static sys_var_readonly		sys_warning_count(&vars, "warning_count",
						  OPT_SESSION,
						  SHOW_LONG,
						  get_warning_count);

static sys_var_rand_seed1 sys_rand_seed1(&vars, "rand_seed1",
                                         sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_rand_seed2 sys_rand_seed2(&vars, "rand_seed2",
                                         sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_thd_ulong        sys_default_week_format(&vars, "default_week_format",
					                &SV::default_week_format);

sys_var_thd_ulong               sys_group_concat_max_len(&vars, "group_concat_max_len",
                                                         &SV::group_concat_max_len);

sys_var_thd_time_zone sys_time_zone(&vars, "time_zone",
                                    sys_var::SESSION_VARIABLE_IN_BINLOG);

/* Global read-only variable containing hostname */
static sys_var_const_str        sys_hostname(&vars, "hostname", glob_hostname);

#ifndef EMBEDDED_LIBRARY
static sys_var_const_str_ptr    sys_repl_report_host(&vars, "report_host", &report_host);
static sys_var_const_str_ptr    sys_repl_report_user(&vars, "report_user", &report_user);
static sys_var_const_str_ptr    sys_repl_report_password(&vars, "report_password", &report_password);

static uchar *slave_get_report_port(THD *thd)
{
  thd->sys_var_tmp.long_value= report_port;
  return (uchar*) &thd->sys_var_tmp.long_value;
}

static sys_var_readonly    sys_repl_report_port(&vars, "report_port", OPT_GLOBAL, SHOW_LONG, slave_get_report_port);

#endif

sys_var_thd_bool  sys_keep_files_on_create(&vars, "keep_files_on_create", 
                                           &SV::keep_files_on_create);
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
static sys_var_have_variable sys_have_community_features(&vars, "have_community_features", &have_community_features);
static sys_var_have_variable sys_have_rtree_keys(&vars, "have_rtree_keys", &have_rtree_keys);
static sys_var_have_variable sys_have_symlink(&vars, "have_symlink", &have_symlink);
/* Global read-only variable describing server license */
static sys_var_const_str	sys_license(&vars, "license", STRINGIFY_ARG(LICENSE));
/* Global variables which enable|disable logging */
static sys_var_log_state sys_var_general_log(&vars, "general_log", &opt_log,
                                      QUERY_LOG_GENERAL);
/* Synonym of "general_log" for consistency with SHOW VARIABLES output */
static sys_var_log_state sys_var_log(&vars, "log", &opt_log,
                                      QUERY_LOG_GENERAL);
static sys_var_log_state sys_var_slow_query_log(&vars, "slow_query_log", &opt_slow_log,
                                         QUERY_LOG_SLOW);
/* Synonym of "slow_query_log" for consistency with SHOW VARIABLES output */
static sys_var_log_state sys_var_log_slow(&vars, "log_slow_queries",
                                          &opt_slow_log, QUERY_LOG_SLOW);
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
static sys_var_readonly         sys_myisam_mmap_size(&vars, "myisam_mmap_size",
                                                     OPT_GLOBAL,
                                                     SHOW_LONGLONG,
                                                     get_myisam_mmap_size);


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
  var_str->is_os_charset= FALSE;
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
    return (ft_boolean_check_syntax_string((uchar*)
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


/**
  If one sets the LOW_PRIORIY UPDATES flag, we also must change the
  used lock type.
*/

static void fix_low_priority_updates(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    thr_upgraded_concurrent_insert_lock= 
      (global_system_variables.low_priority_updates ?
       TL_WRITE_LOW_PRIORITY : TL_WRITE);
  else
    thd->update_lock_default= (thd->variables.low_priority_updates ?
			       TL_WRITE_LOW_PRIORITY : TL_WRITE);
}


static void
fix_myisam_max_sort_file_size(THD *thd, enum_var_type type)
{
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;
}

/**
  Set the OPTION_BIG_SELECTS flag if max_join_size == HA_POS_ERROR.
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


/**
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
  is only active for the next command.
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
    my_net_set_read_timeout(&thd->net, thd->variables.net_read_timeout);
}


static void fix_net_write_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    my_net_set_write_timeout(&thd->net, thd->variables.net_write_timeout);
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
  ulong new_cache_size= query_cache.resize(query_cache_size);

  /*
     Note: query_cache_size is a global variable reflecting the 
     requested cache size. See also query_cache_size_arg
  */

  if (query_cache_size != new_cache_size)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_QC_RESIZE, ER(ER_WARN_QC_RESIZE),
			query_cache_size, new_cache_size);
  
  query_cache_size= new_cache_size;
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

bool sys_var_set::update(THD *thd, set_var *var)
{
  *value= var->save_result.ulong_value;
  return 0;
}

uchar *sys_var_set::value_ptr(THD *thd, enum_var_type type,
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
      tmp.append(enum_names->type_names[i],
                 enum_names->type_lengths[i]);
      tmp.append(',');
    }
  }

  if ((length= tmp.length()))
    length--;
  return (uchar*) thd->strmake(tmp.ptr(), length);
}

void sys_var_set_slave_mode::set_default(THD *thd, enum_var_type type)
{
  slave_exec_mode_options= SLAVE_EXEC_MODE_STRICT;
}

bool sys_var_set_slave_mode::check(THD *thd, set_var *var)
{
  bool rc=  sys_var_set::check(thd, var);
  if (!rc && (var->save_result.ulong_value & SLAVE_EXEC_MODE_STRICT) &&
      (var->save_result.ulong_value & SLAVE_EXEC_MODE_IDEMPOTENT))
  {
    rc= true;
    my_error(ER_SLAVE_AMBIGOUS_EXEC_MODE, MYF(0), "");
  }
  return rc;
}

bool sys_var_set_slave_mode::update(THD *thd, set_var *var)
{
  bool rc;
  pthread_mutex_lock(&LOCK_global_system_variables);
  rc= sys_var_set::update(thd, var);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return rc;
}

void fix_slave_exec_mode(void)
{
  DBUG_ENTER("fix_slave_exec_mode");

  if ((slave_exec_mode_options & SLAVE_EXEC_MODE_STRICT) &&
      (slave_exec_mode_options & SLAVE_EXEC_MODE_IDEMPOTENT))
  {
    sql_print_error("Ambiguous slave modes combination. STRICT will be used");
    slave_exec_mode_options&= ~SLAVE_EXEC_MODE_IDEMPOTENT;
  }
  if (!(slave_exec_mode_options & SLAVE_EXEC_MODE_IDEMPOTENT))
    slave_exec_mode_options|= SLAVE_EXEC_MODE_STRICT;
  DBUG_VOID_RETURN;
}


bool sys_var_thd_binlog_format::check(THD *thd, set_var *var) {
  /*
    All variables that affect writing to binary log (either format or
    turning logging on and off) use the same checking. We call the
    superclass ::check function to assign the variable correctly, and
    then check the value.
   */
  bool result= sys_var_thd_enum::check(thd, var);
  if (!result)
    result= check_log_update(thd, var);
  return result;
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
  thd->server_id= server_id;
}


/**
  Throw warning (error in STRICT mode) if value for variable needed bounding.
  Only call from check(), not update(), because an error in update() would be
  bad mojo. Plug-in interface also uses this.

  @param thd      thread handle
  @param fixed    did we have to correct the value? (throw warn/err if so)
  @param unsignd  is value's type unsigned?
  @param name     variable's name
  @param val      variable's value

  @retval         TRUE on error, FALSE otherwise (warning or OK)
 */
bool throw_bounds_warning(THD *thd, bool fixed, bool unsignd,
                          const char *name, longlong val)
{
  if (fixed)
  {
    char buf[22];

    if (unsignd)
      ullstr((ulonglong) val, buf);
    else
      llstr(val, buf);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES)
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return TRUE;
    }

    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return FALSE;
}


/**
  Get unsigned system-variable.
  Negative value does not wrap around, but becomes zero.
  Check user-supplied value for a systemvariable against bounds.
  If we needed to adjust the value, throw a warning or error depending
  on SQL-mode.

  @param thd             thread handle
  @param var             the system-variable to get
  @param user_max        a limit given with --maximum-variable-name=... or 0
  @param var_type        function will bound on systems where necessary.

  @retval                TRUE on error, FALSE otherwise (warning or OK)
 */
static bool get_unsigned(THD *thd, set_var *var, ulonglong user_max,
                         ulong var_type)
{
  int                     warnings= 0;
  ulonglong               unadjusted;
  const struct my_option *limits= var->var->option_limits;
  struct my_option        fallback;

  /* get_unsigned() */
  if (var->value->unsigned_flag)
    var->save_result.ulonglong_value= (ulonglong) var->value->val_int();
  else
  {
    longlong v= var->value->val_int();
    var->save_result.ulonglong_value= (ulonglong) ((v < 0) ? 0 : v);
    if (v < 0)
    {
      warnings++;
      if (throw_bounds_warning(thd, TRUE, FALSE, var->var->name, v))
        return TRUE;  /* warning was promoted to error, give up */
    }
  }

  unadjusted= var->save_result.ulonglong_value;

  /* max, if any */

  if ((user_max > 0) && (unadjusted > user_max))
  {
    var->save_result.ulonglong_value= user_max;

    if ((warnings == 0) && throw_bounds_warning(thd, TRUE, TRUE,
                                                var->var->name,
                                                (longlong) unadjusted))
      return TRUE;

    warnings++;
  }

  /*
    if the sysvar doesn't have a proper bounds record but the check
    function would like bounding to ULONG where its size differs from
    that of ULONGLONG, we make up a bogus limits record here and let
    the usual suspects handle the actual limiting.
  */

  if (!limits && var_type != GET_ULL)
  {
    bzero(&fallback, sizeof(fallback));
    fallback.var_type= var_type;
    limits= &fallback;
  }

  /* fix_unsigned() */
  if (limits)
  {
    my_bool   fixed;

    var->save_result.ulonglong_value= getopt_ull_limit_value(var->save_result.
                                                             ulonglong_value,
                                                             limits, &fixed);

    if ((warnings == 0) && throw_bounds_warning(thd, fixed, TRUE,
                                                var->var->name,
                                                (longlong) unadjusted))
      return TRUE;
  }

  return FALSE;
}


sys_var_long_ptr::
sys_var_long_ptr(sys_var_chain *chain, const char *name_arg, ulong *value_ptr_arg,
                 sys_after_update_func after_update_arg)
  :sys_var_long_ptr_global(chain, name_arg, value_ptr_arg,
                           &LOCK_global_system_variables, after_update_arg)
{}


bool sys_var_long_ptr_global::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, 0, GET_ULONG);
}

bool sys_var_long_ptr_global::update(THD *thd, set_var *var)
{
  pthread_mutex_lock(guard);
  *value= (ulong) var->save_result.ulonglong_value;
  pthread_mutex_unlock(guard);
  return 0;
}


void sys_var_long_ptr_global::set_default(THD *thd, enum_var_type type)
{
  my_bool not_used;
  pthread_mutex_lock(guard);
  *value= (ulong) getopt_ull_limit_value((ulong) option_limits->def_value,
                                         option_limits, &not_used);
  pthread_mutex_unlock(guard);
}


bool sys_var_ulonglong_ptr::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, 0, GET_ULL);
}


bool sys_var_ulonglong_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= (ulonglong) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_ulonglong_ptr::set_default(THD *thd, enum_var_type type)
{
  my_bool not_used;
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= getopt_ull_limit_value((ulonglong) option_limits->def_value,
                                 option_limits, &not_used);
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


uchar *sys_var_enum::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  return (uchar*) enum_names->type_names[*value];
}


uchar *sys_var_enum_const::value_ptr(THD *thd, enum_var_type type,
                                     LEX_STRING *base)
{
  return (uchar*) enum_names->type_names[global_system_variables.*offset];
}

bool sys_var_thd_ulong::check(THD *thd, set_var *var)
{
  if (get_unsigned(thd, var, max_system_variables.*offset, GET_ULONG))
    return TRUE;
  DBUG_ASSERT(var->save_result.ulonglong_value <= ULONG_MAX);
  return ((check_func && (*check_func)(thd, var)));
}

bool sys_var_thd_ulong::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) var->save_result.ulonglong_value;
  else
    thd->variables.*offset= (ulong) var->save_result.ulonglong_value;

  return 0;
}


void sys_var_thd_ulong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    my_bool not_used;
    /* We will not come here if option_limits is not set */
    global_system_variables.*offset=
      (ulong) getopt_ull_limit_value((ulong) option_limits->def_value,
                                     option_limits, &not_used);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


uchar *sys_var_thd_ulong::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (uchar*) &(global_system_variables.*offset);
  return (uchar*) &(thd->variables.*offset);
}


bool sys_var_thd_ha_rows::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, max_system_variables.*offset,
#ifdef BIG_TABLES
                      GET_ULL
#else
                      GET_ULONG
#endif
                     );
}


bool sys_var_thd_ha_rows::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ha_rows)
                                     var->save_result.ulonglong_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ha_rows) var->save_result.ulonglong_value;
  return 0;
}


void sys_var_thd_ha_rows::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    my_bool not_used;
    /* We will not come here if option_limits is not set */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset=
      (ha_rows) getopt_ull_limit_value((ha_rows) option_limits->def_value,
                                       option_limits, &not_used);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


uchar *sys_var_thd_ha_rows::value_ptr(THD *thd, enum_var_type type,
				     LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (uchar*) &(global_system_variables.*offset);
  return (uchar*) &(thd->variables.*offset);
}

bool sys_var_thd_ulonglong::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, max_system_variables.*offset, GET_ULL);
}

bool sys_var_thd_ulonglong::update(THD *thd,  set_var *var)
{
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ulonglong)
                                     var->save_result.ulonglong_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ulonglong) var->save_result.ulonglong_value;
  return 0;
}


void sys_var_thd_ulonglong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    my_bool not_used;
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset=
      getopt_ull_limit_value((ulonglong) option_limits->def_value,
                             option_limits, &not_used);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


uchar *sys_var_thd_ulonglong::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (uchar*) &(global_system_variables.*offset);
  return (uchar*) &(thd->variables.*offset);
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


uchar *sys_var_thd_bool::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (uchar*) &(global_system_variables.*offset);
  return (uchar*) &(thd->variables.*offset);
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

    if (!m_allow_empty_value &&
        res->length() == 0)
    {
      buff[0]= 0;
      goto err;
    }

    var->save_result.ulong_value= ((ulong)
				   find_set(enum_names, res->c_ptr_safe(),
					    res->length(),
                                            NULL,
                                            &error, &error_len,
					    &not_used));
    if (error_len)
    {
      strmake(buff, error, min(sizeof(buff) - 1, error_len));
      goto err;
    }
  }
  else
  {
    ulonglong tmp= var->value->val_int();

    if (!m_allow_empty_value &&
        tmp == 0)
    {
      buff[0]= '0';
      buff[1]= 0;
      goto err;
    }

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


CHARSET_INFO *sys_var::charset(THD *thd)
{
  return is_os_charset ? thd->variables.character_set_filesystem : 
    system_charset_info;
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


uchar *sys_var_thd_enum::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  ulong tmp= ((type == OPT_GLOBAL) ?
	      global_system_variables.*offset :
	      thd->variables.*offset);
  return (uchar*) enum_names->type_names[tmp];
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


uchar *sys_var_thd_bit::value_ptr(THD *thd, enum_var_type type,
				 LEX_STRING *base)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  thd->sys_var_tmp.my_bool_value= ((thd->options & bit_flag) ?
				   !reverse : reverse);
  return (uchar*) &thd->sys_var_tmp.my_bool_value;
}


/** Update a date_time format variable based on given value. */

void sys_var_thd_date_time_format::update2(THD *thd, enum_var_type type,
					   DATE_TIME_FORMAT *new_value)
{
  DATE_TIME_FORMAT *old;
  DBUG_ENTER("sys_var_date_time_format::update2");
  DBUG_DUMP("positions", (uchar*) new_value->positions,
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


uchar *sys_var_thd_date_time_format::value_ptr(THD *thd, enum_var_type type,
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
    return (uchar*) res;
  }
  return (uchar*) (thd->variables.*offset)->format.str;
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


uchar *sys_var_character_set::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  CHARSET_INFO *cs= ci_ptr(thd,type)[0];
  return cs ? (uchar*) cs->csname : (uchar*) NULL;
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


bool sys_var_character_set_client::check(THD *thd, set_var *var)
{
  if (sys_var_character_set_sv::check(thd, var))
    return 1;
  /* Currently, UCS-2 cannot be used as a client character set */
  if (!is_supported_parser_charset(var->save_result.charset))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, 
             var->save_result.charset->csname);
    return 1;
  }
  return 0;
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


uchar *sys_var_collation_sv::value_ptr(THD *thd, enum_var_type type,
                                       LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		     global_system_variables.*offset : thd->variables.*offset);
  return cs ? (uchar*) cs->name : (uchar*) "NULL";
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


uchar *sys_var_key_cache_param::value_ptr(THD *thd, enum_var_type type,
					 LEX_STRING *base)
{
  KEY_CACHE *key_cache= get_key_cache(base);
  if (!key_cache)
    key_cache= &zero_key_cache;
  return (uchar*) key_cache + offset ;
}


bool sys_var_key_buffer_size::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, 0, GET_ULL);
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
    /* Key cache didn't exist */
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
      error= 1;
      my_error(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE, MYF(0));
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

  key_cache->param_buff_size= (ulonglong) tmp;

  /* If key cache didn't exist initialize it, else resize it */
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

 var->save_result.ulonglong_value = SIZE_T_MAX;

  return error;
}


bool sys_var_key_cache_long::check(THD *thd, set_var *var)
{
  return get_unsigned(thd, var, 0, GET_ULONG);
}


/**
  @todo
  Abort if some other thread is changing the key cache.
  This should be changed so that we wait until the previous
  assignment is done and then do the new assign
*/
bool sys_var_key_cache_long::update(THD *thd, set_var *var)
{
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

  *((ulong*) (((char*) key_cache) + offset))= (ulong)
                                              var->save_result.ulonglong_value;

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
  bool res;

  if (this == &sys_var_log)
    WARN_DEPRECATED(thd, "7.0", "@@log", "'@@general_log'");
  else if (this == &sys_var_log_slow)
    WARN_DEPRECATED(thd, "7.0", "@@log_slow_queries", "'@@slow_query_log'");

  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!var->save_result.ulong_value)
  {
    logger.deactivate_log_handler(thd, log_type);
    res= false;
  }
  else
    res= logger.activate_log_handler(thd, log_type);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return res;
}

void sys_var_log_state::set_default(THD *thd, enum_var_type type)
{
  if (this == &sys_var_log)
    WARN_DEPRECATED(thd, "7.0", "@@log", "'@@general_log'");
  else if (this == &sys_var_log_slow)
    WARN_DEPRECATED(thd, "7.0", "@@log_slow_queries", "'@@slow_query_log'");

  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.deactivate_log_handler(thd, log_type);
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


static int  sys_check_log_path(THD *thd,  set_var *var)
{
  char path[FN_REFLEN], buff[FN_REFLEN];
  MY_STAT f_stat;
  String str(buff, sizeof(buff), system_charset_info), *res;
  const char *log_file_str;
  size_t path_length;

  if (!(res= var->value->val_str(&str)))
    goto err;

  log_file_str= res->c_ptr();
  bzero(&f_stat, sizeof(MY_STAT));

  path_length= unpack_filename(path, log_file_str);

  if (!path_length)
  {
    /* File name is empty. */

    goto err;
  }

  if (!is_filename_allowed(log_file_str, strlen(log_file_str)))
    goto err;

  if (my_stat(path, &f_stat, MYF(0)))
  {
    /*
      A file system object exists. Check if argument is a file and we have
      'write' permission.
    */

    if (!MY_S_ISREG(f_stat.st_mode) ||
        !(f_stat.st_mode & MY_S_IWRITE))
      goto err;

    return 0;
  }

  /* Get dirname of the file path. */
  (void) dirname_part(path, log_file_str, &path_length);

  /* Dirname is empty if file path is relative. */
  if (!path_length)
    return 0;

  /*
    Check if directory exists and we have permission to create file and
    write to file.
  */
  if (my_access(path, (F_OK|W_OK)))
    goto err;

  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->var->name, 
           res ? log_file_str : "NULL");
  return 1;
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
    MY_ASSERT_UNREACHABLE();
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
  logger.lock_exclusive();

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
  logger.lock_exclusive();
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
  logger.lock_exclusive();
  logger.init_slow_log(LOG_FILE);
  logger.init_general_log(LOG_FILE);
  *value= LOG_FILE;
  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


uchar *sys_var_log_output::value_ptr(THD *thd, enum_var_type type,
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
  return (uchar*) thd->strmake(tmp.ptr(), length);
}


/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(THD *thd)
{
  /* Currently, UCS-2 cannot be used as a client character set */
  if (character_set_client->mbminlen > 1)
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
             character_set_client->csname);
    return 1;
  }
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

bool sys_var_timestamp::check(THD *thd, set_var *var)
{
  longlong val;
  var->save_result.ulonglong_value= var->value->val_int();
  val= (longlong) var->save_result.ulonglong_value;
  if (val != 0 &&          // this is how you set the default value
      (val < TIMESTAMP_MIN_VALUE || val > TIMESTAMP_MAX_VALUE))
  {
    char buf[64];
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "timestamp", llstr(val, buf));
    return TRUE;
  }
  return FALSE;
}


bool sys_var_timestamp::update(THD *thd,  set_var *var)
{
  thd->set_time((time_t) var->save_result.ulonglong_value);
  return FALSE;
}


void sys_var_timestamp::set_default(THD *thd, enum_var_type type)
{
  thd->user_time=0;
}


uchar *sys_var_timestamp::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  thd->sys_var_tmp.long_value= (long) thd->start_time;
  return (uchar*) &thd->sys_var_tmp.long_value;
}


bool sys_var_last_insert_id::update(THD *thd, set_var *var)
{
  thd->first_successful_insert_id_in_prev_stmt= 
    var->save_result.ulonglong_value;
  return 0;
}


uchar *sys_var_last_insert_id::value_ptr(THD *thd, enum_var_type type,
					LEX_STRING *base)
{
  /*
    this tmp var makes it robust againt change of type of 
    read_first_successful_insert_id_in_prev_stmt().
  */
  thd->sys_var_tmp.ulonglong_value= 
    thd->read_first_successful_insert_id_in_prev_stmt();
  return (uchar*) &thd->sys_var_tmp.ulonglong_value;
}


bool sys_var_insert_id::update(THD *thd, set_var *var)
{
  thd->force_one_auto_inc_interval(var->save_result.ulonglong_value);
  return 0;
}


uchar *sys_var_insert_id::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  thd->sys_var_tmp.ulonglong_value= 
    thd->auto_inc_intervals_forced.minimum();
  return (uchar*) &thd->sys_var_tmp.ulonglong_value;
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


uchar *sys_var_thd_time_zone::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  /* 
    We can use ptr() instead of c_ptr() here because String contaning
    time zone name is guaranteed to be zero ended.
  */
  if (type == OPT_GLOBAL)
    return (uchar *)(global_system_variables.time_zone->get_name()->ptr());
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
    return (uchar *)(thd->variables.time_zone->get_name()->ptr());
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


uchar *sys_var_max_user_conn::value_ptr(THD *thd, enum_var_type type,
                                       LEX_STRING *base)
{
  if (type != OPT_GLOBAL &&
      thd->user_connect && thd->user_connect->user_resources.user_conn)
    return (uchar*) &(thd->user_connect->user_resources.user_conn);
  return (uchar*) &(max_user_connections);
}


bool sys_var_thd_ulong_session_readonly::check(THD *thd, set_var *var)
{
  if (var->type != OPT_GLOBAL)
  {
    my_error(ER_VARIABLE_IS_READONLY, MYF(0), "SESSION", name, "GLOBAL");
    return TRUE;
  }

  return sys_var_thd_ulong::check(thd, var);
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
    const char *locale_str= res->c_ptr_safe();
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


uchar *sys_var_thd_lc_time_names::value_ptr(THD *thd, enum_var_type type,
					  LEX_STRING *base)
{
  return type == OPT_GLOBAL ?
                 (uchar *) global_system_variables.lc_time_names->name :
                 (uchar *) thd->variables.lc_time_names->name;
}


void sys_var_thd_lc_time_names::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.lc_time_names= my_default_lc_time_names;
  else
    thd->variables.lc_time_names= global_system_variables.lc_time_names;
}

/*
  Handling of microseoncds given as seconds.part_seconds

  NOTES
    The argument to long query time is in seconds in decimal
    which is converted to ulonglong integer holding microseconds for storage.
    This is used for handling long_query_time
*/

bool sys_var_microseconds::update(THD *thd, set_var *var)
{
  double num= var->value->val_real();
  longlong microseconds;
  if (num > (double) option_limits->max_value)
    num= (double) option_limits->max_value;
  if (num < (double) option_limits->min_value)
    num= (double) option_limits->min_value;
  microseconds= (longlong) (num * 1000000.0 + 0.5);
  if (var->type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    (global_system_variables.*offset)= microseconds;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= microseconds;
  return 0;
}


void sys_var_microseconds::set_default(THD *thd, enum_var_type type)
{
  longlong microseconds= (longlong) (option_limits->def_value * 1000000.0);
  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= microseconds;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= microseconds;
}


uchar *sys_var_microseconds::value_ptr(THD *thd, enum_var_type type,
                                          LEX_STRING *base)
{
  thd->tmp_double_value= (double) ((type == OPT_GLOBAL) ?
                                   global_system_variables.*offset :
                                   thd->variables.*offset) / 1000000.0;
  return (uchar*) &thd->tmp_double_value;
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

/*
  Functions to be only used to update thd->options OPTION_BIN_LOG bit
*/
static bool set_option_log_bin_bit(THD *thd, set_var *var)
{
  set_option_bit(thd, var);
  if (!thd->in_sub_stmt)
    thd->sql_log_bin_toplevel= thd->options & OPTION_BIN_LOG;
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
      if (thd->transaction.xid_state.xa_state != XA_NOTR)
      {
        thd->options= org_options;
        my_error(ER_XAER_RMFAIL, MYF(0),
                 xa_state_names[thd->transaction.xid_state.xa_state]);
        return 1;
      }
      thd->options&= ~(ulonglong) (OPTION_BEGIN | OPTION_KEEP_LOG);
      thd->transaction.all.modified_non_trans_table= FALSE;
      thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
      if (ha_commit(thd))
	return 1;
    }
    else
    {
      thd->transaction.all.modified_non_trans_table= FALSE;
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

static uchar *get_warning_count(THD *thd)
{
  thd->sys_var_tmp.long_value=
    (thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_NOTE] +
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR] +
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_WARN]);
  return (uchar*) &thd->sys_var_tmp.long_value;
}

static uchar *get_error_count(THD *thd)
{
  thd->sys_var_tmp.long_value= 
    thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR];
  return (uchar*) &thd->sys_var_tmp.long_value;
}


/**
  Get the tmpdir that was specified or chosen by default.

  This is necessary because if the user does not specify a temporary
  directory via the command line, one is chosen based on the environment
  or system defaults.  But we can't just always use mysql_tmpdir, because
  that is actually a call to my_tmpdir() which cycles among possible
  temporary directories.

  @param thd		thread handle

  @retval
    ptr		pointer to NUL-terminated string
*/
static uchar *get_tmpdir(THD *thd)
{
  if (opt_mysql_tmpdir)
    return (uchar *)opt_mysql_tmpdir;
  return (uchar*)mysql_tmpdir;
}

static uchar *get_myisam_mmap_size(THD *thd)
{
  return (uchar *)&myisam_mmap_size;
}


/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/**
  Find variable name in option my_getopt structure used for
  command line args.

  @param opt	option structure array to search in
  @param name	variable name

  @retval
    0		Error
  @retval
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


/**
  Return variable name and length for hashing of variables.
*/

static uchar *get_sys_var_length(const sys_var *var, size_t *length,
                                 my_bool first)
{
  *length= var->name_length;
  return (uchar*) var->name;
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
    if (my_hash_insert(&system_variable_hash, (uchar*) var))
      goto error;
    if (long_options)
      var->option_limits= find_option(long_options, var->name);
  }
  return 0;

error:
  for (; first != var; first= first->next)
    hash_delete(&system_variable_hash, (uchar*) first);
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
    result|= hash_delete(&system_variable_hash, (uchar*) var);

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
  int size= sizeof(SHOW_VAR) * (count + 1);
  SHOW_VAR *result= (SHOW_VAR*) thd->alloc(size);

  if (result)
  {
    SHOW_VAR *show= result;

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
      my_qsort(result, count, sizeof(SHOW_VAR),
               (qsort_cmp) show_cmp);
    
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
  
  for (sys_var *var=vars.first; var; var= var->next, count++) ;

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
}


/**
  Find a user set-table variable.

  @param str	   Name of system variable to find
  @param length    Length of variable.  zero means that we should use strlen()
                   on the variable
  @param no_error  Refuse to emit an error, even if one occurred.

  @retval
    pointer	pointer to variable definitions
  @retval
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
			      (uchar*) str, length ? length : strlen(str));
  if (!(var || no_error))
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);

  return var;
}


/**
  Execute update of all variables.

  First run a check of all variables that all updates will go ok.
  If yes, then execute all updates, returning an error if any one failed.

  This should ensure that in all normal cases none all or variables are
  updated.

  @param THD		Thread id
  @param var_list       List of variables to update

  @retval
    0	ok
  @retval
    1	ERROR, message sent (normally no variables was updated)
  @retval
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
  if (!(error= test(thd->is_error())))
  {
    it.rewind();
    while ((var= it++))
      error|= var->update(thd);         // Returns 0, -1 or 1
  }

err:
  free_underlaid_joins(thd, &thd->lex->select_lex);
  DBUG_RETURN(error);
}


/**
  Say if all variables set by a SET support the ONE_SHOT keyword
  (currently, only character set and collation do; later timezones
  will).

  @param var_list	List of variables to update

  @note
    It has a "not_" because it makes faster tests (no need to "!")

  @retval
    0	all variables of the list support ONE_SHOT
  @retval
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


/**
  Check variable, but without assigning value (used by PS).

  @param thd		thread handler

  @retval
    0	ok
  @retval
    1	ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
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

/**
  Update variable

  @param   thd    thread handler
  @returns 0|1    ok or	ERROR

  @note ERROR can be only due to abnormal operations involving
  the server's execution evironment such as
  out of memory, hard disk failure or the computer blows up.
  Consider set_var::check() method if there is a need to return
  an error due to logics.
*/
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


/**
  Check variable, but without assigning value (used by PS).

  @param thd		thread handler

  @retval
    0	ok
  @retval
    1	ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
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
    DBUG_ASSERT(thd->security_ctx->priv_host);
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
  if (!user->user.str)
  {
    DBUG_ASSERT(thd->security_ctx->priv_user);
    user->user.str= (char *) thd->security_ctx->priv_user;
    user->user.length= strlen(thd->security_ctx->priv_user);
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
    LEX_STRING engine_name;
    handlerton *hton;
    if (!(res=var->value->val_str(&str)) ||
        !(engine_name.str= (char *)res->ptr()) ||
        !(engine_name.length= res->length()) ||
	!(var->save_result.plugin= ha_resolve_by_name(thd, &engine_name)) ||
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


uchar *sys_var_thd_storage_engine::value_ptr(THD *thd, enum_var_type type,
					    LEX_STRING *base)
{
  uchar* result;
  handlerton *hton;
  LEX_STRING *engine_name;
  plugin_ref plugin= thd->variables.*offset;
  if (type == OPT_GLOBAL)
    plugin= my_plugin_lock(thd, &(global_system_variables.*offset));
  hton= plugin_data(plugin, handlerton*);
  engine_name= &hton2plugin[hton->slot]->name;
  result= (uchar *) thd->strmake(engine_name->str, engine_name->length);
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
  WARN_DEPRECATED(thd, "6.0", "@@table_type", "'@@storage_engine'");
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

/**
  Make string representation of mode.

  @param[in]  thd    thread handler
  @param[in]  val    sql_mode value
  @param[out] len    pointer on length of string

  @return
    pointer to string with sql_mode representation
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


uchar *sys_var_thd_sql_mode::value_ptr(THD *thd, enum_var_type type,
				      LEX_STRING *base)
{
  LEX_STRING sql_mode;
  ulonglong val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
                  thd->variables.*offset);
  (void) symbolic_mode_representation(thd, val, &sql_mode);
  return (uchar *) sql_mode.str;
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

/** Map database specific bits to function bits. */

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


bool
sys_var_thd_optimizer_switch::
symbolic_mode_representation(THD *thd, ulonglong val, LEX_STRING *rep)
{
  char buff[STRING_BUFFER_USUAL_SIZE*8];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  int i;
  ulonglong bit;
  tmp.length(0);
 
  for (i= 0, bit=1; bit != OPTIMIZER_SWITCH_LAST; i++, bit= bit << 1)
  {
    tmp.append(optimizer_switch_typelib.type_names[i],
               optimizer_switch_typelib.type_lengths[i]);
    tmp.append('=');
    tmp.append((val & bit)? "on":"off");
    tmp.append(',');
  }

  if (tmp.length())
    tmp.length(tmp.length() - 1); /* trim the trailing comma */

  rep->str= thd->strmake(tmp.ptr(), tmp.length());

  rep->length= rep->str ? tmp.length() : 0;

  return rep->length != tmp.length();
}


uchar *sys_var_thd_optimizer_switch::value_ptr(THD *thd, enum_var_type type,
				               LEX_STRING *base)
{
  LEX_STRING opts;
  ulonglong val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
                  thd->variables.*offset);
  (void) symbolic_mode_representation(thd, val, &opts);
  return (uchar *) opts.str;
}


/*
  Check (and actually parse) string representation of @@optimizer_switch.
*/

bool sys_var_thd_optimizer_switch::check(THD *thd, set_var *var)
{
  bool not_used;
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  uint error_len= 0;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (!(res= var->value->val_str(&str)))
  {
    strmov(buff, "NULL");
    goto err;
  }
  
  if (res->length() == 0)
  {
    buff[0]= 0;
    goto err;
  }

  var->save_result.ulong_value= 
    (ulong)find_set_from_flags(&optimizer_switch_typelib, 
                               optimizer_switch_typelib.count, 
                               thd->variables.optimizer_switch,
                               global_system_variables.optimizer_switch,
                               res->c_ptr_safe(), res->length(), NULL,
                               &error, &error_len, &not_used);
  if (error_len)
  {
    strmake(buff, error, min(sizeof(buff) - 1, error_len));
    goto err;
  }
  return FALSE;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buff);
  return TRUE;
}


void sys_var_thd_optimizer_switch::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= OPTIMIZER_SWITCH_DEFAULT;
  else
    thd->variables.*offset= global_system_variables.*offset;
}

/****************************************************************************
  Named list handling
****************************************************************************/

uchar* find_named(I_List<NAMED_LIST> *list, const char *name, uint length,
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
		     void (*free_element)(const char *name, uchar*))
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
    if (!new NAMED_LIST(&key_caches, name, length, (uchar*) key_cache))
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


bool process_key_caches(process_key_cache_t func)
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
  WARN_DEPRECATED(thd, VER_CELOSIA, "@@log_bin_trust_routine_creators",
                      "'@@log_bin_trust_function_creators'");
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
  if ((result= close_cached_tables(thd, NULL, FALSE, TRUE, TRUE)))
    goto end_with_read_lock;

  if ((result= make_global_read_lock_block_commit(thd)))
    goto end_with_read_lock;

  /* Change the opt_readonly system variable, safe because the lock is held */
  result= sys_var_bool_ptr::update(thd, var);

end_with_read_lock:
  /* Release the lock */
  unlock_global_read_lock(thd);
  DBUG_RETURN(result);
}


#ifndef DBUG_OFF
/* even session variable here requires SUPER, because of -#o,file */
bool sys_var_thd_dbug::check(THD *thd, set_var *var)
{
  return check_global_access(thd, SUPER_ACL);
}

bool sys_var_thd_dbug::update(THD *thd, set_var *var)
{
  char buf[256];
  String str(buf, sizeof(buf), system_charset_info), *res;

  res= var->value->val_str(&str);

  if (var->type == OPT_GLOBAL)
    DBUG_SET_INITIAL(res ? res->c_ptr() : "");
  else
    DBUG_SET(res ? res->c_ptr() : "");

  return 0;
}


uchar *sys_var_thd_dbug::value_ptr(THD *thd, enum_var_type type, LEX_STRING *b)
{
  char buf[256];
  if (type == OPT_GLOBAL)
    DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
  else
    DBUG_EXPLAIN(buf, sizeof(buf));
  return (uchar*) thd->strdup(buf);
}
#endif /* DBUG_OFF */


#ifdef HAVE_EVENT_SCHEDULER
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


uchar *sys_var_event_scheduler::value_ptr(THD *thd, enum_var_type type,
                                         LEX_STRING *base)
{
  return (uchar *) Events::get_opt_event_scheduler_str();
}
#endif


int 
check_max_allowed_packet(THD *thd,  set_var *var)
{
  longlong val= var->value->val_int();
  if (val < (longlong) global_system_variables.net_buffer_length)
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                        ER_UNKNOWN_ERROR, 
                        "The value of 'max_allowed_packet' should be no less than "
                        "the value of 'net_buffer_length'");
  }
  return 0;
}


int 
check_net_buffer_length(THD *thd,  set_var *var)
{
  longlong val= var->value->val_int();
  if (val > (longlong) global_system_variables.max_allowed_packet)
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                        ER_UNKNOWN_ERROR, 
                        "The value of 'max_allowed_packet' should be no less than "
                        "the value of 'net_buffer_length'");
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

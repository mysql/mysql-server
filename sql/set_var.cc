/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - Use one of the 'sys_var... classes from set_var.h or write a specific
    one for the variable type.
  - Define it in the 'variable definition list' in this file.
  - If the variable should be changeable, it should be added to the
    'list of all variables' list in this file.
  - If the variable should be changed from the command line, add a definition
    of it in the my_option structure list in mysqld.dcc
  - If the variable should show up in 'show variables' add it to the
    init_vars[] struct in this file

  TODO:
    - Add full support for the variable character_set (for 4.1)

    - When updating myisam_delay_key_write, we should do a 'flush tables'
      of all MyISAM tables to ensure that they are reopen with the
      new attribute.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h"
#include "sql_acl.h"
#include <my_getopt.h>
#include <thr_alarm.h>
#include <myisam.h>
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif

static HASH system_variable_hash;
const char *bool_type_names[]= { "OFF", "ON", NullS };
TYPELIB bool_typelib=
{
  array_elements(bool_type_names)-1, "", bool_type_names
};

const char *delay_key_write_type_names[]= { "OFF", "ON", "ALL", NullS };
TYPELIB delay_key_write_typelib=
{
  array_elements(delay_key_write_type_names)-1, "", delay_key_write_type_names
};

static bool sys_check_charset(THD *thd, set_var *var);
static bool sys_update_charset(THD *thd, set_var *var);
static void sys_set_default_charset(THD *thd, enum_var_type type);
static bool set_option_bit(THD *thd, set_var *var);
static bool set_option_autocommit(THD *thd, set_var *var);
static bool set_log_update(THD *thd, set_var *var);
static void fix_low_priority_updates(THD *thd, enum_var_type type);
static void fix_tx_isolation(THD *thd, enum_var_type type);
static void fix_net_read_timeout(THD *thd, enum_var_type type);
static void fix_net_write_timeout(THD *thd, enum_var_type type);
static void fix_net_retry_count(THD *thd, enum_var_type type);
static void fix_max_join_size(THD *thd, enum_var_type type);
static void fix_query_cache_size(THD *thd, enum_var_type type);
static void fix_key_buffer_size(THD *thd, enum_var_type type);
static void fix_myisam_max_extra_sort_file_size(THD *thd, enum_var_type type);
static void fix_myisam_max_sort_file_size(THD *thd, enum_var_type type);
static void fix_max_binlog_size(THD *thd, enum_var_type type);
static void fix_max_relay_log_size(THD *thd, enum_var_type type);
static void fix_max_connections(THD *thd, enum_var_type type);
static void fix_thd_mem_root(THD *thd, enum_var_type type);
static void fix_trans_mem_root(THD *thd, enum_var_type type);
static void fix_server_id(THD *thd, enum_var_type type);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order
*/

sys_var_long_ptr	sys_binlog_cache_size("binlog_cache_size",
					      &binlog_cache_size);
sys_var_thd_ulong	sys_bulk_insert_buff_size("bulk_insert_buffer_size",
						  &SV::bulk_insert_buff_size);
sys_var_str		sys_charset("character_set",
				    sys_check_charset,
				    sys_update_charset,
				    sys_set_default_charset);
sys_var_thd_conv_charset sys_convert_charset("convert_character_set");
sys_var_bool_ptr	sys_concurrent_insert("concurrent_insert",
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
sys_var_bool_ptr	sys_flush("flush", &myisam_flush);
sys_var_long_ptr	sys_flush_time("flush_time", &flush_time);
sys_var_thd_ulong	sys_interactive_timeout("interactive_timeout",
						&SV::net_interactive_timeout);
sys_var_thd_ulong	sys_join_buffer_size("join_buffer_size",
					     &SV::join_buff_size);
sys_var_ulonglong_ptr	sys_key_buffer_size("key_buffer_size",
					    &keybuff_size,
					    fix_key_buffer_size);
sys_var_bool_ptr	sys_local_infile("local_infile",
					 &opt_local_infile);
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
						       &SV::max_insert_delayed_threads);
sys_var_thd_ulong	sys_max_delayed_threads("max_delayed_threads",
						&SV::max_insert_delayed_threads,
						fix_max_connections);
sys_var_thd_ulong	sys_max_heap_table_size("max_heap_table_size",
						&SV::max_heap_table_size);
sys_var_thd_ha_rows	sys_max_join_size("max_join_size",
					  &SV::max_join_size,
					  fix_max_join_size);
sys_var_thd_ulong	sys_max_seeks_for_key("max_seeks_for_key",
					      &SV::max_seeks_for_key);
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
sys_var_long_ptr	sys_max_user_connections("max_user_connections",
						 &max_user_connections);
sys_var_thd_ulong	sys_max_tmp_tables("max_tmp_tables",
					   &SV::max_tmp_tables);
sys_var_long_ptr	sys_max_write_lock_count("max_write_lock_count",
						 &max_write_lock_count);
sys_var_thd_ulonglong	sys_myisam_max_extra_sort_file_size("myisam_max_extra_sort_file_size", &SV::myisam_max_extra_sort_file_size, fix_myisam_max_extra_sort_file_size, 1);
sys_var_thd_ulonglong	sys_myisam_max_sort_file_size("myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
sys_var_thd_ulong       sys_myisam_repair_threads("myisam_repair_threads", &SV::myisam_repair_threads);
sys_var_thd_ulong	sys_myisam_sort_buffer_size("myisam_sort_buffer_size", &SV::myisam_sort_buff_size);
sys_var_thd_ulong	sys_net_buffer_length("net_buffer_length",
					      &SV::net_buffer_length);
sys_var_thd_ulong	sys_net_read_timeout("net_read_timeout",
					     &SV::net_read_timeout,
					     fix_net_read_timeout);
sys_var_thd_ulong	sys_net_write_timeout("net_write_timeout",
					      &SV::net_write_timeout,
					      fix_net_write_timeout);
sys_var_thd_ulong	sys_net_retry_count("net_retry_count",
					    &SV::net_retry_count,
					    fix_net_retry_count);
sys_var_thd_bool	sys_new_mode("new", &SV::new_mode);
sys_var_thd_ulong	sys_read_buff_size("read_buffer_size",
					   &SV::read_buff_size);
sys_var_bool_ptr	sys_readonly("read_only", &opt_readonly);
sys_var_thd_ulong	sys_read_rnd_buff_size("read_rnd_buffer_size",
					       &SV::read_rnd_buff_size);
sys_var_long_ptr	sys_rpl_recovery_rank("rpl_recovery_rank",
					      &rpl_recovery_rank);
sys_var_long_ptr	sys_query_cache_size("query_cache_size",
					     &query_cache_size,
					     fix_query_cache_size);

sys_var_thd_ulong	sys_range_alloc_block_size("range_alloc_block_size",
						   &SV::range_alloc_block_size);
sys_var_thd_ulong	sys_query_alloc_block_size("query_alloc_block_size",
						   &SV::query_alloc_block_size,
						   fix_thd_mem_root);
sys_var_thd_ulong	sys_query_prealloc_size("query_prealloc_size",
						&SV::query_prealloc_size,
						fix_thd_mem_root);
sys_var_thd_ulong	sys_trans_alloc_block_size("transaction_alloc_block_size",
						   &SV::trans_alloc_block_size,
						   fix_trans_mem_root);
sys_var_thd_ulong	sys_trans_prealloc_size("transaction_prealloc_size",
						&SV::trans_prealloc_size,
						fix_trans_mem_root);

#ifdef HAVE_QUERY_CACHE
sys_var_long_ptr	sys_query_cache_limit("query_cache_limit",
					      &query_cache.query_cache_limit);
sys_var_thd_enum	sys_query_cache_type("query_cache_type",
					     &SV::query_cache_type,
					     &query_cache_type_typelib);
sys_var_thd_bool
sys_query_cache_wlock_invalidate("query_cache_wlock_invalidate",
				 &SV::query_cache_wlock_invalidate);
#endif /* HAVE_QUERY_CACHE */
sys_var_long_ptr	sys_server_id("server_id", &server_id, fix_server_id);
sys_var_bool_ptr	sys_slave_compressed_protocol("slave_compressed_protocol",
						      &opt_slave_compressed_protocol);
sys_var_long_ptr	sys_slave_net_timeout("slave_net_timeout",
					      &slave_net_timeout);
sys_var_long_ptr	sys_slow_launch_time("slow_launch_time",
					     &slow_launch_time);
sys_var_thd_ulong	sys_sort_buffer("sort_buffer_size",
					&SV::sortbuff_size);
sys_var_thd_enum	sys_table_type("table_type", &SV::table_type,
				       &ha_table_typelib);
sys_var_long_ptr	sys_table_cache_size("table_cache",
					     &table_cache_size);
sys_var_long_ptr	sys_thread_cache_size("thread_cache_size",
					      &thread_cache_size);
sys_var_thd_enum	sys_tx_isolation("tx_isolation",
					 &SV::tx_isolation,
					 &tx_isolation_typelib,
					 fix_tx_isolation);
sys_var_thd_ulong	sys_tmp_table_size("tmp_table_size",
					   &SV::tmp_table_size);
sys_var_thd_ulong	sys_net_wait_timeout("wait_timeout",
					     &SV::net_wait_timeout);
					     
#ifdef HAVE_INNOBASE_DB
sys_var_long_ptr        sys_innodb_max_dirty_pages_pct("innodb_max_dirty_pages_pct",
                                                        &srv_max_buf_pool_modified_pct);
sys_var_long_ptr	sys_innodb_max_purge_lag("innodb_max_purge_lag",
							&srv_max_purge_lag);
sys_var_thd_bool	sys_innodb_table_locks("innodb_table_locks",
                                               &SV::innodb_table_locks);
#endif 					     


/*
  Variables that are bits in THD
*/

static sys_var_thd_bit	sys_autocommit("autocommit",
				       set_option_autocommit,
				       OPTION_NOT_AUTOCOMMIT,
				       1);
static sys_var_thd_bit	sys_big_tables("big_tables",
				       set_option_bit,
				       OPTION_BIG_TABLES);
#ifndef TO_BE_DELETED	/* Alias for big_tables */
static sys_var_thd_bit	sys_sql_big_tables("sql_big_tables",
					   set_option_bit,
					   OPTION_BIG_TABLES);
#endif
static sys_var_thd_bit	sys_big_selects("sql_big_selects",
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_thd_bit	sys_log_off("sql_log_off",
				    set_option_bit,
				    OPTION_LOG_OFF);
static sys_var_thd_bit	sys_log_update("sql_log_update",
				       set_log_update,
				       OPTION_UPDATE_LOG);
static sys_var_thd_bit	sys_log_binlog("sql_log_bin",
					set_log_update,
					OPTION_BIN_LOG);
static sys_var_thd_bit	sys_sql_warnings("sql_warnings",
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_thd_bit	sys_auto_is_null("sql_auto_is_null",
					 set_option_bit,
					 OPTION_AUTO_IS_NULL);
static sys_var_thd_bit	sys_safe_updates("sql_safe_updates",
					 set_option_bit,
					 OPTION_SAFE_UPDATES);
static sys_var_thd_bit	sys_buffer_results("sql_buffer_result",
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_thd_bit	sys_quote_show_create("sql_quote_show_create",
					      set_option_bit,
					      OPTION_QUOTE_SHOW_CREATE);
static sys_var_thd_bit	sys_foreign_key_checks("foreign_key_checks",
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS,
					       1);
static sys_var_thd_bit	sys_unique_checks("unique_checks",
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
/* alias for last_insert_id() to be compatible with Sybase */
static sys_var_slave_skip_counter sys_slave_skip_counter("sql_slave_skip_counter");
static sys_var_rand_seed1	sys_rand_seed1("rand_seed1");
static sys_var_rand_seed2	sys_rand_seed2("rand_seed2");

static sys_var_thd_ulong        sys_default_week_format("default_week_format",
							&SV::default_week_format);


/* Read only variables */

sys_var_const_str		sys_os("version_compile_os", SYSTEM_TYPE);
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
  &sys_autocommit,
  &sys_big_tables,
  &sys_big_selects,
  &sys_binlog_cache_size,
  &sys_buffer_results,
  &sys_bulk_insert_buff_size,
  &sys_concurrent_insert,
  &sys_connect_timeout,
  &sys_default_week_format,
  &sys_convert_charset,
  &sys_delay_key_write,
  &sys_delayed_insert_limit,
  &sys_delayed_insert_timeout,
  &sys_delayed_queue_size,
  &sys_flush,
  &sys_flush_time,
  &sys_foreign_key_checks,
  &sys_identity,
  &sys_insert_id,
  &sys_interactive_timeout,
  &sys_join_buffer_size,
  &sys_key_buffer_size,
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
  &sys_max_insert_delayed_threads,
  &sys_max_heap_table_size,
  &sys_max_join_size,
  &sys_max_relay_log_size,
  &sys_max_seeks_for_key,
  &sys_max_sort_length,
  &sys_max_tmp_tables,
  &sys_max_user_connections,
  &sys_max_write_lock_count,
  &sys_myisam_max_extra_sort_file_size,
  &sys_myisam_max_sort_file_size,
  &sys_myisam_repair_threads,
  &sys_myisam_sort_buffer_size,
  &sys_net_buffer_length,
  &sys_net_read_timeout,
  &sys_net_retry_count,
  &sys_net_wait_timeout,
  &sys_net_write_timeout,
  &sys_new_mode,
  &sys_query_alloc_block_size,
  &sys_query_cache_size,
  &sys_query_prealloc_size,
#ifdef HAVE_QUERY_CACHE
  &sys_query_cache_limit,
  &sys_query_cache_type,
  &sys_query_cache_wlock_invalidate,
#endif /* HAVE_QUERY_CACHE */
  &sys_quote_show_create,
  &sys_rand_seed1,
  &sys_rand_seed2,
  &sys_range_alloc_block_size,
  &sys_read_buff_size,
  &sys_read_rnd_buff_size,
  &sys_rpl_recovery_rank,
  &sys_safe_updates,
  &sys_select_limit,
  &sys_server_id,
  &sys_slave_compressed_protocol,
  &sys_slave_net_timeout,
  &sys_slave_skip_counter,
  &sys_readonly,
  &sys_slow_launch_time,
  &sys_sort_buffer,
  &sys_sql_big_tables,
  &sys_sql_low_priority_updates,
  &sys_sql_max_join_size,
  &sys_sql_warnings,
  &sys_table_cache_size,
  &sys_table_type,
  &sys_thread_cache_size,
  &sys_timestamp,
  &sys_tmp_table_size,
  &sys_trans_alloc_block_size,
  &sys_trans_prealloc_size,
  &sys_tx_isolation,
  &sys_os,
#ifdef HAVE_INNOBASE_DB
  &sys_innodb_max_dirty_pages_pct,
  &sys_innodb_max_purge_lag,
  &sys_innodb_table_locks,
#endif    
  &sys_unique_checks
};


/*
  Variables shown by SHOW variables in alphabetical order
*/

struct show_var_st init_vars[]= {
  {"back_log",                (char*) &back_log,                    SHOW_LONG},
  {"basedir",                 mysql_home,                           SHOW_CHAR},
#ifdef HAVE_BERKELEY_DB
  {"bdb_cache_size",          (char*) &berkeley_cache_size,         SHOW_LONG},
  {"bdb_log_buffer_size",     (char*) &berkeley_log_buffer_size,    SHOW_LONG},
  {"bdb_home",                (char*) &berkeley_home,               SHOW_CHAR_PTR},
  {"bdb_max_lock",            (char*) &berkeley_max_lock,	    SHOW_LONG},
  {"bdb_logdir",              (char*) &berkeley_logdir,             SHOW_CHAR_PTR},
  {"bdb_shared_data",	      (char*) &berkeley_shared_data,	    SHOW_BOOL},
  {"bdb_tmpdir",              (char*) &berkeley_tmpdir,             SHOW_CHAR_PTR},
  {"bdb_version",             (char*) DB_VERSION_STRING,            SHOW_CHAR},
#endif
  {sys_binlog_cache_size.name,(char*) &sys_binlog_cache_size,	    SHOW_SYS},
  {sys_bulk_insert_buff_size.name,(char*) &sys_bulk_insert_buff_size,SHOW_SYS},
  {sys_charset.name, 	      (char*) &sys_charset,		     SHOW_SYS},
  {"character_sets",          (char*) &charsets_list,               SHOW_CHAR_PTR},
  {sys_concurrent_insert.name,(char*) &sys_concurrent_insert,       SHOW_SYS},
  {sys_connect_timeout.name,  (char*) &sys_connect_timeout,         SHOW_SYS},
  {sys_convert_charset.name,  (char*) &sys_convert_charset,	    SHOW_SYS},
  {"datadir",                 mysql_real_data_home,                 SHOW_CHAR},
  {"default_week_format",     (char*) &sys_default_week_format,     SHOW_SYS},
  {sys_delay_key_write.name,  (char*) &sys_delay_key_write,         SHOW_SYS},
  {sys_delayed_insert_limit.name, (char*) &sys_delayed_insert_limit,SHOW_SYS},
  {sys_delayed_insert_timeout.name, (char*) &sys_delayed_insert_timeout, SHOW_SYS},
  {sys_delayed_queue_size.name,(char*) &sys_delayed_queue_size,     SHOW_SYS},
  {sys_flush.name,             (char*) &sys_flush,                  SHOW_SYS},
  {sys_flush_time.name,        (char*) &sys_flush_time,             SHOW_SYS},
  {"ft_boolean_syntax",       (char*) ft_boolean_syntax,	    SHOW_CHAR},
  {"ft_min_word_len",         (char*) &ft_min_word_len,             SHOW_LONG},
  {"ft_max_word_len",         (char*) &ft_max_word_len,             SHOW_LONG},
  {"ft_max_word_len_for_sort",(char*) &ft_max_word_len_for_sort,    SHOW_LONG},
  {"ft_stopword_file",        (char*) &ft_stopword_file,            SHOW_CHAR_PTR},
  {"have_bdb",		      (char*) &have_berkeley_db,	    SHOW_HAVE},
  {"have_crypt",	      (char*) &have_crypt,		    SHOW_HAVE},
  {"have_innodb",	      (char*) &have_innodb,		    SHOW_HAVE},
  {"have_isam",	      	      (char*) &have_isam,		    SHOW_HAVE},
  {"have_raid",		      (char*) &have_raid,		    SHOW_HAVE},
  {"have_symlink",            (char*) &have_symlink,         	    SHOW_HAVE},
  {"have_openssl",	      (char*) &have_openssl,		    SHOW_HAVE},
  {"have_query_cache",        (char*) &have_query_cache,            SHOW_HAVE},
  {"init_file",               (char*) &opt_init_file,               SHOW_CHAR_PTR},
#ifdef HAVE_INNOBASE_DB
  {"innodb_additional_mem_pool_size", (char*) &innobase_additional_mem_pool_size, SHOW_LONG },
  {"innodb_buffer_pool_size", (char*) &innobase_buffer_pool_size, SHOW_LONG },
  {"innodb_data_file_path", (char*) &innobase_data_file_path,	    SHOW_CHAR_PTR},
  {"innodb_data_home_dir",  (char*) &innobase_data_home_dir,	    SHOW_CHAR_PTR},
  {"innodb_file_io_threads", (char*) &innobase_file_io_threads, SHOW_LONG },
  {"innodb_force_recovery", (char*) &innobase_force_recovery, SHOW_LONG },
  {"innodb_thread_concurrency", (char*) &innobase_thread_concurrency, SHOW_LONG },
  {"innodb_flush_log_at_trx_commit", (char*) &innobase_flush_log_at_trx_commit, SHOW_INT},
  {"innodb_fast_shutdown", (char*) &innobase_fast_shutdown, SHOW_MY_BOOL},
  {"innodb_flush_method",    (char*) &innobase_unix_file_flush_method, SHOW_CHAR_PTR},
  {"innodb_lock_wait_timeout", (char*) &innobase_lock_wait_timeout, SHOW_LONG },
  {"innodb_log_arch_dir",   (char*) &innobase_log_arch_dir, 	    SHOW_CHAR_PTR},
  {"innodb_log_archive",    (char*) &innobase_log_archive, 	    SHOW_MY_BOOL},
  {"innodb_log_buffer_size", (char*) &innobase_log_buffer_size, SHOW_LONG },
  {"innodb_log_file_size", (char*) &innobase_log_file_size, SHOW_LONG},
  {"innodb_log_files_in_group", (char*) &innobase_log_files_in_group,	SHOW_LONG},
  {"innodb_log_group_home_dir", (char*) &innobase_log_group_home_dir, SHOW_CHAR_PTR},
  {"innodb_mirrored_log_groups", (char*) &innobase_mirrored_log_groups, SHOW_LONG},
  {sys_innodb_max_dirty_pages_pct.name, (char*) &sys_innodb_max_dirty_pages_pct, SHOW_SYS},
  {sys_innodb_max_purge_lag.name, (char*) &sys_innodb_max_purge_lag, SHOW_SYS},
  {sys_innodb_table_locks.name, (char*) &sys_innodb_table_locks, SHOW_SYS},
#endif
  {sys_interactive_timeout.name,(char*) &sys_interactive_timeout,   SHOW_SYS},
  {sys_join_buffer_size.name,   (char*) &sys_join_buffer_size,	    SHOW_SYS},
  {sys_key_buffer_size.name,	(char*) &sys_key_buffer_size,	    SHOW_SYS},
  {"language",                language,                             SHOW_CHAR},
  {"large_files_support",     (char*) &opt_large_files,             SHOW_BOOL},
  {sys_license.name,	      (char*) &sys_license,                 SHOW_SYS},
  {sys_local_infile.name,     (char*) &sys_local_infile,	    SHOW_SYS},
#ifdef HAVE_MLOCKALL
  {"locked_in_memory",	      (char*) &locked_in_memory,	    SHOW_BOOL},
#endif
  {"log",                     (char*) &opt_log,                     SHOW_BOOL},
  {"log_update",              (char*) &opt_update_log,              SHOW_BOOL},
  {"log_bin",                 (char*) &opt_bin_log,                 SHOW_BOOL},
  {"log_slave_updates",       (char*) &opt_log_slave_updates,       SHOW_MY_BOOL},
  {"log_slow_queries",        (char*) &opt_slow_log,                SHOW_BOOL},
  {sys_log_warnings.name,     (char*) &sys_log_warnings,	    SHOW_SYS},
  {sys_long_query_time.name,  (char*) &sys_long_query_time, 	    SHOW_SYS},
  {sys_low_priority_updates.name, (char*) &sys_low_priority_updates, SHOW_SYS},
  {"lower_case_file_system",  (char*) &lower_case_file_system,      SHOW_MY_BOOL},
  {"lower_case_table_names",  (char*) &lower_case_table_names,      SHOW_INT},
  {sys_max_allowed_packet.name,(char*) &sys_max_allowed_packet,	    SHOW_SYS},
  {sys_max_binlog_cache_size.name,(char*) &sys_max_binlog_cache_size, SHOW_SYS},
  {sys_max_binlog_size.name,    (char*) &sys_max_binlog_size,	    SHOW_SYS},
  {sys_max_connections.name,    (char*) &sys_max_connections,	    SHOW_SYS},
  {sys_max_connect_errors.name, (char*) &sys_max_connect_errors,    SHOW_SYS},
  {sys_max_delayed_threads.name,(char*) &sys_max_delayed_threads,   SHOW_SYS},
  {sys_max_insert_delayed_threads.name,
   (char*) &sys_max_insert_delayed_threads,   SHOW_SYS},
  {sys_max_heap_table_size.name,(char*) &sys_max_heap_table_size,   SHOW_SYS},
  {sys_max_join_size.name,	(char*) &sys_max_join_size,	    SHOW_SYS},
  {sys_max_relay_log_size.name, (char*) &sys_max_relay_log_size,    SHOW_SYS},
  {sys_max_seeks_for_key.name,  (char*) &sys_max_seeks_for_key,	    SHOW_SYS},
  {sys_max_sort_length.name,	(char*) &sys_max_sort_length,	    SHOW_SYS},
  {sys_max_user_connections.name,(char*) &sys_max_user_connections, SHOW_SYS},
  {sys_max_tmp_tables.name,	(char*) &sys_max_tmp_tables,	    SHOW_SYS},
  {sys_max_write_lock_count.name, (char*) &sys_max_write_lock_count,SHOW_SYS},
  {sys_myisam_max_extra_sort_file_size.name,
   (char*) &sys_myisam_max_extra_sort_file_size,
   SHOW_SYS},
  {sys_myisam_max_sort_file_size.name, (char*) &sys_myisam_max_sort_file_size,
   SHOW_SYS},
  {sys_myisam_repair_threads.name, (char*) &sys_myisam_repair_threads,
   SHOW_SYS},
  {"myisam_recover_options",  (char*) &myisam_recover_options_str,  SHOW_CHAR_PTR},
  {sys_myisam_sort_buffer_size.name, (char*) &sys_myisam_sort_buffer_size, SHOW_SYS},
#ifdef __NT__
  {"named_pipe",	      (char*) &opt_enable_named_pipe,       SHOW_MY_BOOL},
#endif
  {sys_net_buffer_length.name,(char*) &sys_net_buffer_length,       SHOW_SYS},
  {sys_net_read_timeout.name, (char*) &sys_net_read_timeout,        SHOW_SYS},
  {sys_net_retry_count.name,  (char*) &sys_net_retry_count,	    SHOW_SYS},
  {sys_net_write_timeout.name,(char*) &sys_net_write_timeout,       SHOW_SYS},
  {sys_new_mode.name,         (char*) &sys_new_mode,                SHOW_SYS},
  {"open_files_limit",	      (char*) &open_files_limit,	    SHOW_LONG},
  {"pid_file",                (char*) pidfile_name,                 SHOW_CHAR},
  {"log_error",               (char*) log_error_file,               SHOW_CHAR},
  {"port",                    (char*) &mysql_port,                  SHOW_INT},
  {"protocol_version",        (char*) &protocol_version,            SHOW_INT},
  {sys_query_alloc_block_size.name, (char*) &sys_query_alloc_block_size,
   SHOW_SYS},
#ifdef HAVE_QUERY_CACHE
  {sys_query_cache_limit.name,(char*) &sys_query_cache_limit,	    SHOW_SYS},
  {sys_query_cache_size.name, (char*) &sys_query_cache_size,	    SHOW_SYS},
  {sys_query_cache_type.name, (char*) &sys_query_cache_type,        SHOW_SYS},
#endif /* HAVE_QUERY_CACHE */
  {sys_query_prealloc_size.name, (char*) &sys_query_prealloc_size,  SHOW_SYS},
  {sys_range_alloc_block_size.name, (char*) &sys_range_alloc_block_size,
   SHOW_SYS},
  {sys_read_buff_size.name,   (char*) &sys_read_buff_size,	    SHOW_SYS},
  {sys_readonly.name,         (char*) &sys_readonly,                SHOW_SYS},
  {sys_read_rnd_buff_size.name,(char*) &sys_read_rnd_buff_size,	    SHOW_SYS},
  {sys_rpl_recovery_rank.name,(char*) &sys_rpl_recovery_rank,       SHOW_SYS},
  {sys_server_id.name,	      (char*) &sys_server_id,		    SHOW_SYS},
  {sys_slave_net_timeout.name,(char*) &sys_slave_net_timeout,	    SHOW_SYS},
  {"skip_external_locking",   (char*) &my_disable_locking,          SHOW_MY_BOOL},
  {"skip_networking",         (char*) &opt_disable_networking,      SHOW_BOOL},
  {"skip_show_database",      (char*) &opt_skip_show_db,            SHOW_BOOL},
  {sys_slow_launch_time.name, (char*) &sys_slow_launch_time,        SHOW_SYS},
#ifdef HAVE_SYS_UN_H
  {"socket",                  (char*) &mysql_unix_port,             SHOW_CHAR_PTR},
#endif
  {sys_sort_buffer.name,      (char*) &sys_sort_buffer, 	    SHOW_SYS},
  {"sql_mode",                (char*) &opt_sql_mode,                SHOW_LONG},
  {"table_cache",             (char*) &table_cache_size,            SHOW_LONG},
  {sys_table_type.name,	      (char*) &sys_table_type,	            SHOW_SYS},
  {sys_thread_cache_size.name,(char*) &sys_thread_cache_size,       SHOW_SYS},
#ifdef HAVE_THR_SETCONCURRENCY
  {"thread_concurrency",      (char*) &concurrency,                 SHOW_LONG},
#endif
  {"thread_stack",            (char*) &thread_stack,                SHOW_LONG},
  {sys_tx_isolation.name,     (char*) &sys_tx_isolation,	    SHOW_SYS},
#ifdef HAVE_TZNAME
  {"timezone",                time_zone,                            SHOW_CHAR},
#endif
  {sys_tmp_table_size.name,   (char*) &sys_tmp_table_size,	    SHOW_SYS},
  {"tmpdir",                  (char*) &mysql_tmpdir,                SHOW_CHAR_PTR},
  {sys_trans_alloc_block_size.name, (char*) &sys_trans_alloc_block_size,
   SHOW_SYS},
  {sys_trans_prealloc_size.name, (char*) &sys_trans_prealloc_size,  SHOW_SYS},
  {"version",                 server_version,                       SHOW_CHAR},
  {"version_comment",         (char*) MYSQL_COMPILATION_COMMENT,    SHOW_CHAR},
  {sys_os.name,		      (char*) &sys_os,			    SHOW_SYS},
  {sys_net_wait_timeout.name, (char*) &sys_net_wait_timeout,	    SHOW_SYS},
  {NullS, NullS, SHOW_LONG}
};

/*
  Functions to check and update variables
*/

/*
  The following 3 functions need to be changed in 4.1 when we allow
  one to change character sets
*/

static bool sys_check_charset(THD *thd, set_var *var)
{
  return 0;
}


static bool sys_update_charset(THD *thd, set_var *var)
{
  return 0;
}


static void sys_set_default_charset(THD *thd, enum_var_type type)
{
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
fix_myisam_max_extra_sort_file_size(THD *thd, enum_var_type type)
{
  myisam_max_extra_temp_length=
    (my_off_t) global_system_variables.myisam_max_extra_sort_file_size;
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


/*
  If we are changing the thread variable, we have to copy it to NET too
*/

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


static void fix_query_cache_size(THD *thd, enum_var_type type)
{
#ifdef HAVE_QUERY_CACHE
  query_cache.resize(query_cache_size);
#endif
}


static void fix_key_buffer_size(THD *thd, enum_var_type type)
{
  ha_resize_key_cache();
}


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
  if (!max_relay_log_size)
    active_mi->rli.relay_log.set_max_size(max_binlog_size);
  DBUG_VOID_RETURN;
}

static void fix_max_relay_log_size(THD *thd, enum_var_type type)
{
  DBUG_ENTER("fix_max_relay_log_size");
  DBUG_PRINT("info",("max_binlog_size=%lu max_relay_log_size=%lu",
                     max_binlog_size, max_relay_log_size));
  active_mi->rli.relay_log.set_max_size(max_relay_log_size ?
                                        max_relay_log_size: max_binlog_size);
  DBUG_VOID_RETURN;
}


static void fix_max_connections(THD *thd, enum_var_type type)
{
  resize_thr_alarm(max_connections + 
		   global_system_variables.max_insert_delayed_threads + 10);
}


static void fix_thd_mem_root(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(&thd->mem_root,
                        thd->variables.query_alloc_block_size,
                        thd->variables.query_prealloc_size);
}


static void fix_trans_mem_root(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(&thd->transaction.mem_root,
                        thd->variables.trans_alloc_block_size,
                        thd->variables.trans_prealloc_size);
}

static void fix_server_id(THD *thd, enum_var_type type)
{
  server_id_supplied = 1;
}

bool sys_var_long_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->value->val_int();
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
  ulonglong tmp= var->value->val_int();
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


byte *sys_var_enum::value_ptr(THD *thd, enum_var_type type)
{
  return (byte*) enum_names->type_names[*value];
}


bool sys_var_thd_ulong::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->value->val_int();

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ulong) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

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


byte *sys_var_thd_ulong::value_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_ha_rows::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->value->val_int();

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


byte *sys_var_thd_ha_rows::value_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_ulonglong::update(THD *thd,  set_var *var)
{
  ulonglong tmp= var->value->val_int();

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
    global_system_variables.*offset= (ulong) option_limits->def_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ulonglong::value_ptr(THD *thd, enum_var_type type)
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


byte *sys_var_thd_bool::value_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var::check_enum(THD *thd, set_var *var, TYPELIB *enum_names)
{
  char buff[80];
  const char *value;
  String str(buff,sizeof(buff)), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res=var->value->val_str(&str)) ||
	((long) (var->save_result.ulong_value=
		 (ulong) find_type(res->c_ptr(), enum_names, 3)-1))
	< 0)
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

/*
  Return an Item for a variable.  Used with @@[global.]variable_name

  If type is not given, return local value if exists, else global

  We have to use netprintf() instead of my_error() here as this is
  called on the parsing stage.
*/

Item *sys_var::item(THD *thd, enum_var_type var_type)
{
  if (check_type(var_type))
  {
    if (var_type != OPT_DEFAULT)
    {
      net_printf(&thd->net,ER_INCORRECT_GLOBAL_LOCAL_VAR,
		 name, var_type == OPT_GLOBAL ? "LOCAL" : "GLOBAL");
      return 0;
    }
    /* As there was no local variable, return the global value */
    var_type= OPT_GLOBAL;
  }
  switch (type()) {
  case SHOW_LONG:
    return new Item_uint((int32) *(ulong*) value_ptr(thd, var_type));
  case SHOW_LONGLONG:
  {
    longlong value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(longlong*) value_ptr(thd, var_type);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value);
  }
  case SHOW_HA_ROWS:
  {
    ha_rows value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(ha_rows*) value_ptr(thd, var_type);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int((longlong) value);
  }
  case SHOW_MY_BOOL:
    return new Item_int((int32) *(my_bool*) value_ptr(thd, var_type),1);
  case SHOW_CHAR:
  {
    char *str= (char*) value_ptr(thd, var_type);
    return new Item_string(str,strlen(str));
  }
  default:
    net_printf(&thd->net, ER_VAR_CANT_BE_READ, name);
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


byte *sys_var_thd_enum::value_ptr(THD *thd, enum_var_type type)
{
  ulong tmp= ((type == OPT_GLOBAL) ?
	      global_system_variables.*offset :
	      thd->variables.*offset);
  return (byte*) enum_names->type_names[tmp];
}


bool sys_var_thd_bit::update(THD *thd, set_var *var)
{
  int res= (*update_func)(thd, var);
  thd->lex.select_lex.options=thd->options;
  return res;
}


byte *sys_var_thd_bit::value_ptr(THD *thd, enum_var_type type)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  thd->sys_var_tmp.my_bool_value= ((thd->options & bit_flag) ?
				   !reverse : reverse);
  return (byte*) &thd->sys_var_tmp.my_bool_value;
}


bool sys_var_thd_conv_charset::check(THD *thd, set_var *var)
{
  CONVERT *tmp;
  char buff[80];
  String str(buff,sizeof(buff)), *res;

  if (!var->value)					// Default value
  {
    var->save_result.convert= (var->type != OPT_GLOBAL ?
			       global_system_variables.convert_set
			       : (CONVERT*) 0);
    return 0;
  }
  if (!(res=var->value->val_str(&str)))
    res= &empty_string;

  if (!(tmp=get_convert_set(res->c_ptr())))
  {
    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), res->c_ptr());
    return 1;
  }
  var->save_result.convert=tmp;			// Save for update
  return 0;
}


bool sys_var_thd_conv_charset::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.convert_set= var->save_result.convert;
  else
    thd->lex.convert_set= thd->variables.convert_set=
      var->save_result.convert;
  return 0;
}


byte *sys_var_thd_conv_charset::value_ptr(THD *thd, enum_var_type type)
{
  CONVERT *conv= ((type == OPT_GLOBAL) ?
		  global_system_variables.convert_set :
		  thd->variables.convert_set);
  return conv ? (byte*) conv->name : (byte*) "";
}


void sys_var_thd_conv_charset::set_default(THD *thd, enum_var_type type)
{
  thd->variables.convert_set= global_system_variables.convert_set; 
}


bool sys_var_timestamp::update(THD *thd,  set_var *var)
{
  thd->set_time((time_t) var->value->val_int());
  return 0;
}


void sys_var_timestamp::set_default(THD *thd, enum_var_type type)
{
  thd->user_time=0;
}


byte *sys_var_timestamp::value_ptr(THD *thd, enum_var_type type)
{
  thd->sys_var_tmp.long_value= (long) thd->start_time;
  return (byte*) &thd->sys_var_tmp.long_value;
}


bool sys_var_last_insert_id::update(THD *thd, set_var *var)
{
  thd->insert_id(var->value->val_int());
  return 0;
}


byte *sys_var_last_insert_id::value_ptr(THD *thd, enum_var_type type)
{
  thd->sys_var_tmp.long_value= (long) thd->insert_id();
  return (byte*) &thd->last_insert_id;
}


bool sys_var_insert_id::update(THD *thd, set_var *var)
{
  thd->next_insert_id=var->value->val_int();
  return 0;
}


byte *sys_var_insert_id::value_ptr(THD *thd, enum_var_type type)
{
  return (byte*) &thd->current_insert_id;
}


bool sys_var_slave_skip_counter::check(THD *thd, set_var *var)
{
  int result= 0;
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  if (active_mi->rli.slave_running)
  {
    my_error(ER_SLAVE_MUST_STOP, MYF(0));
    result=1;
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
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
    active_mi->rli.slave_skip_counter= (ulong) var->value->val_int();
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}


bool sys_var_rand_seed1::update(THD *thd, set_var *var)
{
  thd->rand.seed1= (ulong) var->value->val_int();
  return 0;
}

bool sys_var_rand_seed2::update(THD *thd, set_var *var)
{
  thd->rand.seed2= (ulong) var->value->val_int();
  return 0;
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


static bool set_log_update(THD *thd, set_var *var)
{
  if (opt_sql_bin_update)
    ((sys_var_thd_bit*) var->var)->bit_flag|= (OPTION_BIN_LOG |
					       OPTION_UPDATE_LOG);
  set_option_bit(thd, var);
  return 0;
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
  extern struct my_option my_long_options[];	// From mysqld

  hash_init(&system_variable_hash,array_elements(sys_variables),0,0,
	    (hash_get_key) get_sys_var_length,0, HASH_CASE_INSENSITIVE);
  sys_var **var, **end;
  for (var= sys_variables, end= sys_variables+array_elements(sys_variables) ;
       var < end;
       var++)
  {
    (*var)->name_length= strlen((*var)->name);
    (*var)->option_limits= find_option(my_long_options, (*var)->name);
    hash_insert(&system_variable_hash, (byte*) *var);
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

  NOTE
    We have to use net_printf() as this is called during the parsing stage

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
    net_printf(&current_thd->net, ER_UNKNOWN_SYSTEM_VARIABLE, (char*) str);
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
  int error= 0;
  List_iterator<set_var_base> it(*var_list);

  set_var_base *var;
  while ((var=it++))
  {
    if ((error=var->check(thd)))
      return error;
  }
  it.rewind();
  while ((var=it++))
    error|= var->update(thd);			// Returns 0, -1 or 1
  return error;
}


/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

int set_var::check(THD *thd)
{
  if (var->check_type(type))
  {
    my_error(type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE,
	     MYF(0),
	     var->name);
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

  if (value->fix_fields(thd,0))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name);
    return -1;
  }
  return var->check(thd, this) ? -1 : 0;
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
  return user_var_item->fix_fields(thd,0) ? -1 : 0;
}


int set_var_user::update(THD *thd)
{
  if (user_var_item->update())
  {
    /* Give an error if it's not given already */
    my_error(ER_SET_CONSTANTS_ONLY, MYF(0));
    return -1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

int set_var_password::check(THD *thd)
{
  if (!user->host.str)
    user->host.str= (char*) thd->host_or_ip;
  /* Returns 1 as the function sends error to client */
  return check_change_password(thd, user->host.str, user->user.str) ? 1 : 0;
}

int set_var_password::update(THD *thd)
{
  /* Returns 1 as the function sends error to client */
  return (change_password(thd, user->host.str, user->user.str, password) ?
	  1 : 0);
}

/****************************************************************************
  Used templates
****************************************************************************/

#ifdef __GNUC__
template class List<set_var_base>;
template class List_iterator<set_var_base>;
#endif

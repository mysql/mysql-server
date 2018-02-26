/* Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file
  Definitions of all server's session or global variables.

  How to add new variables:

  1. copy one of the existing variables, and edit the declaration.
  2. if you need special behavior on assignment or additional checks
     use ON_CHECK and ON_UPDATE callbacks.
  3. *Don't* add new Sys_var classes or uncle Occam will come
     with his razor to haunt you at nights

  Note - all storage engine variables (for example myisam_whatever)
  should go into the corresponding storage engine sources
  (for example in storage/myisam/ha_myisam.cc) !
*/

#include "sys_vars.h"

#include "my_aes.h"                      // my_aes_opmode_names
#include "myisam.h"                      // myisam_flush
#include "auth_common.h"                 // validate_user_plugins
#include "binlog.h"                      // mysql_bin_log
#include "connection_handler_impl.h"     // Per_thread_connection_handler
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "debug_sync.h"                  // DEBUG_SYNC
#include "derror.h"                      // read_texts
#include "events.h"                      // Events
#include "hostname.h"                    // host_cache_resize
#include "item_timefunc.h"               // ISO_FORMAT
#include "log_event.h"                   // MAX_MAX_ALLOWED_PACKET
#include "rpl_info_factory.h"            // Rpl_info_factory
#include "rpl_info_handler.h"            // INFO_REPOSITORY_FILE
#include "rpl_mi.h"                      // Master_info
#include "rpl_msr.h"                     // channel_map
#include "rpl_mts_submode.h"             // MTS_PARALLEL_TYPE_DB_NAME
#include "rpl_rli.h"                     // Relay_log_info
#include "rpl_slave.h"                   // SLAVE_THD_TYPE
#include "socket_connection.h"           // MY_BIND_ALL_ADDRESSES
#include "sp_head.h"                     // SP_PSI_STATEMENT_INFO_COUNT
#include "sql_parse.h"                   // killall_non_super_threads
#include "sql_show.h"                    // opt_ignore_db_dirs
#include "sql_tmp_table.h"               // internal_tmp_disk_storage_engine
#include "sql_time.h"                    // global_date_format
#include "table_cache.h"                 // Table_cache_manager
#include "transaction.h"                 // trans_commit_stmt
#include "rpl_write_set_handler.h"       // transaction_write_set_hashing_algorithms
#include "rpl_group_replication.h"       // is_group_replication_running

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "../storage/perfschema/pfs_server.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

TYPELIB bool_typelib={ array_elements(bool_values)-1, "", bool_values, 0 };

static bool update_buffer_size(THD *thd, KEY_CACHE *key_cache,
                               ptrdiff_t offset, ulonglong new_value)
{
  bool error= false;
  DBUG_ASSERT(offset == offsetof(KEY_CACHE, param_buff_size));

  if (new_value == 0)
  {
    if (key_cache == dflt_key_cache)
    {
      my_error(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE, MYF(0));
      return true;
    }

    if (key_cache->key_cache_inited)            // If initied
    {
      /*
        Move tables using this key cache to the default key cache
        and clear the old key cache.
      */
      key_cache->in_init= 1;
      mysql_mutex_unlock(&LOCK_global_system_variables);
      key_cache->param_buff_size= 0;
      ha_resize_key_cache(key_cache);
      ha_change_key_cache(key_cache, dflt_key_cache);
      /*
        We don't delete the key cache as some running threads my still be in
        the key cache code with a pointer to the deleted (empty) key cache
      */
      mysql_mutex_lock(&LOCK_global_system_variables);
      key_cache->in_init= 0;
    }
    return error;
  }

  key_cache->param_buff_size= new_value;

  /* If key cache didn't exist initialize it, else resize it */
  key_cache->in_init= 1;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (!key_cache->key_cache_inited)
    error= ha_init_key_cache(0, key_cache);
  else
    error= ha_resize_key_cache(key_cache);

  mysql_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;

  return error;
}

static bool update_keycache_param(THD *thd, KEY_CACHE *key_cache,
                                  ptrdiff_t offset, ulonglong new_value)
{
  bool error= false;
  DBUG_ASSERT(offset != offsetof(KEY_CACHE, param_buff_size));

  keycache_var(key_cache, offset)= new_value;

  key_cache->in_init= 1;
  mysql_mutex_unlock(&LOCK_global_system_variables);
  error= ha_resize_key_cache(key_cache);

  mysql_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;

  return error;
}

/*
  The rule for this file: everything should be 'static'. When a sys_var
  variable or a function from this file is - in very rare cases - needed
  elsewhere it should be explicitly declared 'export' here to show that it's
  not a mistakenly forgotten 'static' keyword.
*/
#define export /* not static */

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#ifndef EMBEDDED_LIBRARY

#define PFS_TRAILING_PROPERTIES \
  NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL), \
  NULL, sys_var::PARSE_EARLY

static Sys_var_mybool Sys_pfs_enabled(
       "performance_schema",
       "Enable the performance schema.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_charptr Sys_pfs_instrument(
       "performance_schema_instrument",
       "Default startup value for a performance schema instrument.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_pfs_instrument),
       CMD_LINE(OPT_ARG, OPT_PFS_INSTRUMENT),
       IN_FS_CHARSET,
       DEFAULT(""),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_stages_current(
       "performance_schema_consumer_events_stages_current",
       "Default startup value for the events_stages_current consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_stages_current_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_stages_history(
       "performance_schema_consumer_events_stages_history",
       "Default startup value for the events_stages_history consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_stages_history_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_stages_history_long(
       "performance_schema_consumer_events_stages_history_long",
       "Default startup value for the events_stages_history_long consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_stages_history_long_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_statements_current(
       "performance_schema_consumer_events_statements_current",
       "Default startup value for the events_statements_current consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_statements_current_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_statements_history(
       "performance_schema_consumer_events_statements_history",
       "Default startup value for the events_statements_history consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_statements_history_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_statements_history_long(
       "performance_schema_consumer_events_statements_history_long",
       "Default startup value for the events_statements_history_long consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_statements_history_long_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_transactions_current(
       "performance_schema_consumer_events_transactions_current",
       "Default startup value for the events_transactions_current consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_transactions_current_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_transactions_history(
       "performance_schema_consumer_events_transactions_history",
       "Default startup value for the events_transactions_history consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_transactions_history_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_transactions_history_long(
       "performance_schema_consumer_events_transactions_history_long",
       "Default startup value for the events_transactions_history_long consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_transactions_history_long_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_waits_current(
       "performance_schema_consumer_events_waits_current",
       "Default startup value for the events_waits_current consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_waits_current_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_waits_history(
       "performance_schema_consumer_events_waits_history",
       "Default startup value for the events_waits_history consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_waits_history_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_events_waits_history_long(
       "performance_schema_consumer_events_waits_history_long",
       "Default startup value for the events_waits_history_long consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_events_waits_history_long_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_global_instrumentation(
       "performance_schema_consumer_global_instrumentation",
       "Default startup value for the global_instrumentation consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_global_instrumentation_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_thread_instrumentation(
       "performance_schema_consumer_thread_instrumentation",
       "Default startup value for the thread_instrumentation consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_thread_instrumentation_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_mybool Sys_pfs_consumer_statement_digest(
       "performance_schema_consumer_statements_digest",
       "Default startup value for the statements_digest consumer.",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_consumer_statement_digest_enabled),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_waits_history_long_size(
       "performance_schema_events_waits_history_long_size",
       "Number of rows in EVENTS_WAITS_HISTORY_LONG."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_waits_history_long_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_waits_history_size(
       "performance_schema_events_waits_history_size",
       "Number of rows per thread in EVENTS_WAITS_HISTORY."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_waits_history_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_cond_classes(
       "performance_schema_max_cond_classes",
       "Maximum number of condition instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_cond_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_COND_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_cond_instances(
       "performance_schema_max_cond_instances",
       "Maximum number of instrumented condition objects."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_cond_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_program_instances(
       "performance_schema_max_program_instances",
       "Maximum number of instrumented programs."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_program_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_prepared_stmt_instances(
       "performance_schema_max_prepared_statements_instances",
       "Maximum number of instrumented prepared statements."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_prepared_stmt_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_file_classes(
       "performance_schema_max_file_classes",
       "Maximum number of file instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_file_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_FILE_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_file_handles(
       "performance_schema_max_file_handles",
       "Maximum number of opened instrumented files.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_file_handle_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024*1024),
       DEFAULT(PFS_MAX_FILE_HANDLE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_file_instances(
       "performance_schema_max_file_instances",
       "Maximum number of instrumented files."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_file_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_sockets(
       "performance_schema_max_socket_instances",
       "Maximum number of opened instrumented sockets."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_socket_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_socket_classes(
       "performance_schema_max_socket_classes",
       "Maximum number of socket instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_socket_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_SOCKET_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_mutex_classes(
       "performance_schema_max_mutex_classes",
       "Maximum number of mutex instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_mutex_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_MUTEX_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_mutex_instances(
       "performance_schema_max_mutex_instances",
       "Maximum number of instrumented MUTEX objects."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_mutex_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 100*1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_rwlock_classes(
       "performance_schema_max_rwlock_classes",
       "Maximum number of rwlock instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_rwlock_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_RWLOCK_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_rwlock_instances(
       "performance_schema_max_rwlock_instances",
       "Maximum number of instrumented RWLOCK objects."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_rwlock_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 100*1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_handles(
       "performance_schema_max_table_handles",
       "Maximum number of opened instrumented tables."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_table_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_instances(
       "performance_schema_max_table_instances",
       "Maximum number of instrumented tables."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_table_share_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_lock_stat(
       "performance_schema_max_table_lock_stat",
       "Maximum number of lock statistics for instrumented tables."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_table_lock_stat_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_index_stat(
       "performance_schema_max_index_stat",
       "Maximum number of index statistics for instrumented tables."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_index_stat_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_thread_classes(
       "performance_schema_max_thread_classes",
       "Maximum number of thread instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_thread_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_THREAD_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_thread_instances(
       "performance_schema_max_thread_instances",
       "Maximum number of instrumented threads."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_thread_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_setup_actors_size(
       "performance_schema_setup_actors_size",
       "Maximum number of rows in SETUP_ACTORS."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_setup_actor_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_setup_objects_size(
       "performance_schema_setup_objects_size",
       "Maximum number of rows in SETUP_OBJECTS."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_setup_object_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_accounts_size(
       "performance_schema_accounts_size",
       "Maximum number of instrumented user@host accounts."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_account_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_hosts_size(
       "performance_schema_hosts_size",
       "Maximum number of instrumented hosts."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_host_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_users_size(
       "performance_schema_users_size",
       "Maximum number of instrumented users."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_user_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_stage_classes(
       "performance_schema_max_stage_classes",
       "Maximum number of stage instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_stage_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT(PFS_MAX_STAGE_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_stages_history_long_size(
       "performance_schema_events_stages_history_long_size",
       "Number of rows in EVENTS_STAGES_HISTORY_LONG."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_stages_history_long_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_stages_history_size(
       "performance_schema_events_stages_history_size",
       "Number of rows per thread in EVENTS_STAGES_HISTORY."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_stages_history_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

/**
  Variable performance_schema_max_statement_classes.
  The default number of statement classes is the sum of:
  - COM_END for all regular "statement/com/...",
  - 1 for "statement/com/new_packet", for unknown enum_server_command
  - 1 for "statement/com/Error", for invalid enum_server_command
  - SQLCOM_END for all regular "statement/sql/...",
  - 1 for "statement/sql/error", for invalid enum_sql_command.
  - SP_PSI_STATEMENT_INFO_COUNT for "statement/sp/...". 
  - 1 for "statement/rpl/relay_log", for replicated statements.
  - 1 for "statement/scheduler/event", for scheduled events.
*/
static Sys_var_ulong Sys_pfs_max_statement_classes(
       "performance_schema_max_statement_classes",
       "Maximum number of statement instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_statement_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
       DEFAULT((ulong) SQLCOM_END + (ulong) COM_END + 5 + SP_PSI_STATEMENT_INFO_COUNT),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_statements_history_long_size(
       "performance_schema_events_statements_history_long_size",
       "Number of rows in EVENTS_STATEMENTS_HISTORY_LONG."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_statements_history_long_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_statements_history_size(
       "performance_schema_events_statements_history_size",
       "Number of rows per thread in EVENTS_STATEMENTS_HISTORY."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_statements_history_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_statement_stack_size(
       "performance_schema_max_statement_stack",
       "Number of rows per thread in EVENTS_STATEMENTS_CURRENT.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_statement_stack_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(1, 256),
       DEFAULT(PFS_STATEMENTS_STACK_SIZE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_memory_classes(
       "performance_schema_max_memory_classes",
       "Maximum number of memory pool instruments.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_memory_class_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024),
       DEFAULT(PFS_MAX_MEMORY_CLASS),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_digest_size(
       "performance_schema_digests_size",
       "Size of the statement digest."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_digest_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_transactions_history_long_size(
       "performance_schema_events_transactions_history_long_size",
       "Number of rows in EVENTS_TRANSACTIONS_HISTORY_LONG."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_transactions_history_long_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024*1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_transactions_history_size(
       "performance_schema_events_transactions_history_size",
       "Number of rows per thread in EVENTS_TRANSACTIONS_HISTORY."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_events_transactions_history_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_digest_length(
       "performance_schema_max_digest_length",
       "Maximum length considered for digest text, when stored in performance_schema tables.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_max_digest_length),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024 * 1024),
       DEFAULT(1024),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_connect_attrs_size(
       "performance_schema_session_connect_attrs_size",
       "Size of session attribute string buffer per thread."
         " Use 0 to disable, -1 for automated sizing.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_session_connect_attrs_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
       DEFAULT(PFS_AUTOSIZE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_metadata_locks(
       "performance_schema_max_metadata_locks",
       "Maximum number of metadata locks."
         " Use 0 to disable, -1 for automated scaling.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_metadata_lock_sizing),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 100*1024*1024),
       DEFAULT(PFS_AUTOSCALE_VALUE),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_sql_text_length(
       "performance_schema_max_sql_text_length",
       "Maximum length of displayed sql text.",
       READ_ONLY GLOBAL_VAR(pfs_param.m_max_sql_text_length),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024 * 1024),
       DEFAULT(1024),
       BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

#endif /* EMBEDDED_LIBRARY */
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

static Sys_var_ulong Sys_auto_increment_increment(
       "auto_increment_increment",
       "Auto-increment columns are incremented by this",
       SESSION_VAR(auto_increment_increment),
       CMD_LINE(OPT_ARG),
       VALID_RANGE(1, 65535), DEFAULT(1), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG);

static Sys_var_ulong Sys_auto_increment_offset(
       "auto_increment_offset",
       "Offset added to Auto-increment columns. Used when "
       "auto-increment-increment != 1",
       SESSION_VAR(auto_increment_offset),
       CMD_LINE(OPT_ARG),
       VALID_RANGE(1, 65535), DEFAULT(1), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG);

static Sys_var_mybool Sys_automatic_sp_privileges(
       "automatic_sp_privileges",
       "Creating and dropping stored procedures alters ACLs",
       GLOBAL_VAR(sp_automatic_privileges),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static Sys_var_ulong Sys_back_log(
       "back_log", "The number of outstanding connection requests "
       "MySQL can have. This comes into play when the main MySQL thread "
       "gets very many connection requests in a very short time",
       READ_ONLY GLOBAL_VAR(back_log), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_charptr Sys_basedir(
       "basedir", "Path to installation directory. All paths are "
       "usually resolved relative to this",
       READ_ONLY GLOBAL_VAR(mysql_home_ptr), CMD_LINE(REQUIRED_ARG, 'b'),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_default_authentication_plugin(
       "default_authentication_plugin", "The default authentication plugin "
       "used by the server to hash the password.",
       READ_ONLY GLOBAL_VAR(default_auth_plugin), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT("mysql_native_password"));

static PolyLock_mutex Plock_default_password_lifetime(
                        &LOCK_default_password_lifetime);
static Sys_var_uint Sys_default_password_lifetime(
       "default_password_lifetime", "The number of days after which the "
       "password will expire.",
       GLOBAL_VAR(default_password_lifetime), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX16), DEFAULT(0), BLOCK_SIZE(1),
       &Plock_default_password_lifetime);

#ifndef EMBEDDED_LIBRARY
static Sys_var_charptr Sys_my_bind_addr(
       "bind_address", "IP address to bind to.",
       READ_ONLY GLOBAL_VAR(my_bind_addr_str), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(MY_BIND_ALL_ADDRESSES));
#endif

static bool fix_binlog_cache_size(sys_var *self, THD *thd, enum_var_type type)
{
  check_binlog_cache_size(thd);
  return false;
}

static bool fix_binlog_stmt_cache_size(sys_var *self, THD *thd, enum_var_type type)
{
  check_binlog_stmt_cache_size(thd);
  return false;
}

static Sys_var_ulong Sys_binlog_cache_size(
       "binlog_cache_size", "The size of the transactional cache for "
       "updates to transactional engines for the binary log. "
       "If you often use transactions containing many statements, "
       "you can increase this to get more performance",
       GLOBAL_VAR(binlog_cache_size),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE, ULONG_MAX), DEFAULT(32768), BLOCK_SIZE(IO_SIZE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_binlog_cache_size));

static Sys_var_ulong Sys_binlog_stmt_cache_size(
       "binlog_stmt_cache_size", "The size of the statement cache for "
       "updates to non-transactional engines for the binary log. "
       "If you often use statements updating a great number of rows, "
       "you can increase this to get more performance",
       GLOBAL_VAR(binlog_stmt_cache_size),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE, ULONG_MAX), DEFAULT(32768), BLOCK_SIZE(IO_SIZE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_binlog_stmt_cache_size));

static Sys_var_int32 Sys_binlog_max_flush_queue_time(
       "binlog_max_flush_queue_time",
       "The maximum time that the binary log group commit will keep reading"
       " transactions before it flush the transactions to the binary log (and"
       " optionally sync, depending on the value of sync_binlog).",
       GLOBAL_VAR(opt_binlog_max_flush_queue_time),
       CMD_LINE(REQUIRED_ARG, OPT_BINLOG_MAX_FLUSH_QUEUE_TIME),
       VALID_RANGE(0, 100000), DEFAULT(0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_ulong Sys_binlog_group_commit_sync_delay(
       "binlog_group_commit_sync_delay",
       "The number of microseconds the server waits for the "
       "binary log group commit sync queue to fill before "
       "continuing. Default: 0. Min: 0. Max: 1000000.",
       GLOBAL_VAR(opt_binlog_group_commit_sync_delay),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 1000000 /* max 1 sec */), DEFAULT(0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG);

static Sys_var_ulong Sys_binlog_group_commit_sync_no_delay_count(
       "binlog_group_commit_sync_no_delay_count",
       "If there are this many transactions in the commit sync "
       "queue and the server is waiting for more transactions "
       "to be enqueued (as set using --binlog-group-commit-sync-delay), "
       "the commit procedure resumes.",
       GLOBAL_VAR(opt_binlog_group_commit_sync_no_delay_count),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 100000 /* max connections */),
       DEFAULT(0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG);

static bool check_has_super(sys_var *self, THD *thd, set_var *var)
{
  DBUG_ASSERT(self->scope() != sys_var::GLOBAL);// don't abuse check_has_super()
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!(thd->security_context()->check_access(SUPER_ACL)))
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return true;
  }
#endif
  return false;
}

static bool check_outside_trx(sys_var *self, THD *thd, set_var *var)
{
  if (thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), var->var->name.str);
    return true;
  }
  if (!thd->owned_gtid.is_empty())
  {
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    if (thd->owned_gtid.sidno > 0)
      thd->owned_gtid.to_string(thd->owned_sid, buf);
    else
      strcpy(buf, "ANONYMOUS");
    my_error(ER_CANT_SET_VARIABLE_WHEN_OWNING_GTID, MYF(0), var->var->name.str, buf);
    return true;
  }
  return false;
}

static bool check_super_outside_trx_outside_sf(sys_var *self, THD *thd, set_var *var)
{
  if (thd->in_sub_stmt)
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), var->var->name.str);
    return true;
  }
  if (check_outside_trx(self, thd, var))
    return true;
  if (self->scope() != sys_var::GLOBAL)
    return check_has_super(self, thd, var);
  return false;
}

static bool check_explicit_defaults_for_timestamp(sys_var *self, THD *thd, set_var *var)
{
  if (thd->in_sub_stmt)
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), var->var->name.str);
    return true;
  }
  if (thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), var->var->name.str);
    return true;
  }
  if (self->scope() != sys_var::GLOBAL)
    return check_has_super(self, thd, var);
  return false;
}

#ifdef HAVE_REPLICATION
/**
  Check-function to @@GTID_NEXT system variable.

  @param self   a pointer to the sys_var, i.e. gtid_next
  @param thd    a reference to THD object
  @param var    a pointer to the set_var created by the parser.

  @return @c false if the change is allowed, otherwise @c true.
*/

static bool check_gtid_next(sys_var *self, THD *thd, set_var *var)
{
  bool is_prepared_trx=
    thd->get_transaction()->xid_state()->has_state(XID_STATE::XA_PREPARED);

  if (thd->in_sub_stmt)
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), var->var->name.str);
    return true;
  }
  if (!is_prepared_trx && thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), var->var->name.str);
    return true;
  }
  return check_has_super(self, thd, var);
}
#endif

static bool check_super_outside_trx_outside_sf_outside_sp(sys_var *self, THD *thd, set_var *var)
{
  if (check_super_outside_trx_outside_sf(self, thd, var))
    return true;
  if (thd->lex->sphead)
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SP, MYF(0), var->var->name.str);
    return true;
  }
  return false;
}

static bool binlog_format_check(sys_var *self, THD *thd, set_var *var)
{
  if (check_has_super(self, thd, var))
    return true;

  if (var->type == OPT_GLOBAL)
    return false;

  /*
     If RBR and open temporary tables, their CREATE TABLE may not be in the
     binlog, so we can't toggle to SBR in this connection.

     If binlog_format=MIXED, there are open temporary tables, and an unsafe
     statement is executed, then subsequent statements are logged in row
     format and hence changes to temporary tables may be lost. So we forbid
     switching @@SESSION.binlog_format from MIXED to STATEMENT when there are
     open temp tables and we are logging in row format.
  */
  if (thd->temporary_tables && var->type == OPT_SESSION &&
      var->save_result.ulonglong_value == BINLOG_FORMAT_STMT &&
      ((thd->variables.binlog_format == BINLOG_FORMAT_MIXED &&
        thd->is_current_stmt_binlog_format_row()) ||
       thd->variables.binlog_format == BINLOG_FORMAT_ROW))
  {
    my_error(ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR, MYF(0));
    return true;
  }

  /*
    if in a stored function/trigger, it's too late to change mode
  */
  if (thd->in_sub_stmt)
  {
    my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT, MYF(0));
    return true;
  }
  /*
    Make the session variable 'binlog_format' read-only inside a transaction.
  */
  if (thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_FORMAT, MYF(0));
    return true;
  }

  return false;
}

static bool fix_binlog_format_after_update(sys_var *self, THD *thd,
                                           enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->reset_current_stmt_binlog_format_row();
  return false;
}

static bool prevent_global_rbr_exec_mode_idempotent(sys_var *self, THD *thd,
                                                    set_var *var )
{
  if (var->type == OPT_GLOBAL)
  {
    my_error(ER_LOCAL_VARIABLE, MYF(0), self->name.str);
    return true;
  }
  return false;
}

static Sys_var_test_flag Sys_core_file(
       "core_file", "write a core-file on crashes", TEST_CORE_ON_SIGNAL);

static Sys_var_enum Sys_binlog_format(
       "binlog_format", "What form of binary logging the master will "
       "use: either ROW for row-based binary logging, STATEMENT "
       "for statement-based binary logging, or MIXED. MIXED is statement-"
       "based binary logging except for those statements where only row-"
       "based is correct: those which involve user-defined functions (i.e. "
       "UDFs) or the UUID() function; for those, row-based binary logging is "
       "automatically used. If NDBCLUSTER is enabled and binlog-format is "
       "MIXED, the format switches to row-based and back implicitly per each "
       "query accessing an NDBCLUSTER table",
       SESSION_VAR(binlog_format), CMD_LINE(REQUIRED_ARG, OPT_BINLOG_FORMAT),
       binlog_format_names, DEFAULT(BINLOG_FORMAT_ROW),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(binlog_format_check),
       ON_UPDATE(fix_binlog_format_after_update));

static const char *rbr_exec_mode_names[]=
       {"STRICT", "IDEMPOTENT", 0};
static Sys_var_enum rbr_exec_mode(
       "rbr_exec_mode",
       "Modes for how row events should be executed. Legal values "
       "are STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, "
       "the server will not throw errors for operations that are idempotent. "
       "In STRICT mode, server will throw errors for the operations that "
       "cause a conflict.",
       SESSION_VAR(rbr_exec_mode_options), NO_CMD_LINE,
       rbr_exec_mode_names, DEFAULT(RBR_EXEC_MODE_STRICT),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(prevent_global_rbr_exec_mode_idempotent),
       ON_UPDATE(NULL));

static const char *binlog_row_image_names[]= {"MINIMAL", "NOBLOB", "FULL", NullS};
static Sys_var_enum Sys_binlog_row_image(
       "binlog_row_image", 
       "Controls whether rows should be logged in 'FULL', 'NOBLOB' or "
       "'MINIMAL' formats. 'FULL', means that all columns in the before "
       "and after image are logged. 'NOBLOB', means that mysqld avoids logging "
       "blob columns whenever possible (eg, blob column was not changed or "
       "is not part of primary key). 'MINIMAL', means that a PK equivalent (PK "
       "columns or full row if there is no PK in the table) is logged in the "
       "before image, and only changed columns are logged in the after image. "
       "(Default: FULL).",
       SESSION_VAR(binlog_row_image), CMD_LINE(REQUIRED_ARG),
       binlog_row_image_names, DEFAULT(BINLOG_ROW_IMAGE_FULL),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(NULL));

static bool on_session_track_gtids_update(sys_var *self, THD *thd,
                                          enum_var_type type)
{
  thd->session_tracker.get_tracker(SESSION_GTIDS_TRACKER)->update(thd);
  return false;
}

static const char *session_track_gtids_names[]=
  { "OFF", "OWN_GTID", "ALL_GTIDS", NullS };
static Sys_var_enum Sys_session_track_gtids(
       "session_track_gtids",
       "Controls the amount of global transaction ids to be "
       "included in the response packet sent by the server."
       "(Default: OFF).",
       SESSION_VAR(session_track_gtids), CMD_LINE(REQUIRED_ARG),
       session_track_gtids_names, DEFAULT(OFF),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_outside_trx),
       ON_UPDATE(on_session_track_gtids_update));

static bool binlog_direct_check(sys_var *self, THD *thd, set_var *var)
{
  if (check_has_super(self, thd, var))
    return true;

  if (var->type == OPT_GLOBAL)
    return false;

   /*
     Makes the session variable 'binlog_direct_non_transactional_updates'
     read-only if within a procedure, trigger or function.
   */
   if (thd->in_sub_stmt)
   {
     my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_DIRECT, MYF(0));
     return true;
   }
   /*
     Makes the session variable 'binlog_direct_non_transactional_updates'
     read-only inside a transaction.
   */
   if (thd->in_active_multi_stmt_transaction())
   {
     my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_DIRECT, MYF(0));
     return true;
   }

  return false;
}

static Sys_var_mybool Sys_binlog_direct(
       "binlog_direct_non_transactional_updates",
       "Causes updates to non-transactional engines using statement format to "
       "be written directly to binary log. Before using this option make sure "
       "that there are no dependencies between transactional and "
       "non-transactional tables such as in the statement INSERT INTO t_myisam "
       "SELECT * FROM t_innodb; otherwise, slaves may diverge from the master.",
       SESSION_VAR(binlog_direct_non_trans_update),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(binlog_direct_check));

/**
  This variable is read only to users. It can be enabled or disabled
  only at mysqld startup. This variable is used by User thread and
  as well as by replication slave applier thread to apply relay_log.
  Slave applier thread enables/disables this option based on
  relay_log's from replication master versions. There is possibility of
  slave applier thread and User thread to have different setting for
  explicit_defaults_for_timestamp, hence this options is defined as
  SESSION_VAR rather than GLOBAL_VAR.
*/
static Sys_var_mybool Sys_explicit_defaults_for_timestamp(
       "explicit_defaults_for_timestamp",
       "This option causes CREATE TABLE to create all TIMESTAMP columns "
       "as NULL with DEFAULT NULL attribute, Without this option, "
       "TIMESTAMP columns are NOT NULL and have implicit DEFAULT clauses. "
       "The old behavior is deprecated. "
       "The variable can only be set by users having the SUPER privilege.",
       SESSION_VAR(explicit_defaults_for_timestamp),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_explicit_defaults_for_timestamp));

static bool repository_check(sys_var *self, THD *thd, set_var *var, SLAVE_THD_TYPE thread_mask)
{
  bool ret= FALSE;
  if (check_super_outside_trx_outside_sf(self, thd, var))
    return TRUE;
#ifdef HAVE_REPLICATION
  Master_info *mi;
  int running= 0;
  const char *msg= NULL;
  bool rpl_info_option= static_cast<uint>(var->save_result.ulonglong_value);

  /* don't convert if the repositories are same */
  if (rpl_info_option == (thread_mask== SLAVE_THD_IO ?
                          opt_mi_repository_id: opt_rli_repository_id))
      return FALSE;

  channel_map.wrlock();

  /* Repository conversion not possible, when multiple channels exist */
  if (channel_map.get_num_instances(true) > 1)
  {
      msg= "Repository conversion is possible when only default channel exists";
      my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
      channel_map.unlock();
      return TRUE;
  }

  mi= channel_map.get_default_channel_mi();

  if (mi != NULL)
  {
    lock_slave_threads(mi);
    init_thread_mask(&running, mi, FALSE);
    if(!running)
    {
      switch (thread_mask)
      {
        case SLAVE_THD_IO:
        if (Rpl_info_factory::
            change_mi_repository(mi,
                                 static_cast<uint>(var->save_result.
                                                   ulonglong_value),
                                 &msg))
        {
          ret= TRUE;
          my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
        }
        break;
        case SLAVE_THD_SQL:
          mts_recovery_groups(mi->rli);
          if (!mi->rli->is_mts_recovery())
          {
            if (Rpl_info_factory::reset_workers(mi->rli) ||
                Rpl_info_factory::
                change_rli_repository(mi->rli,
                                      static_cast<uint>(var->save_result.
                                                        ulonglong_value),
                                      &msg))
            {
              ret= TRUE;
              my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
            }
          }
          else
            sql_print_warning("It is not possible to change the type of the "
                              "relay log's repository because there are workers' "
                              "repositories with gaps. Please, fix the gaps first "
                              "before doing such change.");
        break;
        default:
          assert(0);
        break;
      }
    }
    else
    {
      ret= TRUE;
      my_error(ER_SLAVE_CHANNEL_MUST_STOP, MYF(0),mi->get_channel());
    }
    unlock_slave_threads(mi);
  }
  channel_map.unlock();
#endif
  return ret;
}

static bool relay_log_info_repository_check(sys_var *self, THD *thd, set_var *var)
{
  return repository_check(self, thd, var, SLAVE_THD_SQL);
}

static bool master_info_repository_check(sys_var *self, THD *thd, set_var *var)
{
  return repository_check(self, thd, var, SLAVE_THD_IO);
}

static const char *repository_names[]=
{
  "FILE", "TABLE",
#ifndef DBUG_OFF
  "DUMMY",
#endif
  0
};

ulong opt_mi_repository_id= INFO_REPOSITORY_FILE;
static Sys_var_enum Sys_mi_repository(
       "master_info_repository",
       "Defines the type of the repository for the master information."
       ,GLOBAL_VAR(opt_mi_repository_id), CMD_LINE(REQUIRED_ARG),
       repository_names, DEFAULT(INFO_REPOSITORY_FILE), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(master_info_repository_check),
       ON_UPDATE(0));

ulong opt_rli_repository_id= INFO_REPOSITORY_FILE;
static Sys_var_enum Sys_rli_repository(
       "relay_log_info_repository",
       "Defines the type of the repository for the relay log information "
       "and associated workers."
       ,GLOBAL_VAR(opt_rli_repository_id), CMD_LINE(REQUIRED_ARG),
       repository_names, DEFAULT(INFO_REPOSITORY_FILE), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(relay_log_info_repository_check),
       ON_UPDATE(0));

static Sys_var_mybool Sys_binlog_rows_query(
       "binlog_rows_query_log_events",
       "Allow writing of Rows_query_log events into binary log.",
       SESSION_VAR(binlog_rows_query_log_events),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_binlog_order_commits(
       "binlog_order_commits",
       "Issue internal commit calls in the same order as transactions are"
       " written to the binary log. Default is to order commits.",
       GLOBAL_VAR(opt_binlog_order_commits),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static Sys_var_ulong Sys_bulk_insert_buff_size(
       "bulk_insert_buffer_size", "Size of tree cache used in bulk "
       "insert optimisation. Note that this is a limit per thread!",
       SESSION_VAR(bulk_insert_buff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(8192*1024), BLOCK_SIZE(1));

static Sys_var_charptr Sys_character_sets_dir(
       "character_sets_dir", "Directory where character sets are",
       READ_ONLY GLOBAL_VAR(charsets_dir), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static bool check_not_null(sys_var *self, THD *thd, set_var *var)
{
  return var->value && var->value->is_null();
}


/**
  Check storage engine is not empty and log warning.

  Checks if default_storage_engine or default_tmp_storage_engine is set
  empty and return true. This method also logs warning if the
  storage engine set is a disabled storage engine specified in
  disabled_storage_engines.

  @param self    pointer to system variable object.
  @param thd     Connection handle.
  @param var     pointer to set variable object.

  @return  true if the set variable is empty.
           false if the set variable is not empty.
*/
static bool check_storage_engine(sys_var *self, THD *thd, set_var *var)
{
  if (check_not_null(self,thd,var))
    return true;

  if (!opt_bootstrap && !opt_noacl)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;
    LEX_STRING se_name;

    if (var->value)
    {
      res= var->value->val_str(&str);
      lex_string_set(&se_name, res->ptr());
    }
    else
    {
      // Use the default value defined by sys_var.
      lex_string_set(&se_name,
        reinterpret_cast<const char*>(
        dynamic_cast<Sys_var_plugin*>(self)->global_value_ptr(thd, NULL)));
    }

    plugin_ref plugin;
    if ((plugin= ha_resolve_by_name(NULL, &se_name, FALSE)))
    {
      handlerton *hton= plugin_data<handlerton*>(plugin);
      if (ha_is_storage_engine_disabled(hton))
        sql_print_warning("%s is set to a disabled storage engine %s.",
                          self->name.str, se_name.str);
      plugin_unlock(NULL, plugin);
    }
  }
  return false;
}

static bool check_charset(sys_var *self, THD *thd, set_var *var)
{
  if (!var->value)
    return false;

  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == STRING_RESULT)
  {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res= var->value->val_str(&str)))
      var->save_result.ptr= NULL;
    else
    {
      ErrConvString err(res); /* Get utf8 '\0' terminated string */
      if (!(var->save_result.ptr= get_charset_by_csname(err.ptr(),
                                                         MY_CS_PRIMARY,
                                                         MYF(0))) &&
          !(var->save_result.ptr= get_old_charset_by_name(err.ptr())))
      {
        my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), err.ptr());
        return true;
      }
    }
  }
  else // INT_RESULT
  {
    int csno= (int)var->value->val_int();
    if (!(var->save_result.ptr= get_charset(csno, MYF(0))))
    {
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), llstr(csno, buff));
      return true;
    }
  }
  return false;
}
static bool check_charset_not_null(sys_var *self, THD *thd, set_var *var)
{
  return check_charset(self, thd, var) || check_not_null(self, thd, var);
}
static Sys_var_struct Sys_character_set_system(
       "character_set_system", "The character set used by the server "
       "for storing identifiers",
       READ_ONLY GLOBAL_VAR(system_charset_info), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(0));

static Sys_var_struct Sys_character_set_server(
       "character_set_server", "The default character set",
       SESSION_VAR(collation_server), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_charset_not_null));

static bool check_charset_db(sys_var *self, THD *thd, set_var *var)
{
  if (check_charset_not_null(self, thd, var))
    return true;
  if (!var->value) // = DEFAULT
    var->save_result.ptr= thd->db_charset;
  return false;
}
static bool update_deprecated(sys_var *self, THD *thd, enum_var_type type)
{
  push_warning_printf(thd, Sql_condition::SL_WARNING,
                      ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
                      ER_THD(thd, ER_WARN_DEPRECATED_SYSVAR_UPDATE),
                      self->name.str);
  return false;
}
static Sys_var_struct Sys_character_set_database(
       "character_set_database",
       " The character set used by the default database",
       SESSION_VAR(collation_database), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_charset_db),
       ON_UPDATE(update_deprecated));

static bool check_cs_client(sys_var *self, THD *thd, set_var *var)
{
  if (check_charset_not_null(self, thd, var))
    return true;

  // Currently, UCS-2 cannot be used as a client character set
  if (((CHARSET_INFO *)(var->save_result.ptr))->mbminlen > 1)
    return true;

  return false;
}
static bool fix_thd_charset(sys_var *self, THD *thd, enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->update_charset();
  return false;
}
static Sys_var_struct Sys_character_set_client(
       "character_set_client", "The character set for statements "
       "that arrive from the client",
       SESSION_VAR(character_set_client), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_cs_client),
       ON_UPDATE(fix_thd_charset));

static Sys_var_struct Sys_character_set_connection(
       "character_set_connection", "The character set used for "
       "literals that do not have a character set introducer and for "
       "number-to-string conversion",
       SESSION_VAR(collation_connection), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_charset_not_null),
       ON_UPDATE(fix_thd_charset));

static Sys_var_struct Sys_character_set_results(
       "character_set_results", "The character set used for returning "
       "query results to the client",
       SESSION_VAR(character_set_results), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_charset));

static Sys_var_struct Sys_character_set_filesystem(
       "character_set_filesystem", "The filesystem character set",
       SESSION_VAR(character_set_filesystem), NO_CMD_LINE,
       offsetof(CHARSET_INFO, csname), DEFAULT(&character_set_filesystem),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_charset_not_null),
       ON_UPDATE(fix_thd_charset));

static const char *completion_type_names[]= {"NO_CHAIN", "CHAIN", "RELEASE", 0};
static Sys_var_enum Sys_completion_type(
       "completion_type", "The transaction completion type, one of "
       "NO_CHAIN, CHAIN, RELEASE",
       SESSION_VAR(completion_type), CMD_LINE(REQUIRED_ARG),
       completion_type_names, DEFAULT(0));

static bool check_collation_not_null(sys_var *self, THD *thd, set_var *var)
{
  if (!var->value)
    return false;

  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == STRING_RESULT)
  {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res= var->value->val_str(&str)))
      var->save_result.ptr= NULL;
    else
    {
      ErrConvString err(res); /* Get utf8 '\0'-terminated string */
      if (!(var->save_result.ptr= get_charset_by_name(err.ptr(), MYF(0))))
      {
        my_error(ER_UNKNOWN_COLLATION, MYF(0), err.ptr());
        return true;
      }
    }
  }
  else // INT_RESULT
  {
    int csno= (int)var->value->val_int();
    if (!(var->save_result.ptr= get_charset(csno, MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), llstr(csno, buff));
      return true;
    }
  }
  return check_not_null(self, thd, var);
}
static Sys_var_struct Sys_collation_connection(
       "collation_connection", "The collation of the connection "
       "character set",
       SESSION_VAR(collation_connection), NO_CMD_LINE,
       offsetof(CHARSET_INFO, name), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_collation_not_null),
       ON_UPDATE(fix_thd_charset));

static bool check_collation_db(sys_var *self, THD *thd, set_var *var)
{
  if (check_collation_not_null(self, thd, var))
    return true;
  if (!var->value) // = DEFAULT
    var->save_result.ptr= thd->db_charset;
  return false;
}
static Sys_var_struct Sys_collation_database(
       "collation_database", "The collation of the database "
       "character set",
       SESSION_VAR(collation_database), NO_CMD_LINE,
       offsetof(CHARSET_INFO, name), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_collation_db),
       ON_UPDATE(update_deprecated));

static Sys_var_struct Sys_collation_server(
       "collation_server", "The server default collation",
       SESSION_VAR(collation_server), NO_CMD_LINE,
       offsetof(CHARSET_INFO, name), DEFAULT(&default_charset_info),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_collation_not_null));

static const char *concurrent_insert_names[]= {"NEVER", "AUTO", "ALWAYS", 0};
static Sys_var_enum Sys_concurrent_insert(
       "concurrent_insert", "Use concurrent insert with MyISAM. Possible "
       "values are NEVER, AUTO, ALWAYS",
       GLOBAL_VAR(myisam_concurrent_insert), CMD_LINE(OPT_ARG),
       concurrent_insert_names, DEFAULT(1));

static Sys_var_ulong Sys_connect_timeout(
       "connect_timeout",
       "The number of seconds the mysqld server is waiting for a connect "
       "packet before responding with 'Bad handshake'",
       GLOBAL_VAR(connect_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(2, LONG_TIMEOUT), DEFAULT(CONNECT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_charptr Sys_datadir(
       "datadir", "Path to the database root directory",
       READ_ONLY GLOBAL_VAR(mysql_real_data_home_ptr),
       CMD_LINE(REQUIRED_ARG, 'h'), IN_FS_CHARSET, DEFAULT(mysql_real_data_home));

#ifndef DBUG_OFF
static Sys_var_dbug Sys_dbug(
       "debug", "Debug log", sys_var::SESSION,
       CMD_LINE(OPT_ARG, '#'), DEFAULT(""), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_has_super));
#endif

/**
  @todo
    When updating myisam_delay_key_write, we should do a 'flush tables'
    of all MyISAM tables to ensure that they are reopen with the
    new attribute.
*/
export bool fix_delay_key_write(sys_var *self, THD *thd, enum_var_type type)
{
  switch (delay_key_write_options) {
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
  return false;
}
static const char *delay_key_write_names[]= { "OFF", "ON", "ALL", NullS };
static Sys_var_enum Sys_delay_key_write(
       "delay_key_write", "Type of DELAY_KEY_WRITE",
       GLOBAL_VAR(delay_key_write_options), CMD_LINE(OPT_ARG),
       delay_key_write_names, DEFAULT(DELAY_KEY_WRITE_ON),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_delay_key_write));

static Sys_var_ulong Sys_delayed_insert_limit(
       "delayed_insert_limit",
       "After inserting delayed_insert_limit rows, the INSERT DELAYED "
       "handler will check if there are any SELECT statements pending. "
       "If so, it allows these to execute before continuing. "
       "This variable is deprecated along with INSERT DELAYED.",
       GLOBAL_VAR(delayed_insert_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(DELAYED_LIMIT), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_ulong Sys_delayed_insert_timeout(
       "delayed_insert_timeout",
       "How long a INSERT DELAYED thread should wait for INSERT statements "
       "before terminating. "
       "This variable is deprecated along with INSERT DELAYED.",
       GLOBAL_VAR(delayed_insert_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(DELAYED_WAIT_TIMEOUT),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(0), DEPRECATED(""));

static Sys_var_ulong Sys_delayed_queue_size(
       "delayed_queue_size",
       "What size queue (in rows) should be allocated for handling INSERT "
       "DELAYED. If the queue becomes full, any client that does INSERT "
       "DELAYED will wait until there is room in the queue again. "
       "This variable is deprecated along with INSERT DELAYED.",
       GLOBAL_VAR(delayed_queue_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(DELAYED_QUEUE_SIZE), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

#ifndef EMBEDDED_LIBRARY
static const char *event_scheduler_names[]= { "OFF", "ON", "DISABLED", NullS };
static bool event_scheduler_check(sys_var *self, THD *thd, set_var *var)
{
  /* DISABLED is only accepted on the command line */
  if (var->save_result.ulonglong_value == Events::EVENTS_DISABLED)
    return true;
  /*
    If the scheduler was disabled because there are no/bad
    system tables, produce a more meaningful error message
    than ER_OPTION_PREVENTS_STATEMENT
  */
  if (Events::check_if_system_tables_error())
    return true;
  if (Events::opt_event_scheduler == Events::EVENTS_DISABLED)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--event-scheduler=DISABLED or --skip-grant-tables");
    return true;
  }
  return false;
}
static bool event_scheduler_update(sys_var *self, THD *thd, enum_var_type type)
{
  int err_no= 0;
  ulong opt_event_scheduler_value= Events::opt_event_scheduler;
  mysql_mutex_unlock(&LOCK_global_system_variables);
  /*
    Events::start() is heavyweight. In particular it creates a new THD,
    which takes LOCK_global_system_variables internally.
    Thus we have to release it here.
    We need to re-take it before returning, though.

    Note that since we release LOCK_global_system_variables before calling
    start/stop, there is a possibility that the server variable
    can become out of sync with the real event scheduler state.

    This can happen with two concurrent statments if the first gets
    interrupted after start/stop but before retaking
    LOCK_global_system_variables. However, this problem should be quite
    rare and it's difficult to avoid it without opening up possibilities
    for deadlocks. See bug#51160.
  */
  bool ret= opt_event_scheduler_value == Events::EVENTS_ON
            ? Events::start(&err_no)
            : Events::stop();
  mysql_mutex_lock(&LOCK_global_system_variables);
  if (ret)
  {
    Events::opt_event_scheduler= Events::EVENTS_OFF;
    my_error(ER_EVENT_SET_VAR_ERROR, MYF(0), err_no);
  }
  return ret;
}

static Sys_var_enum Sys_event_scheduler(
       "event_scheduler", "Enable the event scheduler. Possible values are "
       "ON, OFF, and DISABLED (keep the event scheduler completely "
       "deactivated, it cannot be activated run-time)",
       GLOBAL_VAR(Events::opt_event_scheduler), CMD_LINE(OPT_ARG),
       event_scheduler_names, DEFAULT(Events::EVENTS_OFF),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(event_scheduler_check), ON_UPDATE(event_scheduler_update));
#endif

static Sys_var_ulong Sys_expire_logs_days(
       "expire_logs_days",
       "If non-zero, binary logs will be purged after expire_logs_days "
       "days; possible purges happen at startup and at binary log rotation",
       GLOBAL_VAR(expire_logs_days),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 99), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_mybool Sys_flush(
       "flush", "Flush MyISAM tables to disk between SQL commands",
       GLOBAL_VAR(myisam_flush),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_ulong Sys_flush_time(
       "flush_time",
       "A dedicated thread is created to flush all tables at the "
       "given interval",
       GLOBAL_VAR(flush_time),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, LONG_TIMEOUT),
       DEFAULT(0), BLOCK_SIZE(1));

static bool check_ftb_syntax(sys_var *self, THD *thd, set_var *var)
{
  return ft_boolean_check_syntax_string((uchar*)
                      (var->save_result.string_value.str));
}
static bool query_cache_flush(sys_var *self, THD *thd, enum_var_type type)
{
  query_cache.flush();
  return false;
}
/// @todo make SESSION_VAR (usability enhancement and a fix for a race condition)
static Sys_var_charptr Sys_ft_boolean_syntax(
       "ft_boolean_syntax", "List of operators for "
       "MATCH ... AGAINST ( ... IN BOOLEAN MODE)",
       GLOBAL_VAR(ft_boolean_syntax),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(DEFAULT_FTB_SYNTAX), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_ftb_syntax), ON_UPDATE(query_cache_flush));

static Sys_var_ulong Sys_ft_max_word_len(
       "ft_max_word_len",
       "The maximum length of the word to be included in a FULLTEXT index. "
       "Note: FULLTEXT indexes must be rebuilt after changing this variable",
       READ_ONLY GLOBAL_VAR(ft_max_word_len), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(10, HA_FT_MAXCHARLEN), DEFAULT(HA_FT_MAXCHARLEN),
       BLOCK_SIZE(1));

static Sys_var_ulong Sys_ft_min_word_len(
       "ft_min_word_len",
       "The minimum length of the word to be included in a FULLTEXT index. "
       "Note: FULLTEXT indexes must be rebuilt after changing this variable",
       READ_ONLY GLOBAL_VAR(ft_min_word_len), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, HA_FT_MAXCHARLEN), DEFAULT(4), BLOCK_SIZE(1));

/// @todo make it an updatable SESSION_VAR
static Sys_var_ulong Sys_ft_query_expansion_limit(
       "ft_query_expansion_limit",
       "Number of best matches to use for query expansion",
       READ_ONLY GLOBAL_VAR(ft_query_expansion_limit),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 1000), DEFAULT(20), BLOCK_SIZE(1));

static Sys_var_charptr Sys_ft_stopword_file(
       "ft_stopword_file",
       "Use stopwords from this file instead of built-in list",
       READ_ONLY GLOBAL_VAR(ft_stopword_file), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_mybool Sys_ignore_builtin_innodb(
       "ignore_builtin_innodb",
       "IGNORED. This option will be removed in future releases. "
       "Disable initialization of builtin InnoDB plugin",
       READ_ONLY GLOBAL_VAR(opt_ignore_builtin_innodb),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static bool check_init_string(sys_var *self, THD *thd, set_var *var)
{
  if (var->save_result.string_value.str == 0)
  {
    var->save_result.string_value.str= const_cast<char*>("");
    var->save_result.string_value.length= 0;
  }
  return false;
}
static PolyLock_rwlock PLock_sys_init_connect(&LOCK_sys_init_connect);
static Sys_var_lexstring Sys_init_connect(
       "init_connect", "Command(s) that are executed for each "
       "new connection", GLOBAL_VAR(opt_init_connect),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(""), &PLock_sys_init_connect, NOT_IN_BINLOG,
       ON_CHECK(check_init_string));

static Sys_var_charptr Sys_init_file(
       "init_file", "Read SQL commands from this file at startup",
       READ_ONLY GLOBAL_VAR(opt_init_file),
       CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static PolyLock_rwlock PLock_sys_init_slave(&LOCK_sys_init_slave);
static Sys_var_lexstring Sys_init_slave(
       "init_slave", "Command(s) that are executed by a slave server "
       "each time the SQL thread starts", GLOBAL_VAR(opt_init_slave),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(""), &PLock_sys_init_slave,
       NOT_IN_BINLOG, ON_CHECK(check_init_string));

static Sys_var_ulong Sys_interactive_timeout(
       "interactive_timeout",
       "The number of seconds the server waits for activity on an interactive "
       "connection before closing it",
       SESSION_VAR(net_interactive_timeout),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(NET_WAIT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_ulong Sys_join_buffer_size(
       "join_buffer_size",
       "The size of the buffer that is used for full joins",
       SESSION_VAR(join_buff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(128, ULONG_MAX), DEFAULT(256 * 1024), BLOCK_SIZE(128));

static Sys_var_keycache Sys_key_buffer_size(
       "key_buffer_size", "The size of the buffer used for "
       "index blocks for MyISAM tables. Increase this to get better index "
       "handling (for all reads and multiple writes) to as much as you can "
       "afford",
       KEYCACHE_VAR(param_buff_size),
       CMD_LINE(REQUIRED_ARG, OPT_KEY_BUFFER_SIZE),
       VALID_RANGE(0, SIZE_T_MAX), DEFAULT(KEY_CACHE_SIZE),
       BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_buffer_size));

static Sys_var_keycache Sys_key_cache_block_size(
       "key_cache_block_size", "The default size of key cache blocks",
       KEYCACHE_VAR(param_block_size),
       CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_BLOCK_SIZE),
       VALID_RANGE(512, 1024*16), DEFAULT(KEY_CACHE_BLOCK_SIZE),
       BLOCK_SIZE(512), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_keycache_param));

static Sys_var_keycache Sys_key_cache_division_limit(
       "key_cache_division_limit",
       "The minimum percentage of warm blocks in key cache",
       KEYCACHE_VAR(param_division_limit),
       CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_DIVISION_LIMIT),
       VALID_RANGE(1, 100), DEFAULT(100),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_keycache_param));

static Sys_var_keycache Sys_key_cache_age_threshold(
       "key_cache_age_threshold", "This characterizes the number of "
       "hits a hot block has to be untouched until it is considered aged "
       "enough to be downgraded to a warm block. This specifies the "
       "percentage ratio of that number of hits to the total number of "
       "blocks in key cache",
       KEYCACHE_VAR(param_age_threshold),
       CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_AGE_THRESHOLD),
       VALID_RANGE(100, ULONG_MAX), DEFAULT(300),
       BLOCK_SIZE(100), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_keycache_param));

static Sys_var_mybool Sys_large_files_support(
       "large_files_support",
       "Whether mysqld was compiled with options for large file support",
       READ_ONLY GLOBAL_VAR(opt_large_files),
       NO_CMD_LINE, DEFAULT(sizeof(my_off_t) > 4));

static Sys_var_uint Sys_large_page_size(
       "large_page_size",
       "If large page support is enabled, this shows the size of memory pages",
       READ_ONLY GLOBAL_VAR(opt_large_page_size), NO_CMD_LINE,
       VALID_RANGE(0, UINT_MAX), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_mybool Sys_large_pages(
       "large_pages", "Enable support for large pages",
       READ_ONLY GLOBAL_VAR(opt_large_pages),
       IF_WIN(NO_CMD_LINE, CMD_LINE(OPT_ARG)), DEFAULT(FALSE));

static Sys_var_charptr Sys_language(
       "lc_messages_dir", "Directory where error messages are",
       READ_ONLY GLOBAL_VAR(lc_messages_dir_ptr), 
       CMD_LINE(REQUIRED_ARG, OPT_LC_MESSAGES_DIRECTORY),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_mybool Sys_local_infile(
       "local_infile", "Enable LOAD DATA LOCAL INFILE",
       GLOBAL_VAR(opt_local_infile), CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static Sys_var_ulong Sys_lock_wait_timeout(
       "lock_wait_timeout",
       "Timeout in seconds to wait for a lock before returning an error.",
       SESSION_VAR(lock_wait_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(LONG_TIMEOUT), BLOCK_SIZE(1));

#ifdef HAVE_MLOCKALL
static Sys_var_mybool Sys_locked_in_memory(
       "locked_in_memory",
       "Whether mysqld was locked in memory with --memlock",
       READ_ONLY GLOBAL_VAR(locked_in_memory), NO_CMD_LINE, DEFAULT(FALSE));
#endif

/* this says NO_CMD_LINE, as command-line option takes a string, not a bool */
static Sys_var_mybool Sys_log_bin(
       "log_bin", "Whether the binary log is enabled",
       READ_ONLY GLOBAL_VAR(opt_bin_log), NO_CMD_LINE, DEFAULT(FALSE));

static bool transaction_write_set_check(sys_var *self, THD *thd, set_var *var)
{
#ifdef HAVE_REPLICATION
  // Can't change the algorithm when group replication is enabled.
  if (is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The write set algorithm cannot be changed when Group replication"
               " is running.", MYF(0));
    return true;
  }
#endif

  if (var->type == OPT_GLOBAL &&
      global_system_variables.binlog_format != BINLOG_FORMAT_ROW)
  {
    my_error(ER_PREVENTS_VARIABLE_WITHOUT_RBR, MYF(0), var->var->name.str);
    return true;
  }

  if (var->type == OPT_SESSION &&
      thd->variables.binlog_format != BINLOG_FORMAT_ROW)
  {
    my_error(ER_PREVENTS_VARIABLE_WITHOUT_RBR, MYF(0), var->var->name.str);
    return true;
  }
  /*
    if in a stored function/trigger, it's too late to change
  */
  if (thd->in_sub_stmt)
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0),
             var->var->name.str);
    return true;
  }
  /*
    Make the session variable 'transaction_write_set_extraction' read-only inside a transaction.
  */
  if (thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0),
             var->var->name.str);
    return true;
  }
  /*
    Disallow changing variable 'transaction_write_set_extraction' while
    binlog_transaction_dependency_tracking is different from COMMIT_ORDER.
  */
  if (mysql_bin_log.m_dependency_tracker.m_opt_tracking_mode
      != DEPENDENCY_TRACKING_COMMIT_ORDER)
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "transaction_write_set_extraction (changed)",
             "binlog_transaction_dependency_tracking (!= COMMIT_ORDER)");
    return true;
  }
  return false;
}

static Sys_var_enum Sys_extract_write_set(
       "transaction_write_set_extraction",
       "This option is used to let the server know when to "
       "extract the write set which will be used for various purposes. ",
       SESSION_VAR(transaction_write_set_extraction), CMD_LINE(OPT_ARG),
       transaction_write_set_hashing_algorithms,
       DEFAULT(HASH_ALGORITHM_OFF), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(transaction_write_set_check),
       ON_UPDATE(NULL));

static Sys_var_ulong Sys_rpl_stop_slave_timeout(
       "rpl_stop_slave_timeout",
       "Timeout in seconds to wait for slave to stop before returning a "
       "warning.",
       GLOBAL_VAR(rpl_stop_slave_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(2, LONG_TIMEOUT), DEFAULT(LONG_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_enum Sys_binlog_error_action(
       "binlog_error_action",
       "When statements cannot be written to the binary log due to a fatal "
       "error, the server can either ignore the error and let the master "
       "continue, or abort.", GLOBAL_VAR(binlog_error_action),
       CMD_LINE(REQUIRED_ARG), binlog_error_action_list, DEFAULT(ABORT_SERVER));

static Sys_var_mybool Sys_trust_function_creators(
       "log_bin_trust_function_creators",
       "If set to FALSE (the default), then when --log-bin is used, creation "
       "of a stored function (or trigger) is allowed only to users having the "
       "SUPER privilege and only if this stored function (trigger) may not "
       "break binary logging. Note that if ALL connections to this server "
       "ALWAYS use row-based binary logging, the security issues do not "
       "exist and the binary logging cannot break, so you can safely set "
       "this to TRUE",
       GLOBAL_VAR(trust_function_creators),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_check_proxy_users(
	"check_proxy_users",
	"If set to FALSE (the default), then proxy user identity will not be "
	"mapped for authentication plugins which support mapping from grant "
	"tables.  When set to TRUE, users associated with authentication "
	"plugins which signal proxy user mapping should be done according to "
	"GRANT PROXY privilege definition.",
	GLOBAL_VAR(check_proxy_users),
	CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_mysql_native_password_proxy_users(
	"mysql_native_password_proxy_users",
	"If set to FALSE (the default), then the mysql_native_password "
	"plugin will not signal for authenticated users to be checked for mapping "
	"to proxy users.  When set to TRUE, the plugin will flag associated "
	"authenticated accounts to be mapped to proxy users when the server option "
	"check_proxy_users is enabled.",
	GLOBAL_VAR(mysql_native_password_proxy_users),
	CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_sha256_password_proxy_users(
	"sha256_password_proxy_users",
	"If set to FALSE (the default), then the sha256_password authentication "
	"plugin will not signal for authenticated users to be checked for mapping "
	"to proxy users.  When set to TRUE, the plugin will flag associated "
	"authenticated accounts to be mapped to proxy users when the server option "
	"check_proxy_users is enabled.",
	GLOBAL_VAR(sha256_password_proxy_users),
	CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_use_v1_row_events(
       "log_bin_use_v1_row_events",
       "If equal to 1 then version 1 row events are written to a row based "
       "binary log.  If equal to 0, then the latest version of events are "
       "written.  "
       "This option is useful during some upgrades.",
       GLOBAL_VAR(log_bin_use_v1_row_events),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_charptr Sys_log_error(
       "log_error", "Error log file",
       READ_ONLY GLOBAL_VAR(log_error_dest),
       CMD_LINE(OPT_ARG, OPT_LOG_ERROR),
       IN_FS_CHARSET, DEFAULT(disabled_my_option), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL),
       NULL, sys_var::PARSE_EARLY);

static Sys_var_mybool Sys_log_queries_not_using_indexes(
       "log_queries_not_using_indexes",
       "Log queries that are executed without benefit of any index to the "
       "slow log if it is open",
       GLOBAL_VAR(opt_log_queries_not_using_indexes),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_log_slow_admin_statements(
       "log_slow_admin_statements",
       "Log slow OPTIMIZE, ANALYZE, ALTER and other administrative statements to "
       "the slow log if it is open.",
       GLOBAL_VAR(opt_log_slow_admin_statements),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_log_slow_slave_statements(
       "log_slow_slave_statements",
       "Log slow statements executed by slave thread to the slow log if it is open.",
       GLOBAL_VAR(opt_log_slow_slave_statements),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static bool update_log_throttle_queries_not_using_indexes(sys_var *self,
                                                          THD *thd,
                                                          enum_var_type type)
{
  // Check if we should print a summary of any suppressed lines to the slow log
  // now since opt_log_throttle_queries_not_using_indexes was changed.
  log_throttle_qni.flush(thd);
  return false;
}

static Sys_var_ulong Sys_log_throttle_queries_not_using_indexes(
       "log_throttle_queries_not_using_indexes",
       "Log at most this many 'not using index' warnings per minute to the "
       "slow log. Any further warnings will be condensed into a single "
       "summary line. A value of 0 disables throttling. "
       "Option has no effect unless --log_queries_not_using_indexes is set.",
       GLOBAL_VAR(opt_log_throttle_queries_not_using_indexes),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0),
       ON_UPDATE(update_log_throttle_queries_not_using_indexes));

static bool update_log_warnings(sys_var *self, THD *thd, enum_var_type type)
{
  // log_warnings is deprecated, but for now, we'll set the
  // new log_error_verbosity from it for backward compatibility.
  log_error_verbosity= std::min(3UL, 1UL + log_warnings);
  return false;
}

static Sys_var_ulong Sys_log_warnings(
       "log_warnings",
       "Log some not critical warnings to the log file",
       GLOBAL_VAR(log_warnings),
       CMD_LINE(OPT_ARG, 'W'),
       VALID_RANGE(0, 2), DEFAULT(2), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(update_log_warnings),
       DEPRECATED("log_error_verbosity"));

static bool update_log_error_verbosity(sys_var *self, THD *thd,
                                       enum_var_type type)
{
  // log_warnings is deprecated, but for now, we'll set it from
  // the new log_error_verbosity for backward compatibility.
  log_warnings= log_error_verbosity - 1;
  return false;
}

static Sys_var_ulong Sys_log_error_verbosity(
       "log_error_verbosity",
       "How detailed the error log should be. "
       "1, log errors only. "
       "2, log errors and warnings. "
       "3, log errors, warnings, and notes. "
       "Messages sent to the client are unaffected by this setting.",
       GLOBAL_VAR(log_error_verbosity),
       CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, 3), DEFAULT(3), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(update_log_error_verbosity));

static Sys_var_enum Sys_log_timestamps(
       "log_timestamps",
       "UTC to timestamp log files in zulu time, for more concise timestamps "
       "and easier correlation of logs from servers from multiple time zones, "
       "or SYSTEM to use the system's local time. "
       "This affects only log files, not log tables, as the timestamp columns "
       "of the latter can be converted at will.",
       GLOBAL_VAR(opt_log_timestamps),
       CMD_LINE(REQUIRED_ARG),
       timestamp_type_names, DEFAULT(0),
       NO_MUTEX_GUARD, NOT_IN_BINLOG);

static Sys_var_mybool Sys_log_statements_unsafe_for_binlog(
       "log_statements_unsafe_for_binlog",
       "Log statements considered unsafe when using statement based binary logging.",
       GLOBAL_VAR(opt_log_unsafe_statements),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE));

/* logging to host OS's syslog */

static bool fix_syslog(sys_var *self, THD *thd, enum_var_type type)
{
  return log_syslog_update_settings();
}

static bool check_syslog_tag(sys_var *self, THD *THD, set_var *var)
{
  return ((var->save_result.string_value.str != NULL) &&
          (strchr(var->save_result.string_value.str, FN_LIBCHAR) != NULL));
}

static Sys_var_mybool Sys_log_syslog_enable(
       "log_syslog",
       "Errors, warnings, and similar issues eligible for MySQL's error log "
       "file may additionally be sent to the host operating system's system "
       "log (\"syslog\").",
       GLOBAL_VAR(opt_log_syslog_enable),
       CMD_LINE(OPT_ARG),
       // preserve current defaults for both platforms:
#ifndef _WIN32
       DEFAULT(FALSE),
#else
       DEFAULT(TRUE),
#endif
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0), ON_UPDATE(fix_syslog));


static Sys_var_charptr Sys_log_syslog_tag(
       "log_syslog_tag",
       "When logging issues using the host operating system's syslog, "
       "tag the entries from this particular MySQL server with this ident. "
       "This will help distinguish entries from MySQL servers co-existing "
       "on the same host machine. A non-empty tag will be appended to the "
       "default ident of 'mysqld', connected by a hyphen.",
       GLOBAL_VAR(opt_log_syslog_tag), CMD_LINE(REQUIRED_ARG),
       IN_SYSTEM_CHARSET, DEFAULT(""), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_syslog_tag), ON_UPDATE(fix_syslog));


#ifndef _WIN32

static bool check_syslog_facility(sys_var *self, THD *THD, set_var *var)
{
  SYSLOG_FACILITY rsf;

  if (var->value &&
      log_syslog_find_facility(var->save_result.string_value.str, &rsf))
    return true;
  return false;
}

static bool fix_syslog_facility(sys_var *self, THD *thd, enum_var_type type)
{
  if (opt_log_syslog_facility == NULL)
    return true;

  return log_syslog_update_settings();
}

static Sys_var_charptr Sys_log_syslog_facility(
       "log_syslog_facility",
       "When logging issues using the host operating system's syslog, "
       "identify as a facility of the given type (to aid in log filtering).",
       GLOBAL_VAR(opt_log_syslog_facility), CMD_LINE(REQUIRED_ARG),
       IN_SYSTEM_CHARSET, DEFAULT("daemon"), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_syslog_facility), ON_UPDATE(fix_syslog_facility));

static Sys_var_mybool Sys_log_syslog_log_pid(
       "log_syslog_include_pid",
       "When logging issues using the host operating system's syslog, "
       "include this MySQL server's process ID (PID). This setting does "
       "not affect MySQL's own error log file.",
       GLOBAL_VAR(opt_log_syslog_include_pid),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0), ON_UPDATE(fix_syslog));

#endif

static bool update_cached_long_query_time(sys_var *self, THD *thd,
                                          enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->variables.long_query_time=
      double2ulonglong(thd->variables.long_query_time_double * 1e6);
  else
    global_system_variables.long_query_time=
      double2ulonglong(global_system_variables.long_query_time_double * 1e6);
  return false;
}

static Sys_var_double Sys_long_query_time(
       "long_query_time",
       "Log all queries that have taken more than long_query_time seconds "
       "to execute to file. The argument will be treated as a decimal value "
       "with microsecond precision",
       SESSION_VAR(long_query_time_double),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(10),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_cached_long_query_time));

static bool fix_low_prio_updates(sys_var *self, THD *thd, enum_var_type type)
{
  if (type == OPT_SESSION)
  {
    thd->update_lock_default= (thd->variables.low_priority_updates ?
                               TL_WRITE_LOW_PRIORITY : TL_WRITE);
    thd->insert_lock_default= (thd->variables.low_priority_updates ?
                               TL_WRITE_LOW_PRIORITY : TL_WRITE_CONCURRENT_INSERT);
  }
  else
    thr_upgraded_concurrent_insert_lock=
      (global_system_variables.low_priority_updates ?
       TL_WRITE_LOW_PRIORITY : TL_WRITE);
  return false;
}
static Sys_var_mybool Sys_low_priority_updates(
       "low_priority_updates",
       "INSERT/DELETE/UPDATE has lower priority than selects",
       SESSION_VAR(low_priority_updates),
       CMD_LINE(OPT_ARG),
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_low_prio_updates));

static Sys_var_mybool Sys_lower_case_file_system(
       "lower_case_file_system",
       "Case sensitivity of file names on the file system where the "
       "data directory is located",
       READ_ONLY GLOBAL_VAR(lower_case_file_system), NO_CMD_LINE,
       DEFAULT(FALSE));

static Sys_var_uint Sys_lower_case_table_names(
       "lower_case_table_names",
       "If set to 1 table names are stored in lowercase on disk and table "
       "names will be case-insensitive.  Should be set to 2 if you are using "
       "a case insensitive file system",
       READ_ONLY GLOBAL_VAR(lower_case_table_names),
       CMD_LINE(OPT_ARG, OPT_LOWER_CASE_TABLE_NAMES),
       VALID_RANGE(0, 2),
#ifdef FN_NO_CASE_SENSE
    DEFAULT(1),
#else
    DEFAULT(0),
#endif
       BLOCK_SIZE(1));

static bool session_readonly(sys_var *self, THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    return false;
  my_error(ER_VARIABLE_IS_READONLY, MYF(0), "SESSION",
           self->name.str, "GLOBAL");
  return true;
}

static bool
check_max_allowed_packet(sys_var *self, THD *thd,  set_var *var)
{
  longlong val;
  if (session_readonly(self, thd, var))
    return true;

  val= var->save_result.ulonglong_value;
  if (val < (longlong) global_system_variables.net_buffer_length)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        WARN_OPTION_BELOW_LIMIT, ER(WARN_OPTION_BELOW_LIMIT),
                        "max_allowed_packet", "net_buffer_length");
  }
  return false;
}


static Sys_var_ulong Sys_max_allowed_packet(
       "max_allowed_packet",
       "Max packet length to send to or receive from the server",
       SESSION_VAR(max_allowed_packet), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, 1024 * 1024 * 1024), DEFAULT(4096 * 1024),
       BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_max_allowed_packet));

static Sys_var_ulong Sys_slave_max_allowed_packet(
       "slave_max_allowed_packet",
       "The maximum packet length to sent successfully from the master to slave.",
       GLOBAL_VAR(slave_max_allowed_packet), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, MAX_MAX_ALLOWED_PACKET),
       DEFAULT(MAX_MAX_ALLOWED_PACKET),
       BLOCK_SIZE(1024));

static Sys_var_ulonglong Sys_max_binlog_cache_size(
       "max_binlog_cache_size",
       "Sets the total size of the transactional cache",
       GLOBAL_VAR(max_binlog_cache_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE, ULLONG_MAX),
       DEFAULT((ULLONG_MAX/IO_SIZE)*IO_SIZE),
       BLOCK_SIZE(IO_SIZE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_binlog_cache_size));

static Sys_var_ulonglong Sys_max_binlog_stmt_cache_size(
       "max_binlog_stmt_cache_size",
       "Sets the total size of the statement cache",
       GLOBAL_VAR(max_binlog_stmt_cache_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE, ULLONG_MAX),
       DEFAULT((ULLONG_MAX/IO_SIZE)*IO_SIZE),
       BLOCK_SIZE(IO_SIZE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_binlog_stmt_cache_size));

static bool fix_max_binlog_size(sys_var *self, THD *thd, enum_var_type type)
{
  mysql_bin_log.set_max_size(max_binlog_size);
#ifdef HAVE_REPLICATION
  /*
    For multisource replication, this max size is set to all relay logs
    per channel. So, run through them
  */
  if (!max_relay_log_size)
  {
    Master_info *mi =NULL;

    channel_map.wrlock();
    for (mi_map::iterator it= channel_map.begin(); it!= channel_map.end(); it++)
    {
      mi= it->second;
      if (mi!= NULL)
        mi->rli->relay_log.set_max_size(max_binlog_size);
    }
    channel_map.unlock();
  }
#endif
  return false;
}
static Sys_var_ulong Sys_max_binlog_size(
       "max_binlog_size",
       "Binary log will be rotated automatically when the size exceeds this "
       "value. Will also apply to relay logs if max_relay_log_size is 0",
       GLOBAL_VAR(max_binlog_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE, 1024*1024L*1024L), DEFAULT(1024*1024L*1024L),
       BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_max_binlog_size));

static Sys_var_ulong Sys_max_connections(
       "max_connections", "The number of simultaneous clients allowed",
       GLOBAL_VAR(max_connections), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, 100000),
       DEFAULT(MAX_CONNECTIONS_DEFAULT),
       BLOCK_SIZE(1),
       NO_MUTEX_GUARD,
       NOT_IN_BINLOG,
       ON_CHECK(0),
       ON_UPDATE(0),
       NULL,
       /* max_connections is used as a sizing hint by the performance schema. */
       sys_var::PARSE_EARLY);

static Sys_var_ulong Sys_max_connect_errors(
       "max_connect_errors",
       "If there is more than this number of interrupted connections from "
       "a host this host will be blocked from further connections",
       GLOBAL_VAR(max_connect_errors), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(100),
       BLOCK_SIZE(1));

static Sys_var_long Sys_max_digest_length(
       "max_digest_length",
       "Maximum length considered for digest text.",
       READ_ONLY GLOBAL_VAR(max_digest_length),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024 * 1024),
       DEFAULT(1024),
       BLOCK_SIZE(1));

static bool check_max_delayed_threads(sys_var *self, THD *thd, set_var *var)
{
  return var->type != OPT_GLOBAL &&
         var->save_result.ulonglong_value != 0 &&
         var->save_result.ulonglong_value !=
                           global_system_variables.max_insert_delayed_threads;
}

// Alias for max_delayed_threads
static Sys_var_ulong Sys_max_insert_delayed_threads(
       "max_insert_delayed_threads",
       "Don't start more than this number of threads to handle INSERT "
       "DELAYED statements. If set to zero INSERT DELAYED will be not used. "
       "This variable is deprecated along with INSERT DELAYED.",
       SESSION_VAR(max_insert_delayed_threads),
       NO_CMD_LINE, VALID_RANGE(0, 16384), DEFAULT(20),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_max_delayed_threads), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_ulong Sys_max_delayed_threads(
       "max_delayed_threads",
       "Don't start more than this number of threads to handle INSERT "
       "DELAYED statements. If set to zero INSERT DELAYED will be not used. "
       "This variable is deprecated along with INSERT DELAYED.",
       SESSION_VAR(max_insert_delayed_threads),
       CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 16384), DEFAULT(20),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_max_delayed_threads), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_ulong Sys_max_error_count(
       "max_error_count",
       "Max number of errors/warnings to store for a statement",
       SESSION_VAR(max_error_count), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 65535), DEFAULT(DEFAULT_ERROR_COUNT), BLOCK_SIZE(1));

static Sys_var_ulonglong Sys_max_heap_table_size(
       "max_heap_table_size",
       "Don't allow creation of heap tables bigger than this",
       SESSION_VAR(max_heap_table_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(16384, (ulonglong)~(intptr)0), DEFAULT(16*1024*1024),
       BLOCK_SIZE(1024));

static ulong mdl_locks_cache_size_unused;
static Sys_var_ulong Sys_metadata_locks_cache_size(
       "metadata_locks_cache_size", "Has no effect, deprecated",
       READ_ONLY GLOBAL_VAR(mdl_locks_cache_size_unused),
       CMD_LINE(REQUIRED_ARG, OPT_MDL_CACHE_SIZE),
       VALID_RANGE(1, 1024*1024), DEFAULT(1024), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));

static ulong mdl_locks_hash_partitions_unused;
static Sys_var_ulong Sys_metadata_locks_hash_instances(
       "metadata_locks_hash_instances", "Has no effect, deprecated",
       READ_ONLY GLOBAL_VAR(mdl_locks_hash_partitions_unused),
       CMD_LINE(REQUIRED_ARG, OPT_MDL_HASH_INSTANCES),
       VALID_RANGE(1, 1024), DEFAULT(8), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));

// relies on DBUG_ASSERT(sizeof(my_thread_id) == 4);
static Sys_var_uint Sys_pseudo_thread_id(
       "pseudo_thread_id",
       "This variable is for internal server use",
       SESSION_ONLY(pseudo_thread_id),
       NO_CMD_LINE, VALID_RANGE(0, UINT_MAX32), DEFAULT(0),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG,
       ON_CHECK(check_has_super));

static bool fix_max_join_size(sys_var *self, THD *thd, enum_var_type type)
{
  SV *sv= type == OPT_GLOBAL ? &global_system_variables : &thd->variables;
  if (sv->max_join_size == HA_POS_ERROR)
    sv->option_bits|= OPTION_BIG_SELECTS;
  else
    sv->option_bits&= ~OPTION_BIG_SELECTS;
  return false;
}
static Sys_var_harows Sys_max_join_size(
       "max_join_size",
       "Joins that are probably going to read more than max_join_size "
       "records return an error",
       SESSION_VAR(max_join_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, HA_POS_ERROR), DEFAULT(HA_POS_ERROR), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_max_join_size));

static Sys_var_ulong Sys_max_seeks_for_key(
       "max_seeks_for_key",
       "Limit assumed max number of seeks when looking up rows based on a key",
       SESSION_VAR(max_seeks_for_key), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(ULONG_MAX), BLOCK_SIZE(1));

static Sys_var_ulong Sys_max_length_for_sort_data(
       "max_length_for_sort_data",
       "Max number of bytes in sorted records",
       SESSION_VAR(max_length_for_sort_data), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(4, 8192*1024L), DEFAULT(1024), BLOCK_SIZE(1));

static Sys_var_ulong Sys_max_points_in_geometry(
       "max_points_in_geometry",
       "Maximum number of points in a geometry",
       SESSION_VAR(max_points_in_geometry), CMD_LINE(OPT_ARG),
       VALID_RANGE(3, 1024*1024L), DEFAULT(64*1024), BLOCK_SIZE(1));

static PolyLock_mutex PLock_prepared_stmt_count(&LOCK_prepared_stmt_count);

static Sys_var_ulong Sys_max_prepared_stmt_count(
       "max_prepared_stmt_count",
       "Maximum number of prepared statements in the server",
       GLOBAL_VAR(max_prepared_stmt_count), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 1024*1024), DEFAULT(16382), BLOCK_SIZE(1),
       &PLock_prepared_stmt_count, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(NULL), NULL,
       /* max_prepared_stmt_count is used as a sizing hint by the performance schema. */
       sys_var::PARSE_EARLY);

static bool fix_max_relay_log_size(sys_var *self, THD *thd, enum_var_type type)
{
#ifdef HAVE_REPLICATION
  Master_info *mi= NULL;

  channel_map.wrlock();
  for (mi_map::iterator it= channel_map.begin(); it!=channel_map.end(); it++)
  {
    mi= it->second;

    if (mi != NULL)
      mi->rli->relay_log.set_max_size(max_relay_log_size ?
                                      max_relay_log_size: max_binlog_size);
  }
  channel_map.unlock();
#endif
  return false;
}
static Sys_var_ulong Sys_max_relay_log_size(
       "max_relay_log_size",
       "If non-zero: relay log will be rotated automatically when the "
       "size exceeds this value; if zero: when the size "
       "exceeds max_binlog_size",
       GLOBAL_VAR(max_relay_log_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 1024L*1024*1024), DEFAULT(0), BLOCK_SIZE(IO_SIZE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_max_relay_log_size));

static Sys_var_ulong Sys_max_sort_length(
       "max_sort_length",
       "The number of bytes to use when sorting BLOB or TEXT values (only "
       "the first max_sort_length bytes of each value are used; the rest "
       "are ignored)",
       SESSION_VAR(max_sort_length), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(4, 8192*1024L), DEFAULT(1024), BLOCK_SIZE(1));

static Sys_var_ulong Sys_max_sp_recursion_depth(
       "max_sp_recursion_depth",
       "Maximum stored procedure recursion depth",
       SESSION_VAR(max_sp_recursion_depth), CMD_LINE(OPT_ARG),
       VALID_RANGE(0, 255), DEFAULT(0), BLOCK_SIZE(1));

// non-standard session_value_ptr() here
static Sys_var_max_user_conn Sys_max_user_connections(
       "max_user_connections",
       "The maximum number of active connections for a single user "
       "(0 = no limit)",
       SESSION_VAR(max_user_connections), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(session_readonly));

static Sys_var_ulong Sys_max_tmp_tables(
       "max_tmp_tables",
       "Maximum number of temporary tables a client can keep open at a time",
       SESSION_VAR(max_tmp_tables), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(32), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));

static Sys_var_ulong Sys_max_write_lock_count(
       "max_write_lock_count",
       "After this many write locks, allow some read locks to run in between",
       GLOBAL_VAR(max_write_lock_count), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(ULONG_MAX), BLOCK_SIZE(1));

static Sys_var_ulong Sys_min_examined_row_limit(
       "min_examined_row_limit",
       "Don't write queries to slow log that examine fewer rows "
       "than that",
       SESSION_VAR(min_examined_row_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1));

#ifdef _WIN32
static Sys_var_mybool Sys_named_pipe(
       "named_pipe", "Enable the named pipe (NT)",
       READ_ONLY GLOBAL_VAR(opt_enable_named_pipe), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));
#endif


static bool 
check_net_buffer_length(sys_var *self, THD *thd,  set_var *var)
{
  longlong val;
  if (session_readonly(self, thd, var))
    return true;

  val= var->save_result.ulonglong_value;
  if (val > (longlong) global_system_variables.max_allowed_packet)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        WARN_OPTION_BELOW_LIMIT, ER(WARN_OPTION_BELOW_LIMIT),
                        "max_allowed_packet", "net_buffer_length");
  }
  return false;
}
static Sys_var_ulong Sys_net_buffer_length(
       "net_buffer_length",
       "Buffer length for TCP/IP and socket communication",
       SESSION_VAR(net_buffer_length), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, 1024*1024), DEFAULT(16384), BLOCK_SIZE(1024),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_net_buffer_length));

static bool fix_net_read_timeout(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    // net_buffer_length is a specific property for the classic protocols
    if (!thd->is_classic_protocol())
    {
      my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
      return true;
    }
    my_net_set_read_timeout(thd->get_protocol_classic()->get_net(),
                            thd->variables.net_read_timeout);
  }
  return false;
}
static Sys_var_ulong Sys_net_read_timeout(
       "net_read_timeout",
       "Number of seconds to wait for more data from a connection before "
       "aborting the read",
       SESSION_VAR(net_read_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(NET_READ_TIMEOUT), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_net_read_timeout));

static bool fix_net_write_timeout(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    // net_read_timeout is a specific property for the classic protocols
    if (!thd->is_classic_protocol())
    {
      my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
      return true;
    }
    my_net_set_write_timeout(thd->get_protocol_classic()->get_net(),
                             thd->variables.net_write_timeout);
  }
  return false;
}
static Sys_var_ulong Sys_net_write_timeout(
       "net_write_timeout",
       "Number of seconds to wait for a block to be written to a connection "
       "before aborting the write",
       SESSION_VAR(net_write_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(NET_WRITE_TIMEOUT), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_net_write_timeout));

static bool fix_net_retry_count(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    // net_write_timeout is a specific property for the classic protocols
    if (!thd->is_classic_protocol())
    {
      my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
      return true;
    }
    thd->get_protocol_classic()->get_net()->retry_count=
      thd->variables.net_retry_count;
  }
  return false;
}
static Sys_var_ulong Sys_net_retry_count(
       "net_retry_count",
       "If a read on a communication port is interrupted, retry this "
       "many times before giving up",
       SESSION_VAR(net_retry_count), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(MYSQLD_NET_RETRY_COUNT),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_net_retry_count));

static Sys_var_mybool Sys_new_mode(
       "new", "Use very new possible \"unsafe\" functions",
       SESSION_VAR(new_mode), CMD_LINE(OPT_ARG, 'n'), DEFAULT(FALSE));

static Sys_var_mybool Sys_old_mode(
       "old", "Use compatible behavior",
       READ_ONLY GLOBAL_VAR(old_mode), CMD_LINE(OPT_ARG), DEFAULT(FALSE));

#ifndef EMBEDDED_LIBRARY
static Sys_var_mybool Sys_show_compatibility_56(
       "show_compatibility_56",
       "SHOW commands / INFORMATION_SCHEMA tables compatible with MySQL 5.6",
       GLOBAL_VAR(show_compatibility_56), CMD_LINE(OPT_ARG), DEFAULT(FALSE));
#endif /* EMBEDDED_LIBRARY */

static Sys_var_mybool Sys_old_alter_table(
       "old_alter_table", "Use old, non-optimized alter table",
       SESSION_VAR(old_alter_table), CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static bool old_passwords_check(sys_var *self  MY_ATTRIBUTE((unused)),
                                THD *thd  MY_ATTRIBUTE((unused)),
                                set_var *var)
{
  push_deprecated_warn_no_replacement(current_thd, "old_passwords");
  /* 1 used to be old passwords */
  return var->save_result.ulonglong_value == 1;
}

static Sys_var_uint Sys_old_passwords(
       "old_passwords",
       "Determine which hash algorithm to use when generating passwords using "
       "the PASSWORD() function",
       SESSION_VAR(old_passwords), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 2), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(old_passwords_check));

static Sys_var_ulong Sys_open_files_limit(
       "open_files_limit",
       "If this is not 0, then mysqld will use this value to reserve file "
       "descriptors to use with setrlimit(). If this value is 0 then mysqld "
       "will reserve max_connections*5 or max_connections + table_open_cache*2 "
       "(whichever is larger) number of file descriptors",
       READ_ONLY GLOBAL_VAR(open_files_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, OS_FILE_LIMIT), DEFAULT(0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL),
       NULL,
       /* open_files_limit is used as a sizing hint by the performance schema. */
       sys_var::PARSE_EARLY);

/// @todo change to enum
static Sys_var_ulong Sys_optimizer_prune_level(
       "optimizer_prune_level",
       "Controls the heuristic(s) applied during query optimization to prune "
       "less-promising partial plans from the optimizer search space. "
       "Meaning: 0 - do not apply any heuristic, thus perform exhaustive "
       "search; 1 - prune plans based on number of retrieved rows",
       SESSION_VAR(optimizer_prune_level), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 1), DEFAULT(1), BLOCK_SIZE(1));

static Sys_var_ulong Sys_optimizer_search_depth(
       "optimizer_search_depth",
       "Maximum depth of search performed by the query optimizer. Values "
       "larger than the number of relations in a query result in better "
       "query plans, but take longer to compile a query. Values smaller "
       "than the number of tables in a relation result in faster "
       "optimization, but may produce very bad query plans. If set to 0, "
       "the system will automatically pick a reasonable value",
       SESSION_VAR(optimizer_search_depth), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, MAX_TABLES+1), DEFAULT(MAX_TABLES+1), BLOCK_SIZE(1));

static Sys_var_ulong Sys_range_optimizer_max_mem_size(
      "range_optimizer_max_mem_size",
      "Maximum amount of memory used by the range optimizer "
      "to allocate predicates during range analysis. "
      "The larger the number, more memory may be consumed during "
      "range analysis. If the value is too low to completed range "
      "optimization of a query, index range scan will not be "
      "considered for this query. A value of 0 means range optimizer "
      "does not have any cap on memory. ",
      SESSION_VAR(range_optimizer_max_mem_size),
      CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, ULONG_MAX),
      DEFAULT(8388608),
      BLOCK_SIZE(1));

static bool
limit_parser_max_mem_size(sys_var *self, THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    return false;
  ulonglong val= var->save_result.ulonglong_value;
  if (val > global_system_variables.parser_max_mem_size)
  {
    if (thd->security_context()->check_access(SUPER_ACL))
      return false;
    var->save_result.ulonglong_value=
      global_system_variables.parser_max_mem_size;
    return throw_bounds_warning(thd, "parser_max_mem_size",
                                true, // fixed
                                true, // is_unsigned
                                val);
  }
  return false;
}

// Similar to what we do for the intptr typedef.
#if SIZEOF_CHARP == SIZEOF_INT
static unsigned int max_mem_sz = ~0;
#elif SIZEOF_CHARP == SIZEOF_LONG
static unsigned long max_mem_sz = ~0;
#elif SIZEOF_CHARP == SIZEOF_LONG_LONG
static unsigned long long max_mem_sz = ~0;
#endif

/*
  Need at least 400Kb to get through bootstrap.
  Need at least 8Mb to get through mtr check testcase, which does
    SELECT * FROM INFORMATION_SCHEMA.VIEWS
*/
static Sys_var_ulonglong Sys_parser_max_mem_size(
      "parser_max_mem_size",
      "Maximum amount of memory available to the parser",
      SESSION_VAR(parser_max_mem_size),
      CMD_LINE(REQUIRED_ARG),
      VALID_RANGE(10 * 1000 * 1000, max_mem_sz),
      DEFAULT(max_mem_sz),
      BLOCK_SIZE(1),
      NO_MUTEX_GUARD, NOT_IN_BINLOG,
      ON_CHECK(limit_parser_max_mem_size),
      ON_UPDATE(NULL));

/*
  There is no call on Sys_var_integer::do_check() for 'set xxx=default';
  The predefined default for parser_max_mem_size is "infinite".
  Update it in case we have seen option maximum-parser-max-mem-size
  Also update global_system_variables, so 'SELECT parser_max_mem_size'
  reports correct data.
*/
export void update_parser_max_mem_size()
{
  const ulonglong max_max= max_system_variables.parser_max_mem_size;
  if (max_max == max_mem_sz)
    return;
  // In case parser-max-mem-size is also set:
  const ulonglong new_val=
    std::min(max_max, global_system_variables.parser_max_mem_size);
  Sys_parser_max_mem_size.update_default(new_val);
  global_system_variables.parser_max_mem_size= new_val;
}

static const char *optimizer_switch_names[]=
{
  "index_merge", "index_merge_union", "index_merge_sort_union",
  "index_merge_intersection", "engine_condition_pushdown",
  "index_condition_pushdown" , "mrr", "mrr_cost_based",
  "block_nested_loop", "batched_key_access",
  "materialization", "semijoin", "loosescan", "firstmatch", "duplicateweedout",
  "subquery_materialization_cost_based",
  "use_index_extensions", "condition_fanout_filter", "derived_merge",
  "default", NullS
};
static Sys_var_flagset Sys_optimizer_switch(
       "optimizer_switch",
       "optimizer_switch=option=val[,option=val...], where option is one of "
       "{index_merge, index_merge_union, index_merge_sort_union, "
       "index_merge_intersection, engine_condition_pushdown, "
       "index_condition_pushdown, mrr, mrr_cost_based"
       ", materialization, semijoin, loosescan, firstmatch, duplicateweedout,"
       " subquery_materialization_cost_based"
       ", block_nested_loop, batched_key_access, use_index_extensions,"
       " condition_fanout_filter, derived_merge} and val is one of "
       "{on, off, default}",
       SESSION_VAR(optimizer_switch), CMD_LINE(REQUIRED_ARG),
       optimizer_switch_names, DEFAULT(OPTIMIZER_SWITCH_DEFAULT),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL));

static Sys_var_mybool Sys_var_end_markers_in_json(
       "end_markers_in_json",
       "In JSON output (\"EXPLAIN FORMAT=JSON\" and optimizer trace), "
       "if variable is set to 1, repeats the structure's key (if it has one) "
       "near the closing bracket",
       SESSION_VAR(end_markers_in_json), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

#ifdef OPTIMIZER_TRACE

static Sys_var_flagset Sys_optimizer_trace(
       "optimizer_trace",
       "Controls tracing of the Optimizer:"
       " optimizer_trace=option=val[,option=val...], where option is one of"
       " {enabled, one_line}"
       " and val is one of {on, default}",
       SESSION_VAR(optimizer_trace), CMD_LINE(REQUIRED_ARG),
       Opt_trace_context::flag_names,
       DEFAULT(Opt_trace_context::FLAG_DEFAULT));
// @see set_var::is_var_optimizer_trace()
export sys_var *Sys_optimizer_trace_ptr= &Sys_optimizer_trace;

/**
  Note how "misc" is not here: it is not accessible to the user; disabling
  "misc" would disable the top object, which would make an empty trace.
*/
static Sys_var_flagset Sys_optimizer_trace_features(
       "optimizer_trace_features",
       "Enables/disables tracing of selected features of the Optimizer:"
       " optimizer_trace_features=option=val[,option=val...], where option is one of"
       " {greedy_search, range_optimizer, dynamic_range, repeated_subselect}"
       " and val is one of {on, off, default}",
       SESSION_VAR(optimizer_trace_features), CMD_LINE(REQUIRED_ARG),
       Opt_trace_context::feature_names,
       DEFAULT(Opt_trace_context::default_features));

/** Delete all old optimizer traces */
static bool optimizer_trace_update(sys_var *self, THD *thd,
                                   enum_var_type type)
{
  thd->opt_trace.reset();
  return false;
}

static Sys_var_long Sys_optimizer_trace_offset(
       "optimizer_trace_offset",
       "Offset of first optimizer trace to show; see manual",
       SESSION_VAR(optimizer_trace_offset), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(LONG_MIN, LONG_MAX), DEFAULT(-1), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(optimizer_trace_update));

static Sys_var_long Sys_optimizer_trace_limit(
       "optimizer_trace_limit",
       "Maximum number of shown optimizer traces",
       SESSION_VAR(optimizer_trace_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, LONG_MAX), DEFAULT(1), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(optimizer_trace_update));

static Sys_var_ulong Sys_optimizer_trace_max_mem_size(
       "optimizer_trace_max_mem_size",
       "Maximum allowed cumulated size of stored optimizer traces",
       SESSION_VAR(optimizer_trace_max_mem_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(1024*16), BLOCK_SIZE(1));

#endif

static Sys_var_charptr Sys_pid_file(
       "pid_file", "Pid file used by safe_mysqld",
       READ_ONLY GLOBAL_VAR(pidfile_name_ptr), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_plugin_dir(
       "plugin_dir", "Directory for plugins",
       READ_ONLY GLOBAL_VAR(opt_plugin_dir_ptr), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_uint Sys_port(
       "port",
       "Port number to use for connection or 0 to default to, "
       "my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
       "/etc/services, "
#endif
       "built-in default (" STRINGIFY_ARG(MYSQL_PORT) "), whatever comes first",
       READ_ONLY GLOBAL_VAR(mysqld_port), CMD_LINE(REQUIRED_ARG, 'P'),
       VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_ulong Sys_preload_buff_size(
       "preload_buffer_size",
       "The size of the buffer that is allocated when preloading indexes",
       SESSION_VAR(preload_buff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, 1024*1024*1024), DEFAULT(32768), BLOCK_SIZE(1));

static Sys_var_uint Sys_protocol_version(
       "protocol_version",
       "The version of the client/server protocol used by the MySQL server",
       READ_ONLY GLOBAL_VAR(protocol_version), NO_CMD_LINE,
       VALID_RANGE(0, ~0), DEFAULT(PROTOCOL_VERSION), BLOCK_SIZE(1));

static Sys_var_proxy_user Sys_proxy_user(
       "proxy_user", "The proxy user account name used when logging in",
       IN_SYSTEM_CHARSET);

static Sys_var_external_user Sys_external_user(
       "external_user", "The external user account used when logging in",
       IN_SYSTEM_CHARSET);

static Sys_var_ulong Sys_read_buff_size(
       "read_buffer_size",
       "Each thread that does a sequential scan allocates a buffer of "
       "this size for each table it scans. If you do many sequential scans, "
       "you may want to increase this value",
       SESSION_VAR(read_buff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(IO_SIZE*2, INT_MAX32), DEFAULT(128*1024),
       BLOCK_SIZE(IO_SIZE));

static bool check_read_only(sys_var *self, THD *thd, set_var *var)
{
  /* Prevent self dead-lock */
  if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    return true;
  }
  return false;
}

#if !defined(EMBEDDED_LIBRARY)

static bool check_require_secure_transport(sys_var *self, THD *thd, set_var *var)
{

#if !defined (_WIN32)
  /*
    always allow require_secure_transport to be enabled on
    Linux, as socket is secure.
  */
  return false;
#else
  /*
    check whether SSL or shared memory transports are enabled before
    turning require_secure_transport ON, otherwise no connections will
    be allowed on Windows.
  */

  if (!var->save_result.ulonglong_value)
    return false;
  if ((have_ssl == SHOW_OPTION_YES) || opt_enable_shared_memory)
    return false;
  /* reject if SSL and shared memory are both disabled: */
  my_error(ER_NO_SECURE_TRANSPORTS_CONFIGURED, MYF(0));
  return true;

#endif
}

#endif

static bool fix_read_only(sys_var *self, THD *thd, enum_var_type type)
{
  bool result= true;
  my_bool new_read_only= read_only; // make a copy before releasing a mutex
  DBUG_ENTER("sys_var_opt_readonly::update");

  if (read_only == FALSE || read_only == opt_readonly)
  {
    if (opt_super_readonly && !read_only)
    {
      opt_super_readonly= FALSE;
      super_read_only= FALSE;
    }
    opt_readonly= read_only;
    DBUG_RETURN(false);
  }

  if (check_read_only(self, thd, 0)) // just in case
    goto end;

  if (thd->global_read_lock.is_acquired())
  {
    /*
      This connection already holds the global read lock.
      This can be the case with:
      - FLUSH TABLES WITH READ LOCK
      - SET GLOBAL READ_ONLY = 1
    */
    if (opt_super_readonly && !read_only)
    {
      opt_super_readonly= FALSE;
      super_read_only= FALSE;
    }
    opt_readonly= read_only;
    DBUG_RETURN(false);
  }

  /*
    READ_ONLY=1 prevents write locks from being taken on tables and
    blocks transactions from committing. We therefore should make sure
    that no such events occur while setting the read_only variable.
    This is a 2 step process:
    [1] lock_global_read_lock()
      Prevents connections from obtaining new write locks on
      tables. Note that we can still have active rw transactions.
    [2] make_global_read_lock_block_commit()
      Prevents transactions from committing.
  */

  read_only= opt_readonly;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (thd->global_read_lock.lock_global_read_lock(thd))
    goto end_with_mutex_unlock;

  if ((result= thd->global_read_lock.make_global_read_lock_block_commit(thd)))
    goto end_with_read_lock;

  /* Change the opt_readonly system variable, safe because the lock is held */
  opt_readonly= new_read_only;

  result= false;

 end_with_read_lock:
  /* Release the lock */
  thd->global_read_lock.unlock_global_read_lock(thd);
 end_with_mutex_unlock:
  mysql_mutex_lock(&LOCK_global_system_variables);
 end:
  read_only= opt_readonly;
  DBUG_RETURN(result);
}

static bool fix_super_read_only(sys_var *self, THD *thd, enum_var_type type)
{
  DBUG_ENTER("sys_var_opt_super_readonly::update");

  /* return if no changes: */
  if (super_read_only == opt_super_readonly)
    DBUG_RETURN(false);

  /* return immediately if turning super_read_only OFF: */
  if (super_read_only == FALSE)
  {
    opt_super_readonly= FALSE;
    DBUG_RETURN(false);
  }
  bool result= true;
  my_bool new_super_read_only = super_read_only; /* make a copy before releasing a mutex */

  /* set read_only to ON if it is OFF, letting fix_read_only()
     handle its own locking needs
  */
  if (!opt_readonly)
  {
    read_only= TRUE;
    if ((result = fix_read_only(NULL, thd, type)))
      goto end;
  }

  /* if we already have global read lock, set super_read_only
     and return immediately:
  */
  if (thd->global_read_lock.is_acquired())
  {
    opt_super_readonly= super_read_only;
    DBUG_RETURN(false);
  }

  /* now we're turning ON super_read_only: */
  super_read_only = opt_super_readonly;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (thd->global_read_lock.lock_global_read_lock(thd))
    goto end_with_mutex_unlock;

  if ((result = thd->global_read_lock.make_global_read_lock_block_commit(thd)))
    goto end_with_read_lock;
  opt_super_readonly= new_super_read_only;
  result= false;

  end_with_read_lock:
    /* Release the lock */
    thd->global_read_lock.unlock_global_read_lock(thd);
  end_with_mutex_unlock:
    mysql_mutex_lock(&LOCK_global_system_variables);
  end:
    super_read_only= opt_super_readonly;
    DBUG_RETURN(result);
}

#if !defined(EMBEDDED_LIBRARY)

static Sys_var_mybool Sys_require_secure_transport(
  "require_secure_transport",
  "When this option is enabled, connections attempted using insecure "
  "transport will be rejected.  Secure transports are SSL/TLS, "
  "Unix socket or Shared Memory (on Windows).",
  GLOBAL_VAR(opt_require_secure_transport),
  CMD_LINE(OPT_ARG),
  DEFAULT(FALSE),
  NO_MUTEX_GUARD, NOT_IN_BINLOG,
  ON_CHECK(check_require_secure_transport), ON_UPDATE(0));

#endif

/**
  The read_only boolean is always equal to the opt_readonly boolean except
  during fix_read_only(); when that function is entered, opt_readonly is
  the pre-update value and read_only is the post-update value.
  fix_read_only() compares them and runs needed operations for the
  transition (especially when transitioning from false to true) and
  synchronizes both booleans in the end.
*/
static Sys_var_mybool Sys_readonly(
       "read_only",
       "Make all non-temporary tables read-only, with the exception for "
       "replication (slave) threads and users with the SUPER privilege",
       GLOBAL_VAR(read_only), CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_read_only), ON_UPDATE(fix_read_only));

/**
Setting super_read_only to ON triggers read_only to also be set to ON.
*/
static Sys_var_mybool Sys_super_readonly(
  "super_read_only",
  "Make all non-temporary tables read-only, with the exception for "
  "replication (slave) threads.  Users with the SUPER privilege are "
  "affected, unlike read_only.  Setting super_read_only to ON "
  "also sets read_only to ON.",
  GLOBAL_VAR(super_read_only), CMD_LINE(OPT_ARG), DEFAULT(FALSE),
  NO_MUTEX_GUARD, NOT_IN_BINLOG,
  ON_CHECK(0), ON_UPDATE(fix_super_read_only));



// Small lower limit to be able to test MRR
static Sys_var_ulong Sys_read_rnd_buff_size(
       "read_rnd_buffer_size",
       "When reading rows in sorted order after a sort, the rows are read "
       "through this buffer to avoid a disk seeks",
       SESSION_VAR(read_rnd_buff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, INT_MAX32), DEFAULT(256*1024), BLOCK_SIZE(1));

static Sys_var_ulong Sys_div_precincrement(
       "div_precision_increment", "Precision of the result of '/' "
       "operator will be increased on that value",
       SESSION_VAR(div_precincrement), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, DECIMAL_MAX_SCALE), DEFAULT(4), BLOCK_SIZE(1));

static Sys_var_uint Sys_eq_range_index_dive_limit(
       "eq_range_index_dive_limit",
       "The optimizer will use existing index statistics instead of "
       "doing index dives for equality ranges if the number of equality "
       "ranges for the index is larger than or equal to this number. "
       "If set to 0, index dives are always used.",
       SESSION_VAR(eq_range_index_dive_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX32), DEFAULT(200), BLOCK_SIZE(1));

static Sys_var_ulong Sys_range_alloc_block_size(
       "range_alloc_block_size",
       "Allocation block size for storing ranges during optimization",
       SESSION_VAR(range_alloc_block_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(RANGE_ALLOC_BLOCK_SIZE, UINT32_MAX),
       DEFAULT(RANGE_ALLOC_BLOCK_SIZE), BLOCK_SIZE(1024));

static Sys_var_ulong Sys_multi_range_count(
       "multi_range_count",
       "Number of key ranges to request at once. "
       "This variable has no effect, and is deprecated. "
       "It will be removed in a future release.",
       SESSION_VAR(multi_range_count), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, ULONG_MAX), DEFAULT(256), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static bool fix_thd_mem_root(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(thd->mem_root,
                        thd->variables.query_alloc_block_size,
                        thd->variables.query_prealloc_size);
  return false;
}
static Sys_var_ulong Sys_query_alloc_block_size(
       "query_alloc_block_size",
       "Allocation block size for query parsing and execution",
       SESSION_VAR(query_alloc_block_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, UINT_MAX32), DEFAULT(QUERY_ALLOC_BLOCK_SIZE),
       BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_thd_mem_root));

static Sys_var_ulong Sys_query_prealloc_size(
       "query_prealloc_size",
       "Persistent buffer for query parsing and execution",
       SESSION_VAR(query_prealloc_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(QUERY_ALLOC_PREALLOC_SIZE, ULONG_MAX),
       DEFAULT(QUERY_ALLOC_PREALLOC_SIZE),
       BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_thd_mem_root));

#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
static Sys_var_mybool Sys_shared_memory(
       "shared_memory", "Enable the shared memory",
       READ_ONLY GLOBAL_VAR(opt_enable_shared_memory), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

static Sys_var_charptr Sys_shared_memory_base_name(
       "shared_memory_base_name", "Base name of shared memory",
       READ_ONLY GLOBAL_VAR(shared_memory_base_name), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));
#endif

// this has to be NO_CMD_LINE as the command-line option has a different name
static Sys_var_mybool Sys_skip_external_locking(
       "skip_external_locking", "Don't use system (external) locking",
       READ_ONLY GLOBAL_VAR(my_disable_locking), NO_CMD_LINE, DEFAULT(TRUE));

static Sys_var_mybool Sys_skip_networking(
       "skip_networking", "Don't allow connection with TCP/IP",
       READ_ONLY GLOBAL_VAR(opt_disable_networking), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

static Sys_var_mybool Sys_skip_name_resolve(
       "skip_name_resolve",
       "Don't resolve hostnames. All hostnames are IP's or 'localhost'.",
       READ_ONLY GLOBAL_VAR(opt_skip_name_resolve),
       CMD_LINE(OPT_ARG, OPT_SKIP_RESOLVE),
       DEFAULT(FALSE));

static Sys_var_mybool Sys_skip_show_database(
       "skip_show_database", "Don't allow 'SHOW DATABASE' commands",
       READ_ONLY GLOBAL_VAR(opt_skip_show_db), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

static Sys_var_charptr Sys_socket(
       "socket", "Socket file to use for connection",
       READ_ONLY GLOBAL_VAR(mysqld_unix_port), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_ulong Sys_thread_stack(
       "thread_stack", "The stack size for each thread",
       READ_ONLY GLOBAL_VAR(my_thread_stack_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(128*1024, ULONG_MAX), DEFAULT(DEFAULT_THREAD_STACK),
       BLOCK_SIZE(1024));

static Sys_var_charptr Sys_tmpdir(
       "tmpdir", "Path for temporary files. Several paths may "
       "be specified, separated by a "
#if defined(_WIN32)
       "semicolon (;)"
#else
       "colon (:)"
#endif
       ", in this case they are used in a round-robin fashion",
       READ_ONLY GLOBAL_VAR(opt_mysql_tmpdir), CMD_LINE(REQUIRED_ARG, 't'),
       IN_FS_CHARSET, DEFAULT(0));

static bool fix_trans_mem_root(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->get_transaction()->init_mem_root_defaults(
        thd->variables.trans_alloc_block_size,
        thd->variables.trans_prealloc_size);
  return false;
}
static Sys_var_ulong Sys_trans_alloc_block_size(
       "transaction_alloc_block_size",
       "Allocation block size for transactions to be stored in binary log",
       SESSION_VAR(trans_alloc_block_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, 128 * 1024), DEFAULT(QUERY_ALLOC_BLOCK_SIZE),
       BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_trans_mem_root));

static Sys_var_ulong Sys_trans_prealloc_size(
       "transaction_prealloc_size",
       "Persistent buffer for transactions to be stored in binary log",
       SESSION_VAR(trans_prealloc_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, 128 * 1024), DEFAULT(TRANS_ALLOC_PREALLOC_SIZE),
       BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_trans_mem_root));

#ifndef EMBEDDED_LIBRARY
static const char *thread_handling_names[]=
{
  "one-thread-per-connection", "no-threads", "loaded-dynamically",
  0
};
static Sys_var_enum Sys_thread_handling(
       "thread_handling",
       "Define threads usage for handling queries, one of "
       "one-thread-per-connection, no-threads, loaded-dynamically"
       , READ_ONLY GLOBAL_VAR(Connection_handler_manager::thread_handling),
       CMD_LINE(REQUIRED_ARG), thread_handling_names, DEFAULT(0));
#endif // !EMBEDDED_LIBRARY

static bool fix_query_cache_size(sys_var *self, THD *thd, enum_var_type type)
{
  ulong new_cache_size= query_cache.resize(query_cache_size);
  /*
     Note: query_cache_size is a global variable reflecting the
     requested cache size. See also query_cache_size_arg
  */
  if (query_cache_size != new_cache_size)
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WARN_QC_RESIZE, ER(ER_WARN_QC_RESIZE),
                        query_cache_size, new_cache_size);

  query_cache_size= new_cache_size;
  return false;
}
static Sys_var_ulong Sys_query_cache_size(
       "query_cache_size",
       "The memory allocated to store results from old queries. "
       "This variable is deprecated and will be removed in a future release.",
       GLOBAL_VAR(query_cache_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(1024U*1024U), BLOCK_SIZE(1024),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_query_cache_size), DEPRECATED(""));

static Sys_var_ulong Sys_query_cache_limit(
       "query_cache_limit",
       "Don't cache results that are bigger than this. "
       "This variable is deprecated and will be removed in a future release.",
       GLOBAL_VAR(query_cache.query_cache_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(1024*1024), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL),
       DEPRECATED(""));

static bool fix_qcache_min_res_unit(sys_var *self, THD *thd, enum_var_type type)
{
  query_cache_min_res_unit=
    query_cache.set_min_res_unit(query_cache_min_res_unit);
  return false;
}
static Sys_var_ulong Sys_query_cache_min_res_unit(
       "query_cache_min_res_unit",
       "The minimum size for blocks allocated by the query cache. "
       "This variable is deprecated and will be removed in a future release.",
       GLOBAL_VAR(query_cache_min_res_unit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(QUERY_CACHE_MIN_RESULT_DATA_SIZE),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_qcache_min_res_unit), DEPRECATED(""));

static const char *query_cache_type_names[]= { "OFF", "ON", "DEMAND", 0 };
static bool check_query_cache_type(sys_var *self, THD *thd, set_var *var)
{
  /*
   Setting it to 0 (or OFF) is always OK, even if the query cache
   is disabled.
  */
  if (var->save_result.ulonglong_value == 0)
    return false;
  else if (query_cache.is_disabled())
  {
    my_error(ER_QUERY_CACHE_DISABLED, MYF(0));
    return true;
  }
  return false;
}
static Sys_var_enum Sys_query_cache_type(
       "query_cache_type",
       "OFF = Don't cache or retrieve results. ON = Cache all results "
       "except SELECT SQL_NO_CACHE ... queries. DEMAND = Cache only "
       "SELECT SQL_CACHE ... queries. "
       "This variable is deprecated and will be removed in a future release.",
       SESSION_VAR(query_cache_type), CMD_LINE(REQUIRED_ARG),
       query_cache_type_names, DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_query_cache_type), ON_UPDATE(NULL), DEPRECATED(""));

static Sys_var_mybool Sys_query_cache_wlock_invalidate(
       "query_cache_wlock_invalidate",
       "Invalidate queries in query cache on LOCK for write. "
       "This variable is deprecated and will be removed in a future release.",
       SESSION_VAR(query_cache_wlock_invalidate), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(NULL), DEPRECATED(""));

static bool
on_check_opt_secure_auth(sys_var *self, THD *thd, set_var *var)
{
  push_deprecated_warn_no_replacement(thd, "--secure-auth");
  return (!var->save_result.ulonglong_value);
}

static Sys_var_mybool Sys_secure_auth(
       "secure_auth",
       "Disallow authentication for accounts that have old (pre-4.1) "
       "passwords. Deprecated. Always TRUE.",
       GLOBAL_VAR(opt_secure_auth), CMD_LINE(OPT_ARG, OPT_SECURE_AUTH),
       DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(on_check_opt_secure_auth)
       );

static Sys_var_charptr Sys_secure_file_priv(
       "secure_file_priv",
       "Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files "
       "within specified directory",
       READ_ONLY GLOBAL_VAR(opt_secure_file_priv),
#ifndef EMBEDDED_LIBRARY
       CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(DEFAULT_SECURE_FILE_PRIV_DIR));
#else
       CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR));
#endif

static bool fix_server_id(sys_var *self, THD *thd, enum_var_type type)
{
  // server_id is 'MYSQL_PLUGIN_IMPORT ulong'
  // So we cast here, rather than change its type.
  server_id_supplied = 1;
  thd->server_id= static_cast<uint32>(server_id);
  return false;
}
static Sys_var_ulong Sys_server_id(
       "server_id",
       "Uniquely identifies the server instance in the community of "
       "replication partners",
       GLOBAL_VAR(server_id), CMD_LINE(REQUIRED_ARG, OPT_SERVER_ID),
       VALID_RANGE(0, UINT_MAX32), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(fix_server_id));

static Sys_var_charptr Sys_server_uuid(
       "server_uuid",
       "Uniquely identifies the server instance in the universe",
       READ_ONLY GLOBAL_VAR(server_uuid_ptr),
       NO_CMD_LINE, IN_FS_CHARSET, DEFAULT(server_uuid));

static Sys_var_uint Sys_server_id_bits(
       "server_id_bits",
       "Set number of significant bits in server-id",
       GLOBAL_VAR(opt_server_id_bits), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 32), DEFAULT(32), BLOCK_SIZE(1));

static Sys_var_mybool Sys_slave_compressed_protocol(
       "slave_compressed_protocol",
       "Use compression on master/slave protocol",
       GLOBAL_VAR(opt_slave_compressed_protocol), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

#ifdef HAVE_REPLICATION
static const char *slave_exec_mode_names[]=
       {"STRICT", "IDEMPOTENT", 0};
static Sys_var_enum Slave_exec_mode(
       "slave_exec_mode",
       "Modes for how replication events should be executed. Legal values "
       "are STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, "
       "replication will not stop for operations that are idempotent. "
       "In STRICT mode, replication will stop on any unexpected difference "
       "between the master and the slave",
       GLOBAL_VAR(slave_exec_mode_options), CMD_LINE(REQUIRED_ARG),
       slave_exec_mode_names, DEFAULT(RBR_EXEC_MODE_STRICT));

const char *slave_type_conversions_name[]=
       {"ALL_LOSSY", "ALL_NON_LOSSY", "ALL_UNSIGNED", "ALL_SIGNED", 0};
static Sys_var_set Slave_type_conversions(
       "slave_type_conversions",
       "Set of slave type conversions that are enabled. Legal values are:"
       " ALL_LOSSY to enable lossy conversions,"
       " ALL_NON_LOSSY to enable non-lossy conversions,"
       " ALL_UNSIGNED to treat all integer column type data to be unsigned values, and"
       " ALL_SIGNED to treat all integer column type data to be signed values." 
       " Default treatment is ALL_SIGNED. If ALL_SIGNED and ALL_UNSIGNED both are"
       " specified, ALL_SIGNED will take higher priority than ALL_UNSIGNED."
       " If the variable is assigned the empty set, no conversions are"
       " allowed and it is expected that the types match exactly.",
       GLOBAL_VAR(slave_type_conversions_options), CMD_LINE(REQUIRED_ARG),
       slave_type_conversions_name,
       DEFAULT(0));

static Sys_var_mybool Sys_slave_sql_verify_checksum(
       "slave_sql_verify_checksum",
       "Force checksum verification of replication events after reading them "
       "from relay log. Note: Events are always checksum-verified by slave on "
       "receiving them from the network before writing them to the relay "
       "log. Enabled by default.",
       GLOBAL_VAR(opt_slave_sql_verify_checksum), CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static bool check_not_null_not_empty(sys_var *self, THD *thd, set_var *var)
{
  String str, *res;
  /* null value is not allowed */
  if (check_not_null(self, thd, var))
    return true;

  /** empty value ('') is not allowed */
  res= var->value? var->value->val_str(&str) : NULL;
  if (res && res->is_empty())
    return true;

  return false;
}

static bool check_slave_stopped(sys_var *self, THD *thd, set_var *var)
{
  bool result= false;
  Master_info *mi= 0;

  if (check_not_null_not_empty(self, thd, var))
    return true;

  channel_map.wrlock();

  for (mi_map::iterator it= channel_map.begin(); it!= channel_map.end(); it++)
  {
    mi= it->second;
    if (mi)
    {
      mysql_mutex_lock(&mi->rli->run_lock);
      if (mi->rli->slave_running)
      {
        my_error(ER_SLAVE_SQL_THREAD_MUST_STOP, MYF(0));
        result= true;
      }
      mysql_mutex_unlock(&mi->rli->run_lock);
    }
  }
  channel_map.unlock();
  return result;
}

static const char *slave_rows_search_algorithms_names[]= {"TABLE_SCAN", "INDEX_SCAN", "HASH_SCAN", 0};
static Sys_var_set Slave_rows_search_algorithms(
       "slave_rows_search_algorithms", 
       "Set of searching algorithms that the slave will use while "
       "searching for records from the storage engine to either "
       "updated or deleted them. Possible values are: INDEX_SCAN, "
       "TABLE_SCAN and HASH_SCAN. Any combination is allowed, and "
       "the slave will always pick the most suitable algorithm for "
       "any given scenario. "
       "(Default: INDEX_SCAN, TABLE_SCAN).",
       GLOBAL_VAR(slave_rows_search_algorithms_options), CMD_LINE(REQUIRED_ARG),
       slave_rows_search_algorithms_names,
       DEFAULT(SLAVE_ROWS_INDEX_SCAN | SLAVE_ROWS_TABLE_SCAN),  NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_not_null_not_empty), ON_UPDATE(NULL));

static const char *mts_parallel_type_names[]= {"DATABASE", "LOGICAL_CLOCK", 0};
static Sys_var_enum Mts_parallel_type(
       "slave_parallel_type",
       "Specifies if the slave will use database partitioning "
       "or information from master to parallelize transactions."
       "(Default: DATABASE).",
       GLOBAL_VAR(mts_parallel_option), CMD_LINE(REQUIRED_ARG),
       mts_parallel_type_names,
       DEFAULT(MTS_PARALLEL_TYPE_DB_NAME),  NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_slave_stopped),
       ON_UPDATE(NULL));

static bool check_binlog_transaction_dependency_tracking(sys_var *self, THD *thd, set_var *var)
{
  if (global_system_variables.transaction_write_set_extraction == HASH_ALGORITHM_OFF
      && var->save_result.ulonglong_value != DEPENDENCY_TRACKING_COMMIT_ORDER)
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "binlog_transaction_dependency_tracking (!= COMMIT_ORDER)",
             "transaction_write_set_extraction (= OFF)");

    return true;
  }
  return false;
}

static bool update_binlog_transaction_dependency_tracking(sys_var* var, THD* thd, enum_var_type v)
{
  /*
    the writeset_history_start needs to be set to 0 whenever there is a
    change in the transaction dependency source so that WS and COMMIT
    transition smoothly.
  */
  mysql_bin_log.m_dependency_tracker.tracking_mode_changed();
  return false;
}

void PolyLock_lock_log::rdlock()
{
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
}

void PolyLock_lock_log::wrlock()
{
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
}

void PolyLock_lock_log::unlock()
{
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());
}

static PolyLock_lock_log PLock_log;
static const char *opt_binlog_transaction_dependency_tracking_names[]=
       {"COMMIT_ORDER", "WRITESET", "WRITESET_SESSION", NullS};
static Sys_var_enum Binlog_transaction_dependency_tracking(
       "binlog_transaction_dependency_tracking",
       "Selects the source of dependency information from which to "
       "assess which transactions can be executed in parallel by the "
       "slave's multi-threaded applier. "
       "Possible values are COMMIT_ORDER, WRITESET and WRITESET_SESSION.",
       GLOBAL_VAR(mysql_bin_log.m_dependency_tracker.m_opt_tracking_mode),
       CMD_LINE(REQUIRED_ARG), opt_binlog_transaction_dependency_tracking_names,
       DEFAULT(DEPENDENCY_TRACKING_COMMIT_ORDER), &PLock_log,
       NOT_IN_BINLOG, ON_CHECK(check_binlog_transaction_dependency_tracking),
       ON_UPDATE(update_binlog_transaction_dependency_tracking));
static Sys_var_ulong Binlog_transaction_dependency_history_size(
       "binlog_transaction_dependency_history_size",
       "Maximum number of rows to keep in the writeset history.",
       GLOBAL_VAR(mysql_bin_log.m_dependency_tracker.get_writeset()->m_opt_max_history_size),
       CMD_LINE(REQUIRED_ARG, 0), VALID_RANGE(1, 1000000), DEFAULT(25000),
       BLOCK_SIZE(1), &PLock_log, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(NULL));

static Sys_var_mybool Sys_slave_preserve_commit_order(
       "slave_preserve_commit_order",
       "Force slave workers to make commits in the same order as on the master. "
       "Disabled by default.",
       GLOBAL_VAR(opt_slave_preserve_commit_order), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_slave_stopped),
       ON_UPDATE(NULL));
#endif

bool Sys_var_enum_binlog_checksum::global_update(THD *thd, set_var *var)
{
  bool check_purge= false;

  /*
    SET binlog_checksome command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.gtid_executed' replication repository
    table.
  */
  thd->set_skip_readonly_check();
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  if(mysql_bin_log.is_open())
  {
    bool alg_changed=
      (binlog_checksum_options != (uint) var->save_result.ulonglong_value);
    if (alg_changed)
      mysql_bin_log.checksum_alg_reset= (uint8) var->save_result.ulonglong_value;
    mysql_bin_log.rotate(true, &check_purge);
    if (alg_changed)
      mysql_bin_log.checksum_alg_reset= binary_log::BINLOG_CHECKSUM_ALG_UNDEF; // done
  }
  else
  {
    binlog_checksum_options=
      static_cast<ulong>(var->save_result.ulonglong_value);
  }
  DBUG_ASSERT(binlog_checksum_options == var->save_result.ulonglong_value);
  DBUG_ASSERT(mysql_bin_log.checksum_alg_reset ==
              binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());
  
  if (check_purge)
    mysql_bin_log.purge();

  return 0;
}

static Sys_var_enum_binlog_checksum Binlog_checksum_enum(
       "binlog_checksum", "Type of BINLOG_CHECKSUM_ALG. Include checksum for "
       "log events in the binary log. Possible values are NONE and CRC32; "
       "default is CRC32.",
       GLOBAL_VAR(binlog_checksum_options), CMD_LINE(REQUIRED_ARG),
       binlog_checksum_type_names, DEFAULT(binary_log::BINLOG_CHECKSUM_ALG_CRC32),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_outside_trx));

static Sys_var_mybool Sys_master_verify_checksum(
       "master_verify_checksum",
       "Force checksum verification of logged events in binary log before "
       "sending them to slaves or printing them in output of SHOW BINLOG EVENTS. "
       "Disabled by default.",
       GLOBAL_VAR(opt_master_verify_checksum), CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_ulong Sys_slow_launch_time(
       "slow_launch_time",
       "If creating the thread takes longer than this value (in seconds), "
       "the Slow_launch_threads counter will be incremented",
       GLOBAL_VAR(slow_launch_time), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(2), BLOCK_SIZE(1));

static Sys_var_ulong Sys_sort_buffer(
       "sort_buffer_size",
       "Each thread that needs to do a sort allocates a buffer of this size",
       SESSION_VAR(sortbuff_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(MIN_SORT_MEMORY, ULONG_MAX), DEFAULT(DEFAULT_SORT_MEMORY),
       BLOCK_SIZE(1));

/**
  Check sql modes strict_mode, 'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and
  'ERROR_FOR_DIVISION_BY_ZERO' are used together. If only subset of it
  is set then warning is reported.

  @param sql_mode sql mode.
*/
static void check_sub_modes_of_strict_mode(sql_mode_t &sql_mode, THD *thd)
{
  const sql_mode_t strict_modes= (MODE_STRICT_TRANS_TABLES |
                                  MODE_STRICT_ALL_TABLES);

  const sql_mode_t new_strict_submodes= (MODE_NO_ZERO_IN_DATE |
                                         MODE_NO_ZERO_DATE |
                                         MODE_ERROR_FOR_DIVISION_BY_ZERO);

  const sql_mode_t strict_modes_set= (sql_mode & strict_modes);
  const sql_mode_t new_strict_submodes_set= (sql_mode & new_strict_submodes);

  if (((strict_modes_set | new_strict_submodes_set) !=0) &&
      ((new_strict_submodes_set != new_strict_submodes) ||
       (strict_modes_set == 0)))
  {
    if (thd)
      push_warning(thd, Sql_condition::SL_WARNING,
                               ER_SQL_MODE_MERGED,
                               ER_THD(thd, ER_SQL_MODE_MERGED));
    else
      sql_print_warning("'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and "
                        "'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used "
                        "with strict mode. They will be merged with strict mode "
                        "in a future release.");
  }
}

export sql_mode_t expand_sql_mode(sql_mode_t sql_mode, THD *thd)
{
  if (sql_mode & MODE_ANSI)
  {
    /*
      Note that we dont set
      MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS | MODE_NO_FIELD_OPTIONS
      to allow one to get full use of MySQL in this mode.
    */
    sql_mode|= (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
                MODE_IGNORE_SPACE | MODE_ONLY_FULL_GROUP_BY);
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
                MODE_ERROR_FOR_DIVISION_BY_ZERO | MODE_NO_AUTO_CREATE_USER |
                MODE_NO_ENGINE_SUBSTITUTION);

  check_sub_modes_of_strict_mode(sql_mode, thd);
  return sql_mode;
}
static bool check_sql_mode(sys_var *self, THD *thd, set_var *var)
{
  var->save_result.ulonglong_value=
    expand_sql_mode(var->save_result.ulonglong_value, thd);

  /* Warning displayed only if the non default sql_mode is specified. */
  if (var->value)
  {
    /* Check if the NO_AUTO_CREATE_USER flag has been swapped. */
    if ((thd->variables.sql_mode ^ var->save_result.ulonglong_value) &
        MODE_NO_AUTO_CREATE_USER)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SQLMODE,
                          ER_THD(thd, ER_WARN_DEPRECATED_SQLMODE),
                          "NO_AUTO_CREATE_USER");
    }
    static const sql_mode_t deprecated_mask= MODE_DB2 | MODE_MAXDB |
      MODE_MSSQL | MODE_MYSQL323 | MODE_MYSQL40 | MODE_ORACLE |
      MODE_POSTGRESQL | MODE_NO_FIELD_OPTIONS | MODE_NO_KEY_OPTIONS |
      MODE_NO_TABLE_OPTIONS;
    sql_mode_t deprecated_modes=
      var->save_result.ulonglong_value & deprecated_mask;
    if (deprecated_modes != 0)
    {
      LEX_STRING buf;
      if (sql_mode_string_representation(thd, deprecated_modes, &buf))
        return true; // OOM
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SQLMODE,
                          ER_THD(thd, ER_WARN_DEPRECATED_SQLMODE),
                          buf.str);
    }
  }

  return false;
}
static bool fix_sql_mode(sys_var *self, THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    /* Update thd->server_status */
    if (thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
      thd->server_status|= SERVER_STATUS_NO_BACKSLASH_ESCAPES;
    else
      thd->server_status&= ~SERVER_STATUS_NO_BACKSLASH_ESCAPES;
  }
  return false;
}
/*
  WARNING: When adding new SQL modes don't forget to update the
  tables definitions that stores it's value (ie: mysql.event, mysql.proc)
*/
static const char *sql_mode_names[]=
{
  "REAL_AS_FLOAT", "PIPES_AS_CONCAT", "ANSI_QUOTES", "IGNORE_SPACE", ",",
  "ONLY_FULL_GROUP_BY", "NO_UNSIGNED_SUBTRACTION", "NO_DIR_IN_CREATE",
  "POSTGRESQL", "ORACLE", "MSSQL", "DB2", "MAXDB", "NO_KEY_OPTIONS",
  "NO_TABLE_OPTIONS", "NO_FIELD_OPTIONS", "MYSQL323", "MYSQL40", "ANSI",
  "NO_AUTO_VALUE_ON_ZERO", "NO_BACKSLASH_ESCAPES", "STRICT_TRANS_TABLES",
  "STRICT_ALL_TABLES", "NO_ZERO_IN_DATE", "NO_ZERO_DATE",
  "ALLOW_INVALID_DATES", "ERROR_FOR_DIVISION_BY_ZERO", "TRADITIONAL",
  "NO_AUTO_CREATE_USER", "HIGH_NOT_PRECEDENCE", "NO_ENGINE_SUBSTITUTION",
  "PAD_CHAR_TO_FULL_LENGTH",
  0
};
export bool sql_mode_string_representation(THD *thd, sql_mode_t sql_mode,
                                           LEX_STRING *ls)
{
  set_to_string(thd, ls, sql_mode, sql_mode_names);
  return ls->str == 0;
}
/*
  sql_mode should *not* be IN_BINLOG: even though it is written to the binlog,
  the slave ignores the MODE_NO_DIR_IN_CREATE variable, so slave's value
  differs from master's (see log_event.cc: Query_log_event::do_apply_event()).
*/
static Sys_var_set Sys_sql_mode(
       "sql_mode",
       "Syntax: sql-mode=mode[,mode[,mode...]]. See the manual for the "
       "complete list of valid sql modes",
       SESSION_VAR(sql_mode), CMD_LINE(REQUIRED_ARG),
       sql_mode_names,
       DEFAULT(MODE_NO_ENGINE_SUBSTITUTION |
               MODE_ONLY_FULL_GROUP_BY |
               MODE_STRICT_TRANS_TABLES |
               MODE_NO_ZERO_IN_DATE |
               MODE_NO_ZERO_DATE |
               MODE_ERROR_FOR_DIVISION_BY_ZERO |
               MODE_NO_AUTO_CREATE_USER),
       NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_sql_mode), ON_UPDATE(fix_sql_mode));

static Sys_var_ulong Sys_max_execution_time(
       "max_execution_time",
       "Kill SELECT statement that takes over the specified number of milliseconds",
       SESSION_VAR(max_execution_time), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1));

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
#define SSL_OPT(X) CMD_LINE(REQUIRED_ARG,X)
#else
#define SSL_OPT(X) NO_CMD_LINE
#endif

/*
  If you are adding new system variable for SSL communication, please take a
  look at do_auto_cert_generation() function in sql_authentication.cc and
  add new system variable in checks if required.
*/

static Sys_var_charptr Sys_ssl_ca(
       "ssl_ca",
       "CA file in PEM format (check OpenSSL docs, implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_ca), SSL_OPT(OPT_SSL_CA),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_ssl_capath(
       "ssl_capath",
       "CA directory (check OpenSSL docs, implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_capath), SSL_OPT(OPT_SSL_CAPATH),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_tls_version(
       "tls_version",
       "TLS version, permitted values are TLSv1, TLSv1.1, TLSv1.2(Only for openssl)",
       READ_ONLY GLOBAL_VAR(opt_tls_version), SSL_OPT(OPT_TLS_VERSION),
#ifdef HAVE_YASSL
       IN_FS_CHARSET, "TLSv1,TLSv1.1");
#else
       IN_FS_CHARSET, "TLSv1,TLSv1.1,TLSv1.2");
#endif

static Sys_var_charptr Sys_ssl_cert(
       "ssl_cert", "X509 cert in PEM format (implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_cert), SSL_OPT(OPT_SSL_CERT),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_ssl_cipher(
       "ssl_cipher", "SSL cipher to use (implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_cipher), SSL_OPT(OPT_SSL_CIPHER),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_ssl_key(
       "ssl_key", "X509 key in PEM format (implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_key), SSL_OPT(OPT_SSL_KEY),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_ssl_crl(
       "ssl_crl",
       "CRL file in PEM format (check OpenSSL docs, implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_crl), SSL_OPT(OPT_SSL_CRL),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_ssl_crlpath(
       "ssl_crlpath",
       "CRL directory (check OpenSSL docs, implies --ssl)",
       READ_ONLY GLOBAL_VAR(opt_ssl_crlpath), SSL_OPT(OPT_SSL_CRLPATH),
       IN_FS_CHARSET, DEFAULT(0));

#if defined(HAVE_OPENSSL) && !defined(HAVE_YASSL)
static Sys_var_mybool Sys_auto_generate_certs(
       "auto_generate_certs",
       "Auto generate SSL certificates at server startup if --ssl is set to "
       "ON and none of the other SSL system variables are specified and "
       "certificate/key files are not present in data directory.",
       READ_ONLY GLOBAL_VAR(opt_auto_generate_certs),
       CMD_LINE(OPT_ARG),
       DEFAULT(TRUE),
       NO_MUTEX_GUARD,
       NOT_IN_BINLOG,
       ON_CHECK(NULL),
       ON_UPDATE(NULL),
       NULL);
#endif /* HAVE_OPENSSL && !HAVE_YASSL */

// why ENUM and not BOOL ?
static const char *updatable_views_with_limit_names[]= {"NO", "YES", 0};
static Sys_var_enum Sys_updatable_views_with_limit(
       "updatable_views_with_limit",
       "YES = Don't issue an error message (warning only) if a VIEW without "
       "presence of a key of the underlying table is used in queries with a "
       "LIMIT clause for updating. NO = Prohibit update of a VIEW, which "
       "does not contain a key of the underlying table and the query uses "
       "a LIMIT clause (usually get from GUI tools)",
       SESSION_VAR(updatable_views_with_limit), CMD_LINE(REQUIRED_ARG),
       updatable_views_with_limit_names, DEFAULT(TRUE));

static Sys_var_mybool Sys_sync_frm(
       "sync_frm", "Sync .frm files to disk on creation",
       GLOBAL_VAR(opt_sync_frm), CMD_LINE(OPT_ARG),
       DEFAULT(TRUE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));

static char *system_time_zone_ptr;
static Sys_var_charptr Sys_system_time_zone(
       "system_time_zone", "The server system time zone",
       READ_ONLY GLOBAL_VAR(system_time_zone_ptr), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(system_time_zone));

static Sys_var_ulong Sys_table_def_size(
       "table_definition_cache",
       "The number of cached table definitions",
       GLOBAL_VAR(table_def_size),
       CMD_LINE(REQUIRED_ARG, OPT_TABLE_DEFINITION_CACHE),
       VALID_RANGE(TABLE_DEF_CACHE_MIN, 512*1024),
       DEFAULT(TABLE_DEF_CACHE_DEFAULT),
       BLOCK_SIZE(1),
       NO_MUTEX_GUARD,
       NOT_IN_BINLOG,
       ON_CHECK(NULL),
       ON_UPDATE(NULL),
       NULL,
       /* table_definition_cache is used as a sizing hint by the performance schema. */
       sys_var::PARSE_EARLY);

static bool fix_table_cache_size(sys_var *self, THD *thd, enum_var_type type)
{
  /*
    table_open_cache parameter is a soft limit for total number of objects
    in all table cache instances. Once this value is updated we need to
    update value of a per-instance soft limit on table cache size.
  */
  table_cache_size_per_instance= table_cache_size / table_cache_instances;
  return false;
}

static Sys_var_ulong Sys_table_cache_size(
       "table_open_cache", "The number of cached open tables "
       "(total for all table cache instances)",
       GLOBAL_VAR(table_cache_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, 512*1024), DEFAULT(TABLE_OPEN_CACHE_DEFAULT),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(fix_table_cache_size),
       NULL,
       /* table_open_cache is used as a sizing hint by the performance schema. */
       sys_var::PARSE_EARLY);

static Sys_var_ulong Sys_table_cache_instances(
       "table_open_cache_instances", "The number of table cache instances",
       READ_ONLY GLOBAL_VAR(table_cache_instances), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, Table_cache_manager::MAX_TABLE_CACHES),
       DEFAULT(Table_cache_manager::DEFAULT_MAX_TABLE_CACHES),
       BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(NULL), NULL,
       /*
         table_open_cache is used as a sizing hint by the performance schema,
         and 'table_open_cache' is a prefix of 'table_open_cache_instances'.
         Is is better to keep these options together, to avoid confusing
         handle_options() with partial name matches.
       */
       sys_var::PARSE_EARLY);

#ifndef EMBEDDED_LIBRARY
static Sys_var_ulong Sys_thread_cache_size(
       "thread_cache_size",
       "How many threads we should keep in a cache for reuse",
       GLOBAL_VAR(Per_thread_connection_handler::max_blocked_pthreads),
       CMD_LINE(REQUIRED_ARG, OPT_THREAD_CACHE_SIZE),
       VALID_RANGE(0, 16384), DEFAULT(0), BLOCK_SIZE(1));
#endif // !EMBEDDED_LIBRARY


/**
  Function to check if the 'next' transaction isolation level
  ('tx_isolation'/ its alternative 'transaction_isolation')
  can be changed.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler.
  @param[in] var    A pointer to set_var holding the specified list of
                    system variable names.

  @retval   FALSE   Success.
  @retval   TRUE    Error.
*/
static bool check_transaction_isolation(sys_var *self, THD *thd, set_var *var)
{
  if (var->type == OPT_DEFAULT && (thd->in_active_multi_stmt_transaction() ||
                                   thd->in_sub_stmt))
  {
    DBUG_ASSERT(thd->in_multi_stmt_transaction_mode() || thd->in_sub_stmt);
    my_error(ER_CANT_CHANGE_TX_CHARACTERISTICS, MYF(0));
    return true;
  }
  return false;
}


/**
  This function sets the session variable thd->variables.tx_isolation/
  thd->variables.transaction_isolation to reflect changes
  to @@session.tx_isolation/@@session.transaction_isolation.
  'tx_isolation' is deprecated and 'transaction_isolation' is its
  alternative.

  @param[in] thd    Thread handler.
  @param[in] var    A pointer to the set_var.

  @retval   FALSE   Success.
  @retval   TRUE    Error.
*/

bool Sys_var_tx_isolation::session_update(THD *thd, set_var *var)
{
  if (var->type == OPT_SESSION && Sys_var_enum::session_update(thd, var))
    return TRUE;

  if (var->type == OPT_DEFAULT || !(thd->in_active_multi_stmt_transaction() ||
                                    thd->in_sub_stmt))
  {
    /*
      Update the isolation level of the next transaction.
      I.e. if one did:
      COMMIT;
      SET SESSION ISOLATION LEVEL ...
      BEGIN; <-- this transaction has the new isolation
      Note, that in case of:
      COMMIT;
      SET TRANSACTION ISOLATION LEVEL ...
      SET SESSION ISOLATION LEVEL ...
      BEGIN; <-- the session isolation level is used, not the
      result of SET TRANSACTION statement.

      When we are in a trigger/function the transaction is already
      started. Adhering to above behavior, the SET TRANSACTION would
      fail when run from within trigger/function. And SET SESSION
      TRANSACTION would always succeed making the characteristics
      effective for the next transaction that starts.
     */
    enum_tx_isolation tx_isol;
    tx_isol= (enum_tx_isolation) var->save_result.ulonglong_value;
    bool one_shot= (var->type == OPT_DEFAULT);
    return set_tx_isolation(thd, tx_isol, one_shot);
  }
  return FALSE;
}


/**
  This function updates the thd->variables.transaction_isolation
  to reflect the changes made to @@session.tx_isolation. 'tx_isolation' is
  deprecated and 'transaction_isolation' is its alternative.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler.
  @param[in] type   The type SESSION, GLOBAL or DEFAULT.

  @retval   FALSE   Success.
  @retval   TRUE    Error.
*/
static bool update_transaction_isolation(sys_var *self, THD *thd,
                                         enum_var_type type)
{
  SV *sv= type == OPT_GLOBAL ? &global_system_variables : &thd->variables;
  sv->transaction_isolation= sv->tx_isolation;
  return false;
}


/**
  This function updates thd->variables.tx_isolation to reflect the
  changes to @@session.transaction_isolation. 'tx_isolation' is
  deprecated and 'transaction_isolation' is its alternative.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler.
  @param[in] type   The type SESSION, GLOBAL or DEFAULT.

  @retval   FALSE   Success.
  @retval   TRUE    Error.
*/
static bool update_tx_isolation(sys_var *self, THD *thd,
                                enum_var_type type)
{
  SV *sv= type == OPT_GLOBAL ? &global_system_variables : &thd->variables;
  sv->tx_isolation= sv->transaction_isolation;
  return false;
}


// NO_CMD_LINE - different name of the option
static Sys_var_tx_isolation Sys_tx_isolation(
       "tx_isolation", "Default transaction isolation level."
       "This variable is deprecated and will be removed in a future release.",
       UNTRACKED_DEFAULT SESSION_VAR(tx_isolation), NO_CMD_LINE,
       tx_isolation_names, DEFAULT(ISO_REPEATABLE_READ),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_transaction_isolation),
       ON_UPDATE(update_transaction_isolation),
       DEPRECATED("'@@transaction_isolation'"));


// NO_CMD_LINE
static Sys_var_tx_isolation Sys_transaction_isolation(
       "transaction_isolation", "Default transaction isolation level",
       UNTRACKED_DEFAULT SESSION_VAR(transaction_isolation), NO_CMD_LINE,
       tx_isolation_names, DEFAULT(ISO_REPEATABLE_READ),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_transaction_isolation),
       ON_UPDATE(update_tx_isolation));


/**
  Function to check if the state of deprecated variable 'tx_read_only'/
  its alternative 'transaction_read_only' can be changed. The state cannot
  be changed if there is already a transaction in progress.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler
  @param[in] var    A pointer to set_var holding the specified list of
                    system variable names.

  @retval   FALSE   Success.
  @retval   TRUE    Error.
*/

static bool check_transaction_read_only(sys_var *self, THD *thd, set_var *var)
{
  if (var->type == OPT_DEFAULT && (thd->in_active_multi_stmt_transaction() ||
                                   thd->in_sub_stmt))
  {
    DBUG_ASSERT(thd->in_multi_stmt_transaction_mode() || thd->in_sub_stmt);
    my_error(ER_CANT_CHANGE_TX_CHARACTERISTICS, MYF(0));
    return true;
  }
  return false;
}


/**
  This function sets the session variable thd->variables.tx_read_only/
  thd->variables.transaction_read_only to reflect changes to
  @@session.tx_read_only/@@session.transaction_read_only. 'tx_read_only'
  is deprecated and 'transaction_read_only' is its alternative.

  @param[in] thd    Thread handler.
  @param[in] var    A pointer to the set_var.

  @retval   FALSE   Success.
*/
bool Sys_var_tx_read_only::session_update(THD *thd, set_var *var)
{
  if (var->type == OPT_SESSION && Sys_var_mybool::session_update(thd, var))
    return true;

  if (var->type == OPT_DEFAULT || !(thd->in_active_multi_stmt_transaction() ||
                                    thd->in_sub_stmt))
  {
    // @see Sys_var_tx_isolation::session_update() above for the rules.
    thd->tx_read_only= var->save_result.ulonglong_value;

    if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
    {
      Transaction_state_tracker *tst= (Transaction_state_tracker *)
             thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER);

      if (var->type == OPT_DEFAULT)
        tst->set_read_flags(thd,
                            thd->tx_read_only ? TX_READ_ONLY : TX_READ_WRITE);
      else
        tst->set_read_flags(thd, TX_READ_INHERIT);
    }
  }
  return false;
}


/**
  This function updates the session variable thd->variables.tx_read_only
  to reflect changes made to  @@session.transaction_read_only. The variable
  'tx_read_only' is deprecated and 'transaction_read_only' is its alternative.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler.
  @param[in] type   The type SESSION, GLOBAL or DEFAULT.

  @retval   FALSE   Success.
*/
static bool update_tx_read_only(sys_var *self, THD *thd,
                                enum_var_type type)
{
  SV *sv= type == OPT_GLOBAL ? &global_system_variables : &thd->variables;
  sv->tx_read_only= sv->transaction_read_only;
  return false;
}


/**
  This function updates the session variable
  thd->variables.transaction_read_only to reflect changes made to
  @@session.tx_read_only. 'tx_read_only' is deprecated and
  'transaction_read_only' is its alternative.

  @param[in] self   A pointer to the sys_var.
  @param[in] thd    Thread handler.
  @param[in] type   The type SESSION, GLOBAL or DEFAULT.

  @retval   FALSE   Success.
*/
static bool update_transaction_read_only(sys_var *self, THD *thd,
                                         enum_var_type type)
{
  SV *sv= type == OPT_GLOBAL ? &global_system_variables : &thd->variables;
  sv->transaction_read_only= sv->tx_read_only;
  return false;
}


static Sys_var_tx_read_only Sys_tx_read_only(
       "tx_read_only", "Set default transaction access mode to read only."
       "This variable is deprecated and will be removed in a future release.",
       UNTRACKED_DEFAULT SESSION_VAR(tx_read_only), NO_CMD_LINE, DEFAULT(0),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_transaction_read_only),
       ON_UPDATE(update_transaction_read_only),
       DEPRECATED("'@@transaction_read_only'"));


static Sys_var_tx_read_only Sys_transaction_read_only(
       "transaction_read_only",
       "Set default transaction access mode to read only.",
       UNTRACKED_DEFAULT SESSION_VAR(transaction_read_only), NO_CMD_LINE,
       DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_transaction_read_only),
       ON_UPDATE(update_tx_read_only));


static Sys_var_ulonglong Sys_tmp_table_size(
       "tmp_table_size",
       "If an internal in-memory temporary table exceeds this size, MySQL "
       "will automatically convert it to an on-disk MyISAM table",
       SESSION_VAR(tmp_table_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, (ulonglong)~(intptr)0), DEFAULT(16*1024*1024),
       BLOCK_SIZE(1));

static char *server_version_ptr;
static Sys_var_version Sys_version(
       "version", "Server version",
       READ_ONLY GLOBAL_VAR(server_version_ptr), NO_CMD_LINE,
       IN_SYSTEM_CHARSET, DEFAULT(server_version));

static char *server_version_comment_ptr;
static Sys_var_charptr Sys_version_comment(
       "version_comment", "version_comment",
       READ_ONLY GLOBAL_VAR(server_version_comment_ptr), NO_CMD_LINE,
       IN_SYSTEM_CHARSET, DEFAULT(MYSQL_COMPILATION_COMMENT));

static char *server_version_compile_machine_ptr;
static Sys_var_charptr Sys_version_compile_machine(
       "version_compile_machine", "version_compile_machine",
       READ_ONLY GLOBAL_VAR(server_version_compile_machine_ptr), NO_CMD_LINE,
       IN_SYSTEM_CHARSET, DEFAULT(MACHINE_TYPE));

static char *server_version_compile_os_ptr;
static Sys_var_charptr Sys_version_compile_os(
       "version_compile_os", "version_compile_os",
       READ_ONLY GLOBAL_VAR(server_version_compile_os_ptr), NO_CMD_LINE,
       IN_SYSTEM_CHARSET, DEFAULT(SYSTEM_TYPE));

static Sys_var_ulong Sys_net_wait_timeout(
       "wait_timeout",
       "The number of seconds the server waits for activity on a "
       "connection before closing it",
       SESSION_VAR(net_wait_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, IF_WIN(INT_MAX32/1000, LONG_TIMEOUT)),
       DEFAULT(NET_WAIT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_plugin Sys_default_storage_engine(
       "default_storage_engine", "The default storage engine for new tables",
       SESSION_VAR(table_plugin), NO_CMD_LINE,
       MYSQL_STORAGE_ENGINE_PLUGIN, DEFAULT(&default_storage_engine),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_storage_engine));

const char *internal_tmp_disk_storage_engine_names[] = { "MyISAM", "InnoDB", 0};
static Sys_var_enum Sys_internal_tmp_disk_storage_engine(
       "internal_tmp_disk_storage_engine",
       "The default storage engine for on-disk internal tmp table",
       GLOBAL_VAR(internal_tmp_disk_storage_engine), CMD_LINE(OPT_ARG),
       internal_tmp_disk_storage_engine_names, DEFAULT(TMP_TABLE_INNODB));

static Sys_var_plugin Sys_default_tmp_storage_engine(
       "default_tmp_storage_engine", "The default storage engine for new explicit temporary tables",
       SESSION_VAR(temp_table_plugin), NO_CMD_LINE,
       MYSQL_STORAGE_ENGINE_PLUGIN, DEFAULT(&default_tmp_storage_engine),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_storage_engine));

#if defined(ENABLED_DEBUG_SYNC)
/*
  Variable can be set for the session only.

  This could be changed later. Then we need to have a global array of
  actions in addition to the thread local ones. SET GLOBAL would
  manage the global array, SET [SESSION] the local array. A sync point
  would need to look for a local and a global action. Setting and
  executing of global actions need to be protected by a mutex.

  The purpose of global actions could be to allow synchronizing with
  connectionless threads that cannot execute SET statements.
*/
static Sys_var_debug_sync Sys_debug_sync(
       "debug_sync", "Debug Sync Facility",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_has_super));
#endif /* defined(ENABLED_DEBUG_SYNC) */

/**
 "time_format" "date_format" "datetime_format"

  the following three variables are unused, and the source of confusion
  (bug reports like "I've changed date_format, but date format hasn't changed.
  I've made them read-only, to alleviate the situation somewhat.

  @todo make them NO_CMD_LINE ?
*/
static Sys_var_charptr Sys_date_format(
       "date_format", "The DATE format (ignored)",
       READ_ONLY GLOBAL_VAR(global_date_format.format.str),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(known_date_time_formats[ISO_FORMAT].date_format),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_charptr Sys_datetime_format(
       "datetime_format", "The DATETIME format (ignored)",
       READ_ONLY GLOBAL_VAR(global_datetime_format.format.str),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(known_date_time_formats[ISO_FORMAT].datetime_format),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static Sys_var_charptr Sys_time_format(
       "time_format", "The TIME format (ignored)",
       READ_ONLY GLOBAL_VAR(global_time_format.format.str),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(known_date_time_formats[ISO_FORMAT].time_format),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0),
       DEPRECATED(""));

static bool fix_autocommit(sys_var *self, THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    if (global_system_variables.option_bits & OPTION_AUTOCOMMIT)
      global_system_variables.option_bits&= ~OPTION_NOT_AUTOCOMMIT;
    else
      global_system_variables.option_bits|= OPTION_NOT_AUTOCOMMIT;
    return false;
  }

  if (thd->variables.option_bits & OPTION_AUTOCOMMIT &&
      thd->variables.option_bits & OPTION_NOT_AUTOCOMMIT)
  { // activating autocommit

    if (trans_commit_stmt(thd) || trans_commit(thd))
    {
      thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
      return true;
    }
    /*
      Don't close thread tables or release metadata locks: if we do so, we
      risk releasing locks/closing tables of expressions used to assign
      other variables, as in:
      set @var=my_stored_function1(), @@autocommit=1, @var2=(select max(a)
      from my_table), ...
      The locks will be released at statement end anyway, as SET
      statement that assigns autocommit is marked to commit
      transaction implicitly at the end (@sa stmt_causes_implicitcommit()).
    */
    thd->variables.option_bits&=
                 ~(OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
    thd->get_transaction()->reset_unsafe_rollback_flags(
        Transaction_ctx::SESSION);
    thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
    return false;
  }

  if (!(thd->variables.option_bits & OPTION_AUTOCOMMIT) &&
      !(thd->variables.option_bits & OPTION_NOT_AUTOCOMMIT))
  { // disabling autocommit

    thd->get_transaction()->reset_unsafe_rollback_flags(
        Transaction_ctx::SESSION);
    thd->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;
    return false;
  }

  return false; // autocommit value wasn't changed
}
static Sys_var_bit Sys_autocommit(
       "autocommit", "autocommit",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_AUTOCOMMIT, DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(fix_autocommit));
export sys_var *Sys_autocommit_ptr= &Sys_autocommit; // for sql_yacc.yy

static Sys_var_mybool Sys_big_tables(
       "big_tables", "Allow big result sets by saving all "
       "temporary sets on file (Solves most 'table full' errors)",
       SESSION_VAR(big_tables), CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_bit Sys_big_selects(
       "sql_big_selects", "sql_big_selects",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_BIG_SELECTS,
       DEFAULT(FALSE));

static Sys_var_bit Sys_log_off(
       "sql_log_off", "sql_log_off",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_LOG_OFF,
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_has_super));

/**
  This function sets the session variable thd->variables.sql_log_bin 
  to reflect changes to @@session.sql_log_bin.

  @param[in] self   A pointer to the sys_var, i.e. Sys_log_binlog.
  @param[in] type   The type either session or global.

  @return @c FALSE.
*/
static bool fix_sql_log_bin_after_update(sys_var *self, THD *thd,
                                         enum_var_type type)
{
  DBUG_ASSERT(type == OPT_SESSION);

  if (thd->variables.sql_log_bin)
    thd->variables.option_bits |= OPTION_BIN_LOG;
  else
    thd->variables.option_bits &= ~OPTION_BIN_LOG;

  return FALSE;
}

/**
  This function checks if the sql_log_bin can be changed,
  what is possible if:
    - the user is a super user;
    - the set is not called from within a function/trigger;
    - there is no on-going transaction.

  @param[in] self   A pointer to the sys_var, i.e. Sys_log_binlog.
  @param[in] var    A pointer to the set_var created by the parser.

  @return @c FALSE if the change is allowed, otherwise @c TRUE.
*/
static bool check_sql_log_bin(sys_var *self, THD *thd, set_var *var)
{
  if (check_has_super(self, thd, var))
    return TRUE;

  if (var->type == OPT_GLOBAL)
    return TRUE;

  /* If in a stored function/trigger, it's too late to change sql_log_bin. */
  if (thd->in_sub_stmt)
  {
    my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_SQL_LOG_BIN, MYF(0));
    return TRUE;
  }
  /* Make the session variable 'sql_log_bin' read-only inside a transaction. */
  if (thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_SQL_LOG_BIN, MYF(0));
    return TRUE;
  }

  return FALSE;
}

static Sys_var_sql_log_bin Sys_log_binlog(
       "sql_log_bin", "Controls whether logging to the binary log is done",
       SESSION_VAR(sql_log_bin), NO_CMD_LINE, DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_sql_log_bin),
       ON_UPDATE(fix_sql_log_bin_after_update));

static Sys_var_bit Sys_transaction_allow_batching(
       "transaction_allow_batching", "transaction_allow_batching",
       SESSION_ONLY(option_bits), NO_CMD_LINE, OPTION_ALLOW_BATCH,
       DEFAULT(FALSE));

static Sys_var_bit Sys_sql_warnings(
       "sql_warnings", "sql_warnings",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_WARNINGS,
       DEFAULT(FALSE));

static Sys_var_bit Sys_sql_notes(
       "sql_notes", "sql_notes",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_SQL_NOTES,
       DEFAULT(TRUE));

static Sys_var_bit Sys_auto_is_null(
       "sql_auto_is_null", "sql_auto_is_null",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_AUTO_IS_NULL,
       DEFAULT(FALSE), NO_MUTEX_GUARD, IN_BINLOG);

static Sys_var_bit Sys_safe_updates(
       "sql_safe_updates", "sql_safe_updates",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_SAFE_UPDATES,
       DEFAULT(FALSE));

static Sys_var_bit Sys_buffer_results(
       "sql_buffer_result", "sql_buffer_result",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_BUFFER_RESULT,
       DEFAULT(FALSE));

static Sys_var_bit Sys_quote_show_create(
       "sql_quote_show_create", "sql_quote_show_create",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_QUOTE_SHOW_CREATE,
       DEFAULT(TRUE));

static Sys_var_bit Sys_foreign_key_checks(
       "foreign_key_checks", "foreign_key_checks",
       SESSION_VAR(option_bits), NO_CMD_LINE,
       REVERSE(OPTION_NO_FOREIGN_KEY_CHECKS),
       DEFAULT(TRUE), NO_MUTEX_GUARD, IN_BINLOG);

static Sys_var_bit Sys_unique_checks(
       "unique_checks", "unique_checks",
       SESSION_VAR(option_bits), NO_CMD_LINE,
       REVERSE(OPTION_RELAXED_UNIQUE_CHECKS),
       DEFAULT(TRUE), NO_MUTEX_GUARD, IN_BINLOG);

#ifdef ENABLED_PROFILING
static Sys_var_bit Sys_profiling(
       "profiling", "profiling",
       SESSION_VAR(option_bits), NO_CMD_LINE, OPTION_PROFILING,
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(0), DEPRECATED(""));

static Sys_var_ulong Sys_profiling_history_size(
       "profiling_history_size", "Limit of query profiling memory",
       SESSION_VAR(profiling_history_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 100), DEFAULT(15), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));
#endif

static Sys_var_harows Sys_select_limit(
       "sql_select_limit",
       "The maximum number of rows to return from SELECT statements",
       SESSION_VAR(select_limit), NO_CMD_LINE,
       VALID_RANGE(0, HA_POS_ERROR), DEFAULT(HA_POS_ERROR), BLOCK_SIZE(1));

static bool update_timestamp(THD *thd, set_var *var)
{
  if (var->value)
  {
    double fl= floor(var->save_result.double_value); // Truncate integer part
    struct timeval tmp;
    tmp.tv_sec= static_cast<long>(fl);
    /* Round nanoseconds to nearest microsecond */
    tmp.tv_usec=
      static_cast<long>(rint((var->save_result.double_value - fl) * 1000000));
    thd->set_time(&tmp);
  }
  else // SET timestamp=DEFAULT
  {
    thd->user_time.tv_sec= 0;
    thd->user_time.tv_usec= 0;
  }
  return false;
}
static double read_timestamp(THD *thd)
{
  return (double) thd->start_time.tv_sec +
         (double) thd->start_time.tv_usec / 1000000;
}


static bool check_timestamp(sys_var *self, THD *thd, set_var *var)
{
  double val;

  if (!var->value)
    return FALSE;

  val= var->save_result.double_value;
  if (val != 0 &&          // this is how you set the default value
      (val < TIMESTAMP_MIN_VALUE || val > TIMESTAMP_MAX_VALUE))
  {
    ErrConvString prm(val);
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "timestamp", prm.ptr());
    return TRUE;
  }
  return FALSE;
}


static Sys_var_session_special_double Sys_timestamp(
       "timestamp", "Set the time for this client",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, 0), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_timestamp), 
       ON_UPDATE(update_timestamp), ON_READ(read_timestamp));

static bool update_last_insert_id(THD *thd, set_var *var)
{
  if (!var->value)
  {
    my_error(ER_NO_DEFAULT, MYF(0), var->var->name.str);
    return true;
  }
  thd->first_successful_insert_id_in_prev_stmt=
    var->save_result.ulonglong_value;
  thd->substitute_null_with_insert_id= TRUE;
  return false;
}
static ulonglong read_last_insert_id(THD *thd)
{
  return thd->read_first_successful_insert_id_in_prev_stmt();
}
static Sys_var_session_special Sys_last_insert_id(
       "last_insert_id", "The value to be returned from LAST_INSERT_ID()",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_last_insert_id), ON_READ(read_last_insert_id));

// alias for last_insert_id(), Sybase-style
static Sys_var_session_special Sys_identity(
       "identity", "Synonym for the last_insert_id variable",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_last_insert_id), ON_READ(read_last_insert_id));

/*
  insert_id should *not* be marked as written to the binlog (i.e., it
  should *not* be IN_BINLOG), because we want any statement that
  refers to insert_id explicitly to be unsafe.  (By "explicitly", we
  mean using @@session.insert_id, whereas insert_id is used
  "implicitly" when NULL value is inserted into an auto_increment
  column).

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
static bool update_insert_id(THD *thd, set_var *var)
{
  if (!var->value)
  {
    my_error(ER_NO_DEFAULT, MYF(0), var->var->name.str);
    return true;
  }
  thd->force_one_auto_inc_interval(var->save_result.ulonglong_value);
  return false;
}

static ulonglong read_insert_id(THD *thd)
{
  return thd->auto_inc_intervals_forced.minimum();
}
static Sys_var_session_special Sys_insert_id(
       "insert_id", "The value to be used by the following INSERT "
       "or ALTER TABLE statement when inserting an AUTO_INCREMENT value",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_insert_id), ON_READ(read_insert_id));

static bool update_rand_seed1(THD *thd, set_var *var)
{
  if (!var->value)
  {
    my_error(ER_NO_DEFAULT, MYF(0), var->var->name.str);
    return true;
  }
  thd->rand.seed1= (ulong) var->save_result.ulonglong_value;
  return false;
}
static ulonglong read_rand_seed(THD *thd)
{
  return 0;
}
static Sys_var_session_special Sys_rand_seed1(
       "rand_seed1", "Sets the internal state of the RAND() "
       "generator for replication purposes",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULONG_MAX), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_rand_seed1), ON_READ(read_rand_seed));

static bool update_rand_seed2(THD *thd, set_var *var)
{
  if (!var->value)
  {
    my_error(ER_NO_DEFAULT, MYF(0), var->var->name.str);
    return true;
  }
  thd->rand.seed2= (ulong) var->save_result.ulonglong_value;
  return false;
}
static Sys_var_session_special Sys_rand_seed2(
       "rand_seed2", "Sets the internal state of the RAND() "
       "generator for replication purposes",
       sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULONG_MAX), BLOCK_SIZE(1),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_rand_seed2), ON_READ(read_rand_seed));

static ulonglong read_error_count(THD *thd)
{
  return thd->get_stmt_da()->error_count(thd);
}
// this really belongs to the SHOW STATUS
static Sys_var_session_special Sys_error_count(
       "error_count", "The number of errors that resulted from the "
       "last statement that generated messages",
       READ_ONLY sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), ON_READ(read_error_count));

static ulonglong read_warning_count(THD *thd)
{
  return thd->get_stmt_da()->warn_count(thd);
}
// this really belongs to the SHOW STATUS
static Sys_var_session_special Sys_warning_count(
       "warning_count", "The number of errors, warnings, and notes "
       "that resulted from the last statement that generated messages",
       READ_ONLY sys_var::ONLY_SESSION, NO_CMD_LINE,
       VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), ON_READ(read_warning_count));

static Sys_var_ulong Sys_default_week_format(
       "default_week_format",
       "The default week format used by WEEK() functions",
       SESSION_VAR(default_week_format), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 7), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_ulong Sys_group_concat_max_len(
       "group_concat_max_len",
       "The maximum length of the result of function  GROUP_CONCAT()",
       SESSION_VAR(group_concat_max_len), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(4, ULONG_MAX), DEFAULT(1024), BLOCK_SIZE(1));

static char *glob_hostname_ptr;
static Sys_var_charptr Sys_hostname(
       "hostname", "Server host name",
       READ_ONLY GLOBAL_VAR(glob_hostname_ptr), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(glob_hostname));

#ifndef EMBEDDED_LIBRARY
static Sys_var_charptr Sys_repl_report_host(
       "report_host",
       "Hostname or IP of the slave to be reported to the master during "
       "slave registration. Will appear in the output of SHOW SLAVE HOSTS. "
       "Leave unset if you do not want the slave to register itself with the "
       "master. Note that it is not sufficient for the master to simply read "
       "the IP of the slave off the socket once the slave connects. Due to "
       "NAT and other routing issues, that IP may not be valid for connecting "
       "to the slave from the master or other hosts",
       READ_ONLY GLOBAL_VAR(report_host), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_repl_report_user(
       "report_user",
       "The account user name of the slave to be reported to the master "
       "during slave registration",
       READ_ONLY GLOBAL_VAR(report_user), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_repl_report_password(
       "report_password",
       "The account password of the slave to be reported to the master "
       "during slave registration",
       READ_ONLY GLOBAL_VAR(report_password), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_uint Sys_repl_report_port(
       "report_port",
       "Port for connecting to slave reported to the master during slave "
       "registration. Set it only if the slave is listening on a non-default "
       "port or if you have a special tunnel from the master or other clients "
       "to the slave. If not sure, leave this option unset",
       READ_ONLY GLOBAL_VAR(report_port), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));
#endif

static Sys_var_mybool Sys_keep_files_on_create(
       "keep_files_on_create",
       "Don't overwrite stale .MYD and .MYI even if no directory is specified",
       SESSION_VAR(keep_files_on_create), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE));

static char *license;
static Sys_var_charptr Sys_license(
       "license", "The type of license the server has",
       READ_ONLY GLOBAL_VAR(license), NO_CMD_LINE, IN_SYSTEM_CHARSET,
       DEFAULT(STRINGIFY_ARG(LICENSE)));

static bool check_log_path(sys_var *self, THD *thd, set_var *var)
{
  if (!var->value)
    return false; // DEFAULT is ok

  if (!var->save_result.string_value.str)
    return true;

  if (!is_valid_log_name(var->save_result.string_value.str,
                         var->save_result.string_value.length))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             self->name.str, var->save_result.string_value.str);
    return true;
  }

  if (var->save_result.string_value.length > FN_REFLEN)
  { // path is too long
    my_error(ER_PATH_LENGTH, MYF(0), self->name.str);
    return true;
  }

  char path[FN_REFLEN];
  size_t path_length= unpack_filename(path, var->save_result.string_value.str);

  if (!path_length)
    return true;

  if (!is_filename_allowed(var->save_result.string_value.str, 
                           var->save_result.string_value.length, TRUE))
  {
     my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), 
              self->name.str, var->save_result.string_value.str);
     return true;
  }

  MY_STAT f_stat;

  if (my_stat(path, &f_stat, MYF(0)))
  {
    if (!MY_S_ISREG(f_stat.st_mode) || !(f_stat.st_mode & MY_S_IWRITE))
      return true; // not a regular writable file
    return false;
  }

  (void) dirname_part(path, var->save_result.string_value.str, &path_length);

  if (var->save_result.string_value.length - path_length >= FN_LEN)
  { // filename is too long
      my_error(ER_PATH_LENGTH, MYF(0), self->name.str);
      return true;
  }

  if (!path_length) // no path is good path (remember, relative to datadir)
    return false;

  if (my_access(path, (F_OK|W_OK)))
    return true; // directory is not writable

  return false;
}
static bool fix_general_log_file(sys_var *self, THD *thd, enum_var_type type)
{
  if (!opt_general_logname) // SET ... = DEFAULT
  {
    char buff[FN_REFLEN];
    opt_general_logname= my_strdup(key_memory_LOG_name,
                                   make_query_log_name(buff, QUERY_LOG_GENERAL),
                                   MYF(MY_FAE+MY_WME));
    if (!opt_general_logname)
      return true;
  }
  bool res= false;
  if (opt_general_log)
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    res= query_logger.reopen_log_file(QUERY_LOG_GENERAL);
    mysql_mutex_lock(&LOCK_global_system_variables);
    if (res)
      opt_general_log= false;
  }
  return res;
}
static Sys_var_charptr Sys_general_log_path(
       "general_log_file", "Log connections and queries to given file",
       GLOBAL_VAR(opt_general_logname), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_log_path), ON_UPDATE(fix_general_log_file));

static bool fix_slow_log_file(sys_var *self, THD *thd, enum_var_type type)
{
  if (!opt_slow_logname) // SET ... = DEFAULT
  {
    char buff[FN_REFLEN];
    opt_slow_logname= my_strdup(key_memory_LOG_name,
                                make_query_log_name(buff, QUERY_LOG_SLOW),
                                MYF(MY_FAE+MY_WME));
    if (!opt_slow_logname)
      return true;
  }
  bool res= false;
  if (opt_slow_log)
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    res= query_logger.reopen_log_file(QUERY_LOG_SLOW);
    mysql_mutex_lock(&LOCK_global_system_variables);
    if (res)
      opt_slow_log= false;
  }
  return res;
}
static Sys_var_charptr Sys_slow_log_path(
       "slow_query_log_file", "Log slow queries to given log file. "
       "Defaults logging to hostname-slow.log. Must be enabled to activate "
       "other slow log options",
       GLOBAL_VAR(opt_slow_logname), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_log_path), ON_UPDATE(fix_slow_log_file));

static Sys_var_have Sys_have_compress(
       "have_compress", "have_compress",
       READ_ONLY GLOBAL_VAR(have_compress), NO_CMD_LINE);

static Sys_var_have Sys_have_crypt(
       "have_crypt", "have_crypt",
       READ_ONLY GLOBAL_VAR(have_crypt), NO_CMD_LINE);

static Sys_var_have Sys_have_dlopen(
       "have_dynamic_loading", "have_dynamic_loading",
       READ_ONLY GLOBAL_VAR(have_dlopen), NO_CMD_LINE);

static Sys_var_have Sys_have_geometry(
       "have_geometry", "have_geometry",
       READ_ONLY GLOBAL_VAR(have_geometry), NO_CMD_LINE);

static Sys_var_have Sys_have_openssl(
       "have_openssl", "have_openssl",
       READ_ONLY GLOBAL_VAR(have_ssl), NO_CMD_LINE);

static Sys_var_have Sys_have_profiling(
       "have_profiling", "have_profiling",
       READ_ONLY GLOBAL_VAR(have_profiling), NO_CMD_LINE, NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(0), ON_UPDATE(0), DEPRECATED(""));

static Sys_var_have Sys_have_query_cache(
       "have_query_cache",
       "have_query_cache. "
       "This variable is deprecated and will be removed in a future release.",
       READ_ONLY GLOBAL_VAR(have_query_cache), NO_CMD_LINE,
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL),
       DEPRECATED(""));

static Sys_var_have Sys_have_rtree_keys(
       "have_rtree_keys", "have_rtree_keys",
       READ_ONLY GLOBAL_VAR(have_rtree_keys), NO_CMD_LINE);

static Sys_var_have Sys_have_ssl(
       "have_ssl", "have_ssl",
       READ_ONLY GLOBAL_VAR(have_ssl), NO_CMD_LINE);

static Sys_var_have Sys_have_symlink(
       "have_symlink", "have_symlink",
       READ_ONLY GLOBAL_VAR(have_symlink), NO_CMD_LINE);

static Sys_var_have Sys_have_statement_timeout(
       "have_statement_timeout", "have_statement_timeout",
       READ_ONLY GLOBAL_VAR(have_statement_timeout), NO_CMD_LINE);

static bool fix_general_log_state(sys_var *self, THD *thd, enum_var_type type)
{
  if (query_logger.is_log_file_enabled(QUERY_LOG_GENERAL) == opt_general_log)
    return false;

  if (!opt_general_log)
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    query_logger.deactivate_log_handler(QUERY_LOG_GENERAL);
    mysql_mutex_lock(&LOCK_global_system_variables);
    return false;
  }
  else
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    bool res= query_logger.activate_log_handler(thd, QUERY_LOG_GENERAL);
    mysql_mutex_lock(&LOCK_global_system_variables);
    if (res)
      opt_general_log= false;
    return res;
  }
}
static Sys_var_mybool Sys_general_log(
       "general_log", "Log connections and queries to a table or log file. "
       "Defaults to logging to a file hostname.log, "
       "or if --log-output=TABLE is used, to a table mysql.general_log.",
       GLOBAL_VAR(opt_general_log), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_general_log_state));

static bool fix_slow_log_state(sys_var *self, THD *thd, enum_var_type type)
{
  if (query_logger.is_log_file_enabled(QUERY_LOG_SLOW) == opt_slow_log)
    return false;

  if (!opt_slow_log)
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    query_logger.deactivate_log_handler(QUERY_LOG_SLOW);
    mysql_mutex_lock(&LOCK_global_system_variables);
    return false;
  }
  else
  {
    mysql_mutex_unlock(&LOCK_global_system_variables);
    bool res= query_logger.activate_log_handler(thd, QUERY_LOG_SLOW);
    mysql_mutex_lock(&LOCK_global_system_variables);
    if (res)
      opt_slow_log= false;
    return res;
  }
}
static Sys_var_mybool Sys_slow_query_log(
       "slow_query_log",
       "Log slow queries to a table or log file. Defaults logging to a file "
       "hostname-slow.log or a table mysql.slow_log if --log-output=TABLE is "
       "used. Must be enabled to activate other slow log options",
       GLOBAL_VAR(opt_slow_log), CMD_LINE(OPT_ARG),
       DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_slow_log_state));

static bool check_not_empty_set(sys_var *self, THD *thd, set_var *var)
{
  return var->save_result.ulonglong_value == 0;
}
static bool fix_log_output(sys_var *self, THD *thd, enum_var_type type)
{
  query_logger.set_handlers(static_cast<uint>(log_output_options));
  return false;
}

static const char *log_output_names[] = { "NONE", "FILE", "TABLE", NULL};

static Sys_var_set Sys_log_output(
       "log_output", "Syntax: log-output=value[,value...], "
       "where \"value\" could be TABLE, FILE or NONE",
       GLOBAL_VAR(log_output_options), CMD_LINE(REQUIRED_ARG),
       log_output_names, DEFAULT(LOG_FILE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_not_empty_set), ON_UPDATE(fix_log_output));

#ifdef HAVE_REPLICATION
static Sys_var_mybool Sys_log_slave_updates(
       "log_slave_updates", "Tells the slave to log the updates from "
       "the slave thread to the binary log. You will need to turn it on if "
       "you plan to daisy-chain the slaves",
       READ_ONLY GLOBAL_VAR(opt_log_slave_updates), CMD_LINE(OPT_ARG),
       DEFAULT(0));

static Sys_var_charptr Sys_relay_log(
       "relay_log", "The location and name to use for relay logs",
       READ_ONLY GLOBAL_VAR(opt_relay_logname), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

/*
  Uses NO_CMD_LINE since the --relay-log-index option set
  opt_relaylog_index_name variable and computes a value for the
  relay_log_index variable.
*/
static Sys_var_charptr Sys_relay_log_index(
       "relay_log_index", "The location and name to use for the file "
       "that keeps a list of the last relay logs",
       READ_ONLY GLOBAL_VAR(relay_log_index), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(0));

/*
  Uses NO_CMD_LINE since the --log-bin-index option set
  opt_binlog_index_name variable and computes a value for the
  log_bin_index variable.
*/
static Sys_var_charptr Sys_binlog_index(
       "log_bin_index", "File that holds the names for last binary log files.",
       READ_ONLY GLOBAL_VAR(log_bin_index), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_relay_log_basename(
       "relay_log_basename",
       "The full path of the relay log file names, excluding the extension.",
       READ_ONLY GLOBAL_VAR(relay_log_basename), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_log_bin_basename(
       "log_bin_basename",
       "The full path of the binary log file names, excluding the extension.",
       READ_ONLY GLOBAL_VAR(log_bin_basename), NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_charptr Sys_relay_log_info_file(
       "relay_log_info_file", "The location and name of the file that "
       "remembers where the SQL replication thread is in the relay logs",
       READ_ONLY GLOBAL_VAR(relay_log_info_file), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static Sys_var_mybool Sys_relay_log_purge(
       "relay_log_purge", "if disabled - do not purge relay logs. "
       "if enabled - purge them as soon as they are no more needed",
       GLOBAL_VAR(relay_log_purge), CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static Sys_var_mybool Sys_relay_log_recovery(
       "relay_log_recovery", "Enables automatic relay log recovery "
       "right after the database startup, which means that the IO Thread "
       "starts re-fetching from the master right after the last transaction "
       "processed",
        READ_ONLY GLOBAL_VAR(relay_log_recovery), CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_slave_allow_batching(
       "slave_allow_batching", "Allow slave to batch requests",
       GLOBAL_VAR(opt_slave_allow_batching),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_charptr Sys_slave_load_tmpdir(
       "slave_load_tmpdir", "The location where the slave should put "
       "its temporary files when replicating a LOAD DATA INFILE command",
       READ_ONLY GLOBAL_VAR(slave_load_tmpdir), CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET, DEFAULT(0));

static bool fix_slave_net_timeout(sys_var *self, THD *thd, enum_var_type type)
{
  DEBUG_SYNC(thd, "fix_slave_net_timeout");
  Master_info *mi;


  /* @TODO: slave net timeout is for all channels, but does this make
           sense?
   */

  /*
   Here we have lock on LOCK_global_system_variables and we need
    lock on channel_map lock. In START_SLAVE handler, we take these
    two locks in different order. This can lead to DEADLOCKs. See
    BUG#14236151 for more details.
   So we release lock on LOCK_global_system_variables before acquiring
    lock on channel_map lock. But this could lead to isolation issues
    between multiple setters. Hence introducing secondary guard
    for this global variable and releasing the lock here and acquiring
    locks back again at the end of this function.
   */
  mysql_mutex_unlock(&LOCK_slave_net_timeout);
  mysql_mutex_unlock(&LOCK_global_system_variables);
  channel_map.wrlock();

  for (mi_map::iterator it=channel_map.begin(); it!=channel_map.end(); it++)
  {
    mi= it->second;

    DBUG_PRINT("info", ("slave_net_timeout=%u mi->heartbeat_period=%.3f",
                        slave_net_timeout,
                        (mi ? mi->heartbeat_period : 0.0)));
    if (mi != NULL && slave_net_timeout < mi->heartbeat_period)
      push_warning(thd, Sql_condition::SL_WARNING,
                   ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX,
                   ER(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX));
  }

  channel_map.unlock();
  mysql_mutex_lock(&LOCK_global_system_variables);
  mysql_mutex_lock(&LOCK_slave_net_timeout);
  return false;
}
static PolyLock_mutex PLock_slave_net_timeout(&LOCK_slave_net_timeout);
static Sys_var_uint Sys_slave_net_timeout(
       "slave_net_timeout", "Number of seconds to wait for more data "
       "from a master/slave connection before aborting the read",
       GLOBAL_VAR(slave_net_timeout), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(SLAVE_NET_TIMEOUT), BLOCK_SIZE(1),
       &PLock_slave_net_timeout, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(fix_slave_net_timeout));

static bool check_slave_skip_counter(sys_var *self, THD *thd, set_var *var)
{
  /*
    @todo: move this check into the set function and hold the lock on
    gtid_mode_lock until the operation has completed, so that we are
    sure a concurrent connection does not change gtid_mode between
    check and fix.
  */
  if (get_gtid_mode(GTID_MODE_LOCK_NONE) == GTID_MODE_ON)
  {
    my_message(ER_SQL_SLAVE_SKIP_COUNTER_NOT_SETTABLE_IN_GTID_MODE,
               ER(ER_SQL_SLAVE_SKIP_COUNTER_NOT_SETTABLE_IN_GTID_MODE),
               MYF(0));
    return true;
  }

  return false;
}

static PolyLock_mutex PLock_sql_slave_skip_counter(&LOCK_sql_slave_skip_counter);
static Sys_var_uint Sys_slave_skip_counter(
       "sql_slave_skip_counter", "sql_slave_skip_counter",
       GLOBAL_VAR(sql_slave_skip_counter), NO_CMD_LINE,
       VALID_RANGE(0, UINT_MAX), DEFAULT(0), BLOCK_SIZE(1),
       &PLock_sql_slave_skip_counter, NOT_IN_BINLOG,
       ON_CHECK(check_slave_skip_counter));

static Sys_var_charptr Sys_slave_skip_errors(
       "slave_skip_errors", "Tells the slave thread to continue "
       "replication when a query event returns an error from the "
       "provided list",
       READ_ONLY GLOBAL_VAR(opt_slave_skip_errors), CMD_LINE(REQUIRED_ARG),
       IN_SYSTEM_CHARSET, DEFAULT(0));

static Sys_var_ulonglong Sys_relay_log_space_limit(
       "relay_log_space_limit", "Maximum space to use for all relay logs",
       READ_ONLY GLOBAL_VAR(relay_log_space_limit), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_relaylog_period(
       "sync_relay_log", "Synchronously flush relay log to disk after "
       "every #th event. Use 0 to disable synchronous flushing",
       GLOBAL_VAR(sync_relaylog_period), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX), DEFAULT(10000), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_relayloginfo_period(
       "sync_relay_log_info", "Synchronously flush relay log info "
       "to disk after every #th transaction. Use 0 to disable "
       "synchronous flushing",
       GLOBAL_VAR(sync_relayloginfo_period), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX), DEFAULT(10000), BLOCK_SIZE(1));

static Sys_var_uint Sys_checkpoint_mts_period(
       "slave_checkpoint_period", "Gather workers' activities to "
       "Update progress status of Multi-threaded slave and flush "
       "the relay log info to disk after every #th milli-seconds.",
       GLOBAL_VAR(opt_mts_checkpoint_period), CMD_LINE(REQUIRED_ARG),
#ifndef DBUG_OFF
       VALID_RANGE(0, UINT_MAX), DEFAULT(300), BLOCK_SIZE(1));
#else
       VALID_RANGE(1, UINT_MAX), DEFAULT(300), BLOCK_SIZE(1));
#endif /* DBUG_OFF */

static Sys_var_uint Sys_checkpoint_mts_group(
       "slave_checkpoint_group",
       "Maximum number of processed transactions by Multi-threaded slave "
       "before a checkpoint operation is called to update progress status.",
       GLOBAL_VAR(opt_mts_checkpoint_group), CMD_LINE(REQUIRED_ARG),
#ifndef DBUG_OFF
       VALID_RANGE(1, MTS_MAX_BITS_IN_GROUP), DEFAULT(512), BLOCK_SIZE(1));
#else
       VALID_RANGE(32, MTS_MAX_BITS_IN_GROUP), DEFAULT(512), BLOCK_SIZE(8));
#endif /* DBUG_OFF */
#endif /* HAVE_REPLICATION */

static Sys_var_uint Sys_sync_binlog_period(
       "sync_binlog", "Synchronously flush binary log to disk after"
       " every #th write to the file. Use 0 to disable synchronous"
       " flushing",
       GLOBAL_VAR(sync_binlog_period), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX), DEFAULT(1), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_masterinfo_period(
       "sync_master_info", "Synchronously flush master info to disk "
       "after every #th event. Use 0 to disable synchronous flushing",
       GLOBAL_VAR(sync_masterinfo_period), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, UINT_MAX), DEFAULT(10000), BLOCK_SIZE(1));

#ifdef HAVE_REPLICATION
static Sys_var_ulong Sys_slave_trans_retries(
       "slave_transaction_retries", "Number of times the slave SQL "
       "thread will retry a transaction in case it failed with a deadlock "
       "or elapsed lock wait timeout, before giving up and stopping",
       GLOBAL_VAR(slave_trans_retries), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, ULONG_MAX), DEFAULT(10), BLOCK_SIZE(1));

static Sys_var_ulong Sys_slave_parallel_workers(
       "slave_parallel_workers",
       "Number of worker threads for executing events in parallel ",
       GLOBAL_VAR(opt_mts_slave_parallel_workers), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(0, MTS_MAX_WORKERS), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_ulonglong Sys_mts_pending_jobs_size_max(
       "slave_pending_jobs_size_max",
       "Max size of Slave Worker queues holding yet not applied events."
       "The least possible value must be not less than the master side "
       "max_allowed_packet.",
       GLOBAL_VAR(opt_mts_pending_jobs_size_max), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(1024, (ulonglong)~(intptr)0), DEFAULT(16 * 1024*1024),
       BLOCK_SIZE(1024), ON_CHECK(0));
#endif

static bool check_locale(sys_var *self, THD *thd, set_var *var)
{
  if (!var->value)
    return false;

  MY_LOCALE *locale;
  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == INT_RESULT)
  {
    int lcno= (int)var->value->val_int();
    if (!(locale= my_locale_by_number(lcno)))
    {
      my_error(ER_UNKNOWN_LOCALE, MYF(0), llstr(lcno, buff));
      return true;
    }
    if (check_not_null(self, thd, var))
      return true;
  }
  else // STRING_RESULT
  {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res=var->value->val_str(&str)))
      return true;
    else if (!(locale= my_locale_by_name(res->c_ptr_safe())))
    {
      ErrConvString err(res);
      my_error(ER_UNKNOWN_LOCALE, MYF(0), err.ptr());
      return true;
    }
  }

  var->save_result.ptr= locale;

  if (!locale->errmsgs->is_loaded())
  {
    mysql_mutex_lock(&LOCK_error_messages);
    if (!locale->errmsgs->is_loaded() &&
        locale->errmsgs->read_texts())
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                          "Can't process error message file for locale '%s'",
                          locale->name);
      mysql_mutex_unlock(&LOCK_error_messages);
      return true;
    }
    mysql_mutex_unlock(&LOCK_error_messages);
  }
  return false;
}
static Sys_var_struct Sys_lc_messages(
       "lc_messages", "Set the language used for the error messages",
       SESSION_VAR(lc_messages), NO_CMD_LINE,
       my_offsetof(MY_LOCALE, name), DEFAULT(&my_default_lc_messages),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_locale));

static Sys_var_struct Sys_lc_time_names(
       "lc_time_names", "Set the language used for the month "
       "names and the days of the week",
       SESSION_VAR(lc_time_names), NO_CMD_LINE,
       my_offsetof(MY_LOCALE, name), DEFAULT(&my_default_lc_time_names),
       NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_locale));

static Sys_var_tz Sys_time_zone(
       "time_zone", "time_zone",
       SESSION_VAR(time_zone), NO_CMD_LINE,
       DEFAULT(&default_tz), NO_MUTEX_GUARD, IN_BINLOG);

static bool fix_host_cache_size(sys_var *, THD *, enum_var_type)
{
  hostname_cache_resize(host_cache_size);
  return false;
}

static Sys_var_uint Sys_host_cache_size(
       "host_cache_size",
       "How many host names should be cached to avoid resolving.",
       GLOBAL_VAR(host_cache_size),
       CMD_LINE(REQUIRED_ARG, OPT_HOST_CACHE_SIZE), VALID_RANGE(0, 65536),
       DEFAULT(HOST_CACHE_SIZE),
       BLOCK_SIZE(1),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL),
       ON_UPDATE(fix_host_cache_size));

static Sys_var_charptr Sys_ignore_db_dirs(
       "ignore_db_dirs",
       "The list of directories to ignore when collecting database lists",
       READ_ONLY GLOBAL_VAR(opt_ignore_db_dirs), 
       NO_CMD_LINE,
       IN_FS_CHARSET, DEFAULT(0));

const Sys_var_multi_enum::ALIAS enforce_gtid_consistency_aliases[]=
{
  { "OFF", 0 },
  { "ON", 1 },
  { "WARN", 2 },
  { "FALSE", 0 },
  { "TRUE", 1 },
  { NULL, 0 }
};
static Sys_var_enforce_gtid_consistency Sys_enforce_gtid_consistency(
       "enforce_gtid_consistency",
       "Prevents execution of statements that would be impossible to log "
       "in a transactionally safe manner. Currently, the disallowed "
       "statements include CREATE TEMPORARY TABLE inside transactions, "
       "all updates to non-transactional tables, and CREATE TABLE ... SELECT.",
       GLOBAL_VAR(_gtid_consistency_mode),
       CMD_LINE(OPT_ARG, OPT_ENFORCE_GTID_CONSISTENCY),
       enforce_gtid_consistency_aliases, 3,
       DEFAULT(3/*position of "FALSE" in enforce_gtid_consistency_aliases*/),
       DEFAULT(GTID_CONSISTENCY_MODE_ON),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_super_outside_trx_outside_sf_outside_sp));
const char *fixup_enforce_gtid_consistency_command_line(char *value_arg)
{
  return Sys_enforce_gtid_consistency.fixup_command_line(value_arg);
}

static Sys_var_mybool Sys_binlog_gtid_simple_recovery(
       "binlog_gtid_simple_recovery",
       "If this option is enabled, the server does not open more than "
       "two binary logs when initializing GTID_PURGED and "
       "GTID_EXECUTED, either during server restart or when binary "
       "logs are being purged. Enabling this option is useful when "
       "the server has already generated many binary logs without "
       "GTID events (e.g., having GTID_MODE = OFF). Note: If this "
       "option is enabled, GLOBAL.GTID_EXECUTED and "
       "GLOBAL.GTID_PURGED may be initialized wrongly in two cases: "
       "(1) All binary logs were generated by MySQL 5.7.5 or older, "
       "and GTID_MODE was ON for some binary logs but OFF for the "
       "newest binary log. (2) The oldest existing binary log was "
       "generated by MySQL 5.7.5 or older, and SET GTID_PURGED was "
       "issued after the oldest binary log was generated. If a wrong "
       "set is computed in one of case (1) or case (2), it will "
       "remain wrong even if the server is later restarted with this "
       "option disabled.",
       READ_ONLY GLOBAL_VAR(binlog_gtid_simple_recovery),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE));

static Sys_var_ulong Sys_sp_cache_size(
       "stored_program_cache",
       "The soft upper limit for number of cached stored routines for "
       "one connection.",
       GLOBAL_VAR(stored_program_cache_size), CMD_LINE(REQUIRED_ARG),
       VALID_RANGE(16, 512 * 1024), DEFAULT(256), BLOCK_SIZE(1));

static bool check_pseudo_slave_mode(sys_var *self, THD *thd, set_var *var)
{
  if (check_outside_trx(self, thd, var))
    return true;
  longlong previous_val= thd->variables.pseudo_slave_mode;
  longlong val= (longlong) var->save_result.ulonglong_value;
  bool rli_fake= false;

#ifndef EMBEDDED_LIBRARY
  rli_fake= thd->rli_fake ? true : false;
#endif

  if (rli_fake)
  {
    if (!val)
    {
#ifndef EMBEDDED_LIBRARY
      thd->rli_fake->end_info();
      delete thd->rli_fake;
      thd->rli_fake= NULL;
#endif
    }
    else if (previous_val && val)
      goto ineffective;
    else if (!previous_val && val)
      push_warning(thd, Sql_condition::SL_WARNING,
                   ER_WRONG_VALUE_FOR_VAR,
                   "'pseudo_slave_mode' is already ON.");
  }
  else
  {
    if (!previous_val && !val)
      goto ineffective;
    else if (previous_val && !val)
      push_warning(thd, Sql_condition::SL_WARNING,
                   ER_WRONG_VALUE_FOR_VAR,
                   "Slave applier execution mode not active, "
                   "statement ineffective.");
  }
  goto end;

ineffective:
  push_warning(thd, Sql_condition::SL_WARNING,
               ER_WRONG_VALUE_FOR_VAR,
               "'pseudo_slave_mode' change was ineffective.");

end:
  return FALSE;
}
static Sys_var_mybool Sys_pseudo_slave_mode(
       "pseudo_slave_mode",
       "SET pseudo_slave_mode= 0,1 are commands that mysqlbinlog "
       "adds to beginning and end of binary log dumps. While zero "
       "value indeed disables, the actual enabling of the slave "
       "applier execution mode is done implicitly when a "
       "Format_description_event is sent through the session.",
       SESSION_ONLY(pseudo_slave_mode), NO_CMD_LINE, DEFAULT(FALSE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_pseudo_slave_mode));


#ifdef HAVE_REPLICATION
#ifdef HAVE_GTID_NEXT_LIST
static bool check_gtid_next_list(sys_var *self, THD *thd, set_var *var)
{
  DBUG_ENTER("check_gtid_next_list");
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "GTID_NEXT_LIST");
  if (check_super_outside_trx_outside_sf_outside_sp(self, thd, var))
    DBUG_RETURN(true);
  /*
    @todo: move this check into the set function and hold the lock on
    gtid_mode_lock until the operation has completed, so that we are
    sure a concurrent connection does not change gtid_mode between
    check and fix - if we ever implement this variable.
  */
  if (get_gtid_mode(GTID_MODE_LOCK_NONE) == GTID_MODE_OFF &&
      var->save_result.string_value.str != NULL)
    my_error(ER_CANT_SET_GTID_NEXT_LIST_TO_NON_NULL_WHEN_GTID_MODE_IS_OFF,
             MYF(0));
  DBUG_RETURN(false);
}

static bool update_gtid_next_list(sys_var *self, THD *thd, enum_var_type type)
{
  DBUG_ASSERT(type == OPT_SESSION);
  if (thd->get_gtid_next_list() != NULL)
    return gtid_acquire_ownership_multiple(thd) != 0 ? true : false;
  return false;
}

static Sys_var_gtid_set Sys_gtid_next_list(
       "gtid_next_list",
       "Before re-executing a transaction that contains multiple "
       "Global Transaction Identifiers, this variable must be set "
       "to the set of all re-executed transactions.",
       SESSION_ONLY(gtid_next_list), NO_CMD_LINE,
       DEFAULT(NULL), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_gtid_next_list),
       ON_UPDATE(update_gtid_next_list)
);
export sys_var *Sys_gtid_next_list_ptr= &Sys_gtid_next_list;
#endif //HAVE_GTID_NEXT_LIST

static Sys_var_gtid_next Sys_gtid_next(
       "gtid_next",
       "Specifies the Global Transaction Identifier for the following "
       "transaction.",
       SESSION_ONLY(gtid_next), NO_CMD_LINE,
       DEFAULT("AUTOMATIC"), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_gtid_next));
export sys_var *Sys_gtid_next_ptr= &Sys_gtid_next;

static Sys_var_gtid_executed Sys_gtid_executed(
       "gtid_executed",
       "The global variable contains the set of GTIDs in the "
       "binary log. The session variable contains the set of GTIDs "
       "in the current, ongoing transaction.");

static bool check_gtid_purged(sys_var *self, THD *thd, set_var *var)
{
  DBUG_ENTER("check_gtid_purged");

  if (!var->value ||
      check_super_outside_trx_outside_sf_outside_sp(self, thd, var))
    DBUG_RETURN(true);

  if (var->value->result_type() != STRING_RESULT ||
      !var->save_result.string_value.str)
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

bool Sys_var_gtid_purged::global_update(THD *thd, set_var *var)
{
  DBUG_ENTER("Sys_var_gtid_purged::global_update");
#ifdef HAVE_REPLICATION
  bool error= false;

  global_sid_lock->wrlock();

  /*
    ensures the commit of the transaction started when saving the
    purged gtid set in the table
  */
  thd->lex->autocommit= true;
  /*
    SET GITD_PURGED command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.gtid_executed' replication repository
    table.
  */
  thd->set_skip_readonly_check();
  char *previous_gtid_executed= NULL, *previous_gtid_purged= NULL,
    *current_gtid_executed= NULL, *current_gtid_purged= NULL;
  gtid_state->get_executed_gtids()->to_string(&previous_gtid_executed);
  gtid_state->get_lost_gtids()->to_string(&previous_gtid_purged);
  enum_return_status ret;
  Gtid_set gtid_set(global_sid_map, var->save_result.string_value.str,
                    &ret, global_sid_lock);
  if (ret != RETURN_STATUS_OK)
  {
    global_sid_lock->unlock();
    error= true;
    goto end;
  }
  ret= gtid_state->add_lost_gtids(&gtid_set);
  if (ret != RETURN_STATUS_OK)
  {
    global_sid_lock->unlock();
    error= true;
    goto end;
  }
  gtid_state->get_executed_gtids()->to_string(&current_gtid_executed);
  gtid_state->get_lost_gtids()->to_string(&current_gtid_purged);
  global_sid_lock->unlock();

  // Log messages saying that GTID_PURGED and GTID_EXECUTED were changed.
  sql_print_information(ER(ER_GTID_PURGED_WAS_CHANGED),
                        previous_gtid_purged, current_gtid_purged);
  sql_print_information(ER(ER_GTID_EXECUTED_WAS_CHANGED),
                        previous_gtid_executed, current_gtid_executed);

end:
  my_free(previous_gtid_executed);
  my_free(previous_gtid_purged);
  my_free(current_gtid_executed);
  my_free(current_gtid_purged);
  DBUG_RETURN(error);
#else
  DBUG_RETURN(true);
#endif /* HAVE_REPLICATION */
}

Gtid_set *gtid_purged;
static Sys_var_gtid_purged Sys_gtid_purged(
       "gtid_purged",
       "The set of GTIDs that existed in previous, purged binary logs.",
       GLOBAL_VAR(gtid_purged), NO_CMD_LINE,
       DEFAULT(NULL), NO_MUTEX_GUARD,
       NOT_IN_BINLOG, ON_CHECK(check_gtid_purged));
export sys_var *Sys_gtid_purged_ptr= &Sys_gtid_purged;

static Sys_var_gtid_owned Sys_gtid_owned(
       "gtid_owned",
       "The global variable lists all GTIDs owned by all threads. "
       "The session variable lists all GTIDs owned by the current thread.");

static Sys_var_gtid_mode Sys_gtid_mode(
       "gtid_mode",
       "Controls whether Global Transaction Identifiers (GTIDs) are "
       "enabled. Can be OFF, OFF_PERMISSIVE, ON_PERMISSIVE, or ON. OFF "
       "means that no transaction has a GTID. OFF_PERMISSIVE means that "
       "new transactions (committed in a client session using "
       "GTID_NEXT='AUTOMATIC') are not assigned any GTID, and "
       "replicated transactions are allowed to have or not have a "
       "GTID. ON_PERMISSIVE means that new transactions are assigned a "
       "GTID, and replicated transactions are allowed to have or not "
       "have a GTID. ON means that all transactions have a GTID. "
       "ON is required on a master before any slave can use "
       "MASTER_AUTO_POSITION=1. To safely switch from OFF to ON, first "
       "set all servers to OFF_PERMISSIVE, then set all servers to "
       "ON_PERMISSIVE, then wait for all transactions without a GTID to "
       "be replicated and executed on all servers, and finally set all "
       "servers to GTID_MODE = ON.",
       GLOBAL_VAR(_gtid_mode), CMD_LINE(REQUIRED_ARG), gtid_mode_names,
       DEFAULT(GTID_MODE_OFF), NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(check_super_outside_trx_outside_sf_outside_sp));

#endif // HAVE_REPLICATION

static Sys_var_uint Sys_gtid_executed_compression_period(
       "gtid_executed_compression_period", "When binlog is disabled, "
       "a background thread wakes up to compress the gtid_executed table "
       "every gtid_executed_compression_period transactions, as a "
       "special case, if variable is 0, the thread never wakes up "
       "to compress the gtid_executed table.",
       GLOBAL_VAR(gtid_executed_compression_period),
       CMD_LINE(OPT_ARG), VALID_RANGE(0, UINT_MAX32), DEFAULT(1000),
       BLOCK_SIZE(1));

static Sys_var_mybool Sys_disconnect_on_expired_password(
       "disconnect_on_expired_password",
       "Give clients that don't signal password expiration support execution time error(s) instead of connection error",
       READ_ONLY GLOBAL_VAR(disconnect_on_expired_password),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE));

#ifndef NO_EMBEDDED_ACCESS_CHECKS 
static Sys_var_mybool Sys_validate_user_plugins(
       "validate_user_plugins",
       "Turns on additional validation of authentication plugins assigned "
       "to user accounts. ",
       READ_ONLY NOT_VISIBLE GLOBAL_VAR(validate_user_plugins),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG);
#endif

static Sys_var_enum Sys_block_encryption_mode(
  "block_encryption_mode", "mode for AES_ENCRYPT/AES_DECRYPT",
  SESSION_VAR(my_aes_mode), CMD_LINE(REQUIRED_ARG),
  my_aes_opmode_names, DEFAULT(my_aes_128_ecb));

static bool check_track_session_sys_vars(sys_var *self, THD *thd, set_var *var)
{
  DBUG_ENTER("check_sysvar_change_reporter");
  DBUG_RETURN(thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->check(thd, var));
  DBUG_RETURN(false);
}

static bool update_track_session_sys_vars(sys_var *self, THD *thd,
                                          enum_var_type type)
{
  DBUG_ENTER("check_sysvar_change_reporter");
  /* Populate map only for session variable. */
  if (type == OPT_SESSION)
    DBUG_RETURN(thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->update(thd));
  DBUG_RETURN(false);
}

static Sys_var_charptr Sys_track_session_sys_vars(
       "session_track_system_variables",
       "Track changes in registered system variables.",
       SESSION_VAR(track_sysvars_ptr),
       CMD_LINE(REQUIRED_ARG),
       IN_FS_CHARSET,
       DEFAULT("time_zone,autocommit,character_set_client,character_set_results,"
               "character_set_connection"),
       NO_MUTEX_GUARD,
       NOT_IN_BINLOG,
       ON_CHECK(check_track_session_sys_vars),
       ON_UPDATE(update_track_session_sys_vars)
);

static bool update_session_track_schema(sys_var *self, THD *thd,
                                        enum_var_type type)
{
  DBUG_ENTER("update_session_track_schema");
  DBUG_RETURN(thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->update(thd));
}

static Sys_var_mybool Sys_session_track_schema(
       "session_track_schema",
       "Track changes to the 'default schema'.",
       SESSION_VAR(session_track_schema),
       CMD_LINE(OPT_ARG), DEFAULT(TRUE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0),
       ON_UPDATE(update_session_track_schema));

static bool update_session_track_tx_info(sys_var *self, THD *thd,
                                         enum_var_type type)
{
  DBUG_ENTER("update_session_track_tx_info");
  DBUG_RETURN(thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER)->update(thd));
}

static const char *session_track_transaction_info_names[]=
  { "OFF", "STATE", "CHARACTERISTICS", NullS };

static Sys_var_enum Sys_session_track_transaction_info(
       "session_track_transaction_info",
       "Track changes to the transaction attributes. OFF to disable; "
       "STATE to track just transaction state (Is there an active transaction? "
       "Does it have any data? etc.); CHARACTERISTICS to track transaction "
       "state "
       "and report all statements needed to start a transaction with the same "
       "characteristics (isolation level, read only/read write, snapshot - "
       "but not any work done / data modified within the transaction).",
       SESSION_VAR(session_track_transaction_info),
       CMD_LINE(REQUIRED_ARG), session_track_transaction_info_names,
       DEFAULT(OFF), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(0),
       ON_UPDATE(update_session_track_tx_info));

static bool update_session_track_state_change(sys_var *self, THD *thd,
                                              enum_var_type type)
{
  DBUG_ENTER("update_session_track_state_change");
  DBUG_RETURN(thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->update(thd));
}

static Sys_var_mybool Sys_session_track_state_change(
       "session_track_state_change",
       "Track changes to the 'session state'.",
       SESSION_VAR(session_track_state_change),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       NO_MUTEX_GUARD, NOT_IN_BINLOG,
       ON_CHECK(0),
       ON_UPDATE(update_session_track_state_change));

static bool handle_offline_mode(sys_var *self, THD *thd, enum_var_type type)
{
  DBUG_ENTER("handle_offline_mode");
  if (offline_mode == TRUE)
    killall_non_super_threads(thd);
  DBUG_RETURN(false);
}

static PolyLock_mutex PLock_offline_mode(&LOCK_offline_mode);
static Sys_var_mybool Sys_offline_mode(
       "offline_mode",
       "Make the server into offline mode",
       GLOBAL_VAR(offline_mode), CMD_LINE(OPT_ARG), DEFAULT(FALSE),
       &PLock_offline_mode, NOT_IN_BINLOG,
       ON_CHECK(0), ON_UPDATE(handle_offline_mode));

static Sys_var_mybool Sys_log_builtin_as_identified_by_password(
       "log_builtin_as_identified_by_password",
       "Controls logging of CREATE/ALTER/GRANT and SET PASSWORD user statements "
       "in replication binlogs, general query logs and audit logs.",
       GLOBAL_VAR(opt_log_builtin_as_identified_by_password),
       CMD_LINE(OPT_ARG), DEFAULT(FALSE));

static Sys_var_mybool Sys_avoid_temporal_upgrade(
       "avoid_temporal_upgrade",
       "When this option is enabled, the pre-5.6.4 temporal types are "
       "not upgraded to the new format for ALTER TABLE requests ADD/CHANGE/MODIFY"
       " COLUMN, ADD INDEX or FORCE operation. "
       "This variable is deprecated and will be removed in a future release.",
        GLOBAL_VAR(avoid_temporal_upgrade),
        CMD_LINE(OPT_ARG, OPT_AVOID_TEMPORAL_UPGRADE),
        DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
        ON_CHECK(0), ON_UPDATE(0),
        DEPRECATED(""));

static Sys_var_mybool Sys_show_old_temporals(
       "show_old_temporals",
       "When this option is enabled, the pre-5.6.4 temporal types will "
       "be marked in the 'SHOW CREATE TABLE' and 'INFORMATION_SCHEMA.COLUMNS' "
       "table as a comment in COLUMN_TYPE field. "
       "This variable is deprecated and will be removed in a future release.",
        SESSION_VAR(show_old_temporals),
        CMD_LINE(OPT_ARG, OPT_SHOW_OLD_TEMPORALS),
        DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
        ON_CHECK(0), ON_UPDATE(0),
        DEPRECATED(""));

static Sys_var_charptr Sys_disabled_storage_engines(
       "disabled_storage_engines",
       "Limit CREATE TABLE for the storage engines listed",
       READ_ONLY GLOBAL_VAR(opt_disabled_storage_engines),
       CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
       DEFAULT(""));

static Sys_var_mybool Sys_show_create_table_verbosity(
       "show_create_table_verbosity",
       "When this option is enabled, it increases the verbosity of "
       "'SHOW CREATE TABLE'.",
        SESSION_VAR(show_create_table_verbosity),
        CMD_LINE(OPT_ARG),
        DEFAULT(FALSE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
        ON_CHECK(0), ON_UPDATE(0));

static bool check_keyring_access(sys_var*, THD* thd, set_var*)
{
  if (!(thd->security_context()->check_access(SUPER_ACL)))
  {
    my_error(ER_KEYRING_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER");
    return true;
  }
  return false;
}

/**
  This is a mutex used to protect global variable @@keyring_operations.
*/
static PolyLock_mutex PLock_keyring_operations(&LOCK_keyring_operations);
/**
  This variable provides access to keyring service APIs. When this variable
  is disabled calls to keyring_key_generate(), keyring_key_store() and
  keyring_key_remove() will report error until this variable is enabled.
  This variable is protected under a mutex named PLock_keyring_operations.
  To access this variable you must first set this mutex.

  @sa PLock_keyring_operations
*/
static Sys_var_mybool Sys_keyring_operations(
       "keyring_operations",
       "This variable provides access to keyring service APIs. When this "
       "option is disabled calls to keyring_key_generate(), keyring_key_store() "
       "and keyring_key_remove() will report error until this variable is enabled.",
       GLOBAL_VAR(opt_keyring_operations),
       NO_CMD_LINE, DEFAULT(TRUE),
       &PLock_keyring_operations,
       NOT_IN_BINLOG,
       ON_CHECK(check_keyring_access),
       ON_UPDATE(0));

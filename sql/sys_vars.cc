/* Copyright (c) 2009, 2023, Oracle and/or its affiliates.

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

#include "sql/sys_vars.h"

#include "my_config.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <zlib.h>
#include <atomic>
#include <limits>

#include "include/compression.h"

#include "my_loglevel.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql_com.h"
#include "sql/protocol.h"
#include "sql/rpl_trx_tracking.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <map>
#include <utility>

#include "ft_global.h"
#include "libbinlogevents/include/binlog_event.h"
#include "libbinlogevents/include/binlog_event.h"  // binary_log::max_log_event_size
#include "libbinlogevents/include/compression/zstd_comp.h"  // DEFAULT_COMPRESSION_LEVEL
#include "m_string.h"
#include "my_aes.h"  // my_aes_opmode_names
#include "my_command.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_double2ulonglong.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_thread.h"
#include "my_thread_local.h"
#include "my_time.h"
#include "myisam.h"  // myisam_flush
#include "mysql/plugin_group_replication.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql_version.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // validate_user_plugins
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/changestreams/apply/replication_thread_status.h"
#include "sql/clone_handler.h"
#include "sql/conn_handler/connection_handler_impl.h"  // Per_thread_connection_handler
#include "sql/conn_handler/connection_handler_manager.h"  // Connection_handler_manager
#include "sql/conn_handler/socket_connection.h"  // MY_BIND_ALL_ADDRESSES
#include "sql/derror.h"                          // read_texts
#include "sql/discrete_interval.h"
#include "sql/events.h"          // Events
#include "sql/hostname_cache.h"  // host_cache_resize
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/my_decimal.h"
#include "sql/opt_trace_context.h"
#include "sql/options_mysqld.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/rpl_group_replication.h"  // is_group_replication_running
#include "sql/rpl_handler.h"            // delegates_update_lock_type
#include "sql/rpl_info_factory.h"       // Rpl_info_factory
#include "sql/rpl_info_handler.h"       // INFO_REPOSITORY_TABLE
#include "sql/rpl_log_encryption.h"
#include "sql/rpl_mi.h"                 // Master_info
#include "sql/rpl_msr.h"                // channel_map
#include "sql/rpl_mta_submode.h"        // MTS_PARALLEL_TYPE_DB_NAME
#include "sql/rpl_replica.h"            // SLAVE_THD_TYPE
#include "sql/rpl_rli.h"                // Relay_log_info
#include "sql/rpl_write_set_handler.h"  // transaction_write_set_hashing_algorithms
#include "sql/server_component/log_builtins_filter_imp.h"  // until we have pluggable variables
#include "sql/server_component/log_builtins_imp.h"
#include "sql/session_tracker.h"
#include "sql/sp_head.h"  // SP_PSI_STATEMENT_INFO_COUNT
#include "sql/sql_lex.h"
#include "sql/sql_locale.h"            // my_locale_by_number
#include "sql/sql_parse.h"             // killall_non_super_threads
#include "sql/sql_show_processlist.h"  // pfs_processlist_enabled
#include "sql/sql_tmp_table.h"         // internal_tmp_mem_storage_engine_names
#include "sql/ssl_acceptor_context_operator.h"
#include "sql/system_variables.h"
#include "sql/table_cache.h"  // Table_cache_manager
#include "sql/transaction.h"  // trans_commit_stmt
#include "sql/transaction_info.h"
#include "sql/xa.h"
#include "template_utils.h"  // pointer_cast
#include "thr_lock.h"
#ifdef _WIN32
#include "sql/named_pipe.h"
#endif
#include "my_openssl_fips.h"

#ifdef WITH_LOCK_ORDER
#include "sql/debug_lock_order.h"
#endif /* WITH_LOCK_ORDER */

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/terminology_use_previous.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

static constexpr const unsigned long DEFAULT_ERROR_COUNT{1024};
static constexpr const unsigned long DEFAULT_SORT_MEMORY{256UL * 1024UL};
static constexpr const unsigned HOST_CACHE_SIZE{128};
static constexpr const unsigned long SCHEMA_DEF_CACHE_DEFAULT{256};
static constexpr const unsigned long STORED_PROGRAM_DEF_CACHE_DEFAULT{256};
static constexpr const unsigned long TABLESPACE_DEF_CACHE_DEFAULT{256};

/**
  We must have room for at least 400 table definitions in the table
  cache, since otherwise there is no chance prepared
  statements that use these many tables can work.
  Prepared statements use table definition cache ids (table_map_id)
  as table version identifiers. If the table definition
  cache size is less than the number of tables used in a statement,
  the contents of the table definition cache is guaranteed to rotate
  between a prepare and execute. This leads to stable validation
  errors. In future we shall use more stable version identifiers,
  for now the only solution is to ensure that the table definition
  cache can contain at least all tables of a given statement.
*/
static constexpr const unsigned long TABLE_DEF_CACHE_MIN{400};
static constexpr const unsigned long SCHEMA_DEF_CACHE_MIN{256};
static constexpr const unsigned long STORED_PROGRAM_DEF_CACHE_MIN{256};
static constexpr const unsigned long TABLESPACE_DEF_CACHE_MIN{256};
/*
  Default time to wait before aborting a new client connection
  that does not respond to "initial server greeting" timely
*/
static constexpr const unsigned long CONNECT_TIMEOUT{10};

/* Defaults for deprecated "insert delayed" */
static constexpr const unsigned long DELAYED_LIMIT{100};
static constexpr const unsigned long DELAYED_QUEUE_SIZE{1000};
static constexpr const unsigned long DELAYED_WAIT_TIMEOUT{5 * 60};

static constexpr const unsigned long QUERY_ALLOC_BLOCK_SIZE{8192};
static constexpr const unsigned long QUERY_ALLOC_PREALLOC_SIZE{8192};
static constexpr const unsigned long TRANS_ALLOC_PREALLOC_SIZE{4096};
static constexpr const unsigned long RANGE_ALLOC_BLOCK_SIZE{4096};

// Including the switch in this set, makes its default 'on'
static constexpr const unsigned long long OPTIMIZER_SWITCH_DEFAULT{
    OPTIMIZER_SWITCH_INDEX_MERGE | OPTIMIZER_SWITCH_INDEX_MERGE_UNION |
    OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION |
    OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT |
    OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN |
    OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN | OPTIMIZER_SWITCH_MRR |
    OPTIMIZER_SWITCH_MRR_COST_BASED | OPTIMIZER_SWITCH_BNL |
    OPTIMIZER_SWITCH_MATERIALIZATION | OPTIMIZER_SWITCH_SEMIJOIN |
    OPTIMIZER_SWITCH_LOOSE_SCAN | OPTIMIZER_SWITCH_FIRSTMATCH |
    OPTIMIZER_SWITCH_DUPSWEEDOUT | OPTIMIZER_SWITCH_SUBQ_MAT_COST_BASED |
    OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS |
    OPTIMIZER_SWITCH_COND_FANOUT_FILTER | OPTIMIZER_SWITCH_DERIVED_MERGE |
    OPTIMIZER_SKIP_SCAN | OPTIMIZER_SWITCH_HASH_JOIN |
    OPTIMIZER_SWITCH_PREFER_ORDERING_INDEX |
    OPTIMIZER_SWITCH_DERIVED_CONDITION_PUSHDOWN};

static constexpr const unsigned long MYSQLD_NET_RETRY_COUNT{10};

TYPELIB bool_typelib = {array_elements(bool_values) - 1, "", bool_values,
                        nullptr};

static bool update_buffer_size(THD *, KEY_CACHE *key_cache,
                               ptrdiff_t offset [[maybe_unused]],
                               ulonglong new_value) {
  bool error = false;
  assert(offset == offsetof(KEY_CACHE, param_buff_size));

  if (new_value == 0) {
    if (key_cache == dflt_key_cache) {
      my_error(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE, MYF(0));
      return true;
    }

    if (key_cache->key_cache_inited)  // If initied
    {
      /*
        Move tables using this key cache to the default key cache
        and clear the old key cache.
      */
      key_cache->in_init = true;
      mysql_mutex_unlock(&LOCK_global_system_variables);
      key_cache->param_buff_size = 0;
      ha_resize_key_cache(key_cache);
      ha_change_key_cache(key_cache, dflt_key_cache);
      /*
        We don't delete the key cache as some running threads my still be in
        the key cache code with a pointer to the deleted (empty) key cache
      */
      mysql_mutex_lock(&LOCK_global_system_variables);
      key_cache->in_init = false;
    }
    return error;
  }

  key_cache->param_buff_size = new_value;

  /* If key cache didn't exist initialize it, else resize it */
  key_cache->in_init = true;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (!key_cache->key_cache_inited)
    error = ha_init_key_cache({}, key_cache);
  else
    error = ha_resize_key_cache(key_cache);

  mysql_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init = false;

  return error;
}

static bool update_keycache_param(THD *, KEY_CACHE *key_cache, ptrdiff_t offset,
                                  ulonglong new_value) {
  bool error = false;
  assert(offset != offsetof(KEY_CACHE, param_buff_size));

  keycache_var(key_cache, offset) = new_value;

  key_cache->in_init = true;
  mysql_mutex_unlock(&LOCK_global_system_variables);
  error = ha_resize_key_cache(key_cache);

  mysql_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init = false;

  return error;
}

/**
  Check if REPLICATION_APPLIER granted. Throw SQL error if not.

  Use this when setting session variables that are to be protected within
  replication applier context.

  @note For compatibility we also accept SUPER.

  @retval true failure
  @retval false success

  @param self the system variable to set value for
  @param thd the session context
  @param setv the SET operations metadata
 */
static bool check_session_admin_or_replication_applier(sys_var *self
                                                       [[maybe_unused]],
                                                       THD *thd,
                                                       set_var *setv) {
  assert(self->scope() != sys_var::GLOBAL);
  Security_context *sctx = thd->security_context();
  if ((setv->type == OPT_SESSION || setv->type == OPT_DEFAULT) &&
      !sctx->has_global_grant(STRING_WITH_LEN("REPLICATION_APPLIER")).first &&
      !sctx->has_global_grant(STRING_WITH_LEN("SESSION_VARIABLES_ADMIN"))
           .first &&
      !sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
           .first &&
      !sctx->check_access(SUPER_ACL)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or "
             "REPLICATION_APPLIER");
    return true;
  }
  return false;
}

/**
  Utility method that checks if user has correct session administrative
  dynamic privileges.
  @return 0 on success, 1 on failure.
*/
static bool check_session_admin_privileges_only(sys_var *self [[maybe_unused]],
                                                THD *thd, set_var *setv) {
  // Privilege check for global variable must have already done before.
  assert(self->scope() != sys_var::GLOBAL);
  Security_context *sctx = thd->security_context();
  if ((setv->type == OPT_SESSION || setv->type == OPT_DEFAULT) &&
      !sctx->has_global_grant(STRING_WITH_LEN("SESSION_VARIABLES_ADMIN"))
           .first &&
      !sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
           .first) {
    return true;
  }
  return false;
}

/**
  Check if SESSION_VARIABLES_ADMIN granted. Throw SQL error if not.

  Use this when setting session variables that are sensitive and should
  be protected.

  We also accept SYSTEM_VARIABLES_ADMIN since it doesn't make a lot of
  sense to be allowed to set the global variable and not the session ones.

  @retval true failure
  @retval false success

  @param self the system variable to set value for
  @param thd the session context
  @param setv the SET operations metadata
 */
static bool check_session_admin_no_super(sys_var *self, THD *thd,
                                         set_var *setv) {
  if (check_session_admin_privileges_only(self, thd, setv)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN");
    return true;
  }
  return false;
}

/**
  Check if SESSION_VARIABLES_ADMIN granted. Throw SQL error if not.

  Use this when setting session variables that are sensitive and should
  be protected.

  We also accept SYSTEM_VARIABLES_ADMIN since it doesn't make a lot of
  sense to be allowed to set the global variable and not the session ones.

  @note For compatibility we also accept SUPER.

  @retval true failure
  @retval false success

  @param self the system variable to set value for
  @param thd the session context
  @param setv the SET operations metadata
 */
static bool check_session_admin(sys_var *self, THD *thd, set_var *setv) {
  Security_context *sctx = thd->security_context();
  if (check_session_admin_privileges_only(self, thd, setv) &&
      !sctx->check_access(SUPER_ACL)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN");
    return true;
  }
  return false;
}

/*
  The rule for this file: everything should be 'static'. When a sys_var
  variable or a function from this file is - in very rare cases - needed
  elsewhere it should be explicitly declared 'export' here to show that it's
  not a mistakenly forgotten 'static' keyword.
*/
#define export /* not static */

#ifdef WITH_LOCK_ORDER

#define LO_TRAILING_PROPERTIES                                          \
  NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL), NULL, \
      sys_var::PARSE_EARLY

static Sys_var_bool Sys_lo_enabled("lock_order", "Enable the lock order.",
                                   READ_ONLY GLOBAL_VAR(lo_param.m_enabled),
                                   CMD_LINE(OPT_ARG), DEFAULT(false),
                                   LO_TRAILING_PROPERTIES);

static Sys_var_charptr Sys_lo_out_dir("lock_order_output_directory",
                                      "Lock order output directory.",
                                      READ_ONLY GLOBAL_VAR(lo_param.m_out_dir),
                                      CMD_LINE(OPT_ARG), IN_FS_CHARSET,
                                      DEFAULT(nullptr), LO_TRAILING_PROPERTIES);

static Sys_var_charptr Sys_lo_dep_1(
    "lock_order_dependencies", "Lock order dependencies file.",
    READ_ONLY GLOBAL_VAR(lo_param.m_dependencies_1), CMD_LINE(OPT_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), LO_TRAILING_PROPERTIES);

static Sys_var_charptr Sys_lo_dep_2(
    "lock_order_extra_dependencies", "Lock order extra dependencies file.",
    READ_ONLY GLOBAL_VAR(lo_param.m_dependencies_2), CMD_LINE(OPT_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_print_txt("lock_order_print_txt",
                                     "Print the lock_order.txt file.",
                                     READ_ONLY GLOBAL_VAR(lo_param.m_print_txt),
                                     CMD_LINE(OPT_ARG), DEFAULT(false),
                                     LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_trace_loop(
    "lock_order_trace_loop", "Enable tracing for all loops.",
    READ_ONLY GLOBAL_VAR(lo_param.m_trace_loop), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_debug_loop(
    "lock_order_debug_loop", "Enable debugging for all loops.",
    READ_ONLY GLOBAL_VAR(lo_param.m_debug_loop), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_trace_missing_arc(
    "lock_order_trace_missing_arc", "Enable tracing for all missing arcs.",
    READ_ONLY GLOBAL_VAR(lo_param.m_trace_missing_arc), CMD_LINE(OPT_ARG),
    DEFAULT(true), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_debug_missing_arc(
    "lock_order_debug_missing_arc", "Enable debugging for all missing arcs.",
    READ_ONLY GLOBAL_VAR(lo_param.m_debug_missing_arc), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_trace_missing_unlock(
    "lock_order_trace_missing_unlock", "Enable tracing for all missing unlocks",
    READ_ONLY GLOBAL_VAR(lo_param.m_trace_missing_unlock), CMD_LINE(OPT_ARG),
    DEFAULT(true), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_debug_missing_unlock(
    "lock_order_debug_missing_unlock",
    "Enable debugging for all missing unlocks",
    READ_ONLY GLOBAL_VAR(lo_param.m_debug_missing_unlock), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_trace_missing_key(
    "lock_order_trace_missing_key",
    "Enable trace for missing performance schema keys",
    READ_ONLY GLOBAL_VAR(lo_param.m_trace_missing_key), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

static Sys_var_bool Sys_lo_debug_missing_key(
    "lock_order_debug_missing_key",
    "Enable debugging for missing performance schema keys",
    READ_ONLY GLOBAL_VAR(lo_param.m_debug_missing_key), CMD_LINE(OPT_ARG),
    DEFAULT(false), LO_TRAILING_PROPERTIES);

#endif /* WITH_LOCK_ORDER */

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE

#define PFS_TRAILING_PROPERTIES                                         \
  NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL), NULL, \
      sys_var::PARSE_EARLY

static Sys_var_bool Sys_pfs_enabled("performance_schema",
                                    "Enable the performance schema.",
                                    READ_ONLY GLOBAL_VAR(pfs_param.m_enabled),
                                    CMD_LINE(OPT_ARG), DEFAULT(true),
                                    PFS_TRAILING_PROPERTIES);

static Sys_var_charptr Sys_pfs_instrument(
    "performance_schema_instrument",
    "Default startup value for a performance schema instrument.",
    READ_ONLY NOT_VISIBLE GLOBAL_VAR(pfs_param.m_pfs_instrument),
    CMD_LINE(OPT_ARG, OPT_PFS_INSTRUMENT), IN_FS_CHARSET, DEFAULT(""),
    PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_processlist(
    "performance_schema_show_processlist",
    "Default startup value to enable SHOW PROCESSLIST "
    "in the performance schema.",
    GLOBAL_VAR(pfs_processlist_enabled), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    nullptr, sys_var::PARSE_NORMAL);

static Sys_var_bool Sys_pfs_consumer_events_stages_current(
    "performance_schema_consumer_events_stages_current",
    "Default startup value for the events_stages_current consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_stages_current_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_stages_history(
    "performance_schema_consumer_events_stages_history",
    "Default startup value for the events_stages_history consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_stages_history_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_stages_history_long(
    "performance_schema_consumer_events_stages_history_long",
    "Default startup value for the events_stages_history_long consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_stages_history_long_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_statements_cpu(
    "performance_schema_consumer_events_statements_cpu",
    "Default startup value for the events_statements_cpu consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_statements_cpu_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_statements_current(
    "performance_schema_consumer_events_statements_current",
    "Default startup value for the events_statements_current consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_statements_current_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_statements_history(
    "performance_schema_consumer_events_statements_history",
    "Default startup value for the events_statements_history consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_statements_history_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_statements_history_long(
    "performance_schema_consumer_events_statements_history_long",
    "Default startup value for the events_statements_history_long consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_statements_history_long_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_transactions_current(
    "performance_schema_consumer_events_transactions_current",
    "Default startup value for the events_transactions_current consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_transactions_current_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_transactions_history(
    "performance_schema_consumer_events_transactions_history",
    "Default startup value for the events_transactions_history consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_transactions_history_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_transactions_history_long(
    "performance_schema_consumer_events_transactions_history_long",
    "Default startup value for the events_transactions_history_long consumer.",
    READ_ONLY NOT_VISIBLE GLOBAL_VAR(
        pfs_param.m_consumer_events_transactions_history_long_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_waits_current(
    "performance_schema_consumer_events_waits_current",
    "Default startup value for the events_waits_current consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_waits_current_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_waits_history(
    "performance_schema_consumer_events_waits_history",
    "Default startup value for the events_waits_history consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_waits_history_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_events_waits_history_long(
    "performance_schema_consumer_events_waits_history_long",
    "Default startup value for the events_waits_history_long consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_events_waits_history_long_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(false), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_global_instrumentation(
    "performance_schema_consumer_global_instrumentation",
    "Default startup value for the global_instrumentation consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_global_instrumentation_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_thread_instrumentation(
    "performance_schema_consumer_thread_instrumentation",
    "Default startup value for the thread_instrumentation consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_thread_instrumentation_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_bool Sys_pfs_consumer_statement_digest(
    "performance_schema_consumer_statements_digest",
    "Default startup value for the statements_digest consumer.",
    READ_ONLY NOT_VISIBLE
        GLOBAL_VAR(pfs_param.m_consumer_statement_digest_enabled),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_waits_history_long_size(
    "performance_schema_events_waits_history_long_size",
    "Number of rows in EVENTS_WAITS_HISTORY_LONG."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_waits_history_long_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_waits_history_size(
    "performance_schema_events_waits_history_size",
    "Number of rows per thread in EVENTS_WAITS_HISTORY."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_waits_history_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024), DEFAULT(PFS_AUTOSIZE_VALUE),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_cond_classes(
    "performance_schema_max_cond_classes",
    "Maximum number of condition instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_cond_class_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_COND_CLASS), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_cond_instances(
    "performance_schema_max_cond_instances",
    "Maximum number of instrumented condition objects."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_cond_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_program_instances(
    "performance_schema_max_program_instances",
    "Maximum number of instrumented programs."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_program_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static constexpr int num_prepared_stmt_limit = 4 * 1024 * 1024;

static Sys_var_long Sys_pfs_max_prepared_stmt_instances(
    "performance_schema_max_prepared_statements_instances",
    "Maximum number of instrumented prepared statements."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_prepared_stmt_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, num_prepared_stmt_limit),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_file_classes(
    "performance_schema_max_file_classes",
    "Maximum number of file instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_file_class_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_FILE_CLASS), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_file_handles(
    "performance_schema_max_file_handles",
    "Maximum number of opened instrumented files.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_file_handle_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024 * 1024),
    DEFAULT(PFS_MAX_FILE_HANDLE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_file_instances(
    "performance_schema_max_file_instances",
    "Maximum number of instrumented files."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_file_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_sockets(
    "performance_schema_max_socket_instances",
    "Maximum number of opened instrumented sockets."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_socket_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_socket_classes(
    "performance_schema_max_socket_classes",
    "Maximum number of socket instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_socket_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_SOCKET_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_mutex_classes(
    "performance_schema_max_mutex_classes",
    "Maximum number of mutex instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_mutex_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_MUTEX_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_mutex_instances(
    "performance_schema_max_mutex_instances",
    "Maximum number of instrumented MUTEX objects."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_mutex_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 100 * 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_rwlock_classes(
    "performance_schema_max_rwlock_classes",
    "Maximum number of rwlock instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_rwlock_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_RWLOCK_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_rwlock_instances(
    "performance_schema_max_rwlock_instances",
    "Maximum number of instrumented RWLOCK objects."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_rwlock_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 100 * 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_handles(
    "performance_schema_max_table_handles",
    "Maximum number of opened instrumented tables."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_table_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_instances(
    "performance_schema_max_table_instances",
    "Maximum number of instrumented tables."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_table_share_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_table_lock_stat(
    "performance_schema_max_table_lock_stat",
    "Maximum number of lock statistics for instrumented tables."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_table_lock_stat_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_index_stat(
    "performance_schema_max_index_stat",
    "Maximum number of index statistics for instrumented tables."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_index_stat_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_thread_classes(
    "performance_schema_max_thread_classes",
    "Maximum number of thread instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_thread_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_THREAD_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_thread_instances(
    "performance_schema_max_thread_instances",
    "Maximum number of instrumented threads."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_thread_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_setup_actors_size(
    "performance_schema_setup_actors_size",
    "Maximum number of rows in SETUP_ACTORS."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_setup_actor_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_setup_objects_size(
    "performance_schema_setup_objects_size",
    "Maximum number of rows in SETUP_OBJECTS."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_setup_object_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_accounts_size(
    "performance_schema_accounts_size",
    "Maximum number of instrumented user@host accounts."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_account_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_hosts_size(
    "performance_schema_hosts_size",
    "Maximum number of instrumented hosts."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_host_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_users_size(
    "performance_schema_users_size",
    "Maximum number of instrumented users."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_user_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_stage_classes(
    "performance_schema_max_stage_classes",
    "Maximum number of stage instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_stage_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_STAGE_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_stages_history_long_size(
    "performance_schema_events_stages_history_long_size",
    "Number of rows in EVENTS_STAGES_HISTORY_LONG."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_stages_history_long_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_stages_history_size(
    "performance_schema_events_stages_history_size",
    "Number of rows per thread in EVENTS_STAGES_HISTORY."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_stages_history_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024), DEFAULT(PFS_AUTOSIZE_VALUE),
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
  - CLONE_PSI_STATEMENT_COUNT for "statement/clone/...".
  - 1 for "statement/rpl/relay_log", for replicated statements.
  - 1 for "statement/scheduler/event", for scheduled events.
*/
static Sys_var_ulong Sys_pfs_max_statement_classes(
    "performance_schema_max_statement_classes",
    "Maximum number of statement instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_statement_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 256),
    DEFAULT((ulong)SQLCOM_END + (ulong)COM_END + 5 +
            SP_PSI_STATEMENT_INFO_COUNT + CLONE_PSI_STATEMENT_COUNT),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_statements_history_long_size(
    "performance_schema_events_statements_history_long_size",
    "Number of rows in EVENTS_STATEMENTS_HISTORY_LONG."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_statements_history_long_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_statements_history_size(
    "performance_schema_events_statements_history_size",
    "Number of rows per thread in EVENTS_STATEMENTS_HISTORY."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_statements_history_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024), DEFAULT(PFS_AUTOSIZE_VALUE),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_statement_stack_size(
    "performance_schema_max_statement_stack",
    "Number of rows per thread in EVENTS_STATEMENTS_CURRENT.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_statement_stack_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(1, 256),
    DEFAULT(PFS_STATEMENTS_STACK_SIZE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_memory_classes(
    "performance_schema_max_memory_classes",
    "Maximum number of memory pool instruments.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_memory_class_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024), DEFAULT(PFS_MAX_MEMORY_CLASS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_digest_size(
    "performance_schema_digests_size",
    "Size of the statement digest."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_digest_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(-1, 1024 * 1024), DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_transactions_history_long_size(
    "performance_schema_events_transactions_history_long_size",
    "Number of rows in EVENTS_TRANSACTIONS_HISTORY_LONG."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_transactions_history_long_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_events_transactions_history_size(
    "performance_schema_events_transactions_history_size",
    "Number of rows per thread in EVENTS_TRANSACTIONS_HISTORY."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_events_transactions_history_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024), DEFAULT(PFS_AUTOSIZE_VALUE),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_digest_length(
    "performance_schema_max_digest_length",
    "Maximum length considered for digest text, when stored in "
    "performance_schema tables.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_max_digest_length), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024 * 1024), DEFAULT(1024), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_ulong Sys_pfs_max_digest_sample_age(
    "performance_schema_max_digest_sample_age",
    "The time in seconds after which a previous query sample is considered old."
    " When the value is 0, queries are sampled once."
    " When the value is greater than zero, queries are re sampled if the"
    " last sample is more than performance_schema_max_digest_sample_age "
    "seconds old.",
    GLOBAL_VAR(pfs_param.m_max_digest_sample_age), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024 * 1024), DEFAULT(60), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_connect_attrs_size(
    "performance_schema_session_connect_attrs_size",
    "Size of session attribute string buffer per thread."
    " Use 0 to disable, -1 for automated sizing.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_session_connect_attrs_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 1024 * 1024),
    DEFAULT(PFS_AUTOSIZE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_metadata_locks(
    "performance_schema_max_metadata_locks",
    "Maximum number of metadata locks."
    " Use 0 to disable, -1 for automated scaling.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_metadata_lock_sizing),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(-1, 100 * 1024 * 1024),
    DEFAULT(PFS_AUTOSCALE_VALUE), BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_max_sql_text_length(
    "performance_schema_max_sql_text_length",
    "Maximum length of displayed sql text.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_max_sql_text_length),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 1024 * 1024), DEFAULT(1024),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

static Sys_var_long Sys_pfs_error_size(
    "performance_schema_error_size", "Number of server errors instrumented.",
    READ_ONLY GLOBAL_VAR(pfs_param.m_error_sizing), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024 * 1024), DEFAULT(PFS_MAX_GLOBAL_SERVER_ERRORS),
    BLOCK_SIZE(1), PFS_TRAILING_PROPERTIES);

#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

static Sys_var_ulong Sys_auto_increment_increment(
    "auto_increment_increment",
    "Auto-increment columns are incremented by this",
    HINT_UPDATEABLE SESSION_VAR(auto_increment_increment), CMD_LINE(OPT_ARG),
    VALID_RANGE(1, 65535), DEFAULT(1), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    IN_BINLOG);

static Sys_var_ulong Sys_auto_increment_offset(
    "auto_increment_offset",
    "Offset added to Auto-increment columns. Used when "
    "auto-increment-increment != 1",
    HINT_UPDATEABLE SESSION_VAR(auto_increment_offset), CMD_LINE(OPT_ARG),
    VALID_RANGE(1, 65535), DEFAULT(1), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    IN_BINLOG);

static Sys_var_bool Sys_windowing_use_high_precision(
    "windowing_use_high_precision",
    "For SQL window functions, determines whether to enable inversion "
    "optimization for moving window frames also for floating values.",
    HINT_UPDATEABLE SESSION_VAR(windowing_use_high_precision),
    CMD_LINE(OPT_ARG), DEFAULT(true));

static Sys_var_uint Sys_cte_max_recursion_depth(
    "cte_max_recursion_depth",
    "Abort a recursive common table expression "
    "if it does more than this number of iterations.",
    HINT_UPDATEABLE SESSION_VAR(cte_max_recursion_depth),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, UINT_MAX32), DEFAULT(1000),
    BLOCK_SIZE(1));

static Sys_var_bool Sys_automatic_sp_privileges(
    "automatic_sp_privileges",
    "Creating and dropping stored procedures alters ACLs",
    GLOBAL_VAR(sp_automatic_privileges), CMD_LINE(OPT_ARG), DEFAULT(true));

static Sys_var_ulong Sys_back_log(
    "back_log",
    "The number of outstanding connection requests "
    "MySQL can have. This comes into play when the main MySQL thread "
    "gets very many connection requests in a very short time",
    READ_ONLY GLOBAL_VAR(back_log), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_charptr Sys_basedir(
    "basedir",
    "Path to installation directory. All paths are "
    "usually resolved relative to this",
    READ_ONLY NON_PERSIST GLOBAL_VAR(mysql_home_ptr),
    CMD_LINE(REQUIRED_ARG, 'b'), IN_FS_CHARSET, DEFAULT(nullptr));

/*
  --authentication_policy will take precedence over this variable
  except in case where plugin name for first factor is not a concrete
  value. Please refer authentication_policy variable.
*/
static Sys_var_charptr Sys_default_authentication_plugin(
    "default_authentication_plugin",
    "The default authentication plugin "
    "used by the server to hash the password.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(default_auth_plugin),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT("caching_sha2_password"),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR("authentication_policy"));

static PolyLock_mutex Plock_default_password_lifetime(
    &LOCK_default_password_lifetime);
static Sys_var_uint Sys_default_password_lifetime(
    "default_password_lifetime",
    "The number of days after which the "
    "password will expire.",
    GLOBAL_VAR(default_password_lifetime), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX16), DEFAULT(0), BLOCK_SIZE(1),
    &Plock_default_password_lifetime);

static Sys_var_charptr Sys_my_bind_addr(
    "bind_address",
    "IP address(es) to bind to. Syntax: address[,address]...,"
    " where address can be an IPv4 address, IPv6 address,"
    " host name or one of the wildcard values *, ::, 0.0.0.0."
    " In case more than one address is specified in a"
    " comma-separated list, wildcard values are not allowed."
    " Every address can have optional network namespace separated"
    " by the delimiter / from the address value. E.g., the following value"
    " 192.168.1.1/red,172.16.1.1/green,193.168.1.1 specifies three IP"
    " addresses to listen for incoming TCP connections two of that have"
    " to be placed in corresponding namespaces: the address 192.168.1.1"
    " must be placed into the namespace red and the address 172.16.1.1"
    " must be placed into the namespace green. Using of network namespace"
    " requires its support from underlying Operating System. Attempt to specify"
    " a network namespace for a platform that doesn't support it results in"
    " error during socket creation.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(my_bind_addr_str), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(MY_BIND_ALL_ADDRESSES));

static Sys_var_charptr Sys_admin_addr(
    "admin_address",
    "IP address to bind to for service connection. Address can be an IPv4"
    " address, IPv6 address, or host name. Wildcard values *, ::, 0.0.0.0"
    " are not allowed. Address value can have following optional network"
    " namespace separated by the delimiter / from the address value."
    " E.g., the following value 192.168.1.1/red specifies IP addresses to"
    " listen for incoming TCP connections that have to be placed into"
    " the namespace 'red'. Using of network namespace requires its support"
    " from underlying Operating System. Attempt to specify a network namespace"
    " for a platform that doesn't support it results in error during socket"
    " creation.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(my_admin_bind_addr_str),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_uint Sys_admin_port(
    "admin_port",
    "Port number to use for service connection,"
    " built-in default (" STRINGIFY_ARG(MYSQL_ADMIN_PORT) ")",
    READ_ONLY NON_PERSIST GLOBAL_VAR(mysqld_admin_port), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 65535), DEFAULT(MYSQL_ADMIN_PORT), BLOCK_SIZE(1));

static Sys_var_bool Sys_use_separate_thread_for_admin(
    "create_admin_listener_thread",
    "Use a dedicated thread for listening incoming connections on admin"
    " interface",
    READ_ONLY NON_PERSIST GLOBAL_VAR(listen_admin_interface_in_separate_thread),
    CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_bool Sys_password_require_current(
    "password_require_current",
    "Current password is needed to be specified in order to change it",
    GLOBAL_VAR(password_require_current), CMD_LINE(OPT_ARG), DEFAULT(false));

/**
  Checks,
  if there exists at least a partial revoke on a database at the time
  of turning OFF the system variable "@@partial_revokes". If it does then
  throw error.
  if there exists at least a DB grant with wildcard entry at the time of
  turning ON the system variable "@@partial_revokes". If it does then
  throw error.

  @retval true failure
  @retval false success

  @param self the system variable to set value for
  @param thd the session context
  @param setv the SET operations metadata
*/
static bool check_partial_revokes(sys_var *self, THD *thd, set_var *setv) {
  if (is_partial_revoke_exists(thd) && setv->save_result.ulonglong_value == 0) {
    my_error(ER_PARTIAL_REVOKES_EXIST, MYF(0), self->name.str);
    return true;
  }
  return false;
}

/**
  Set the updated global variable to the corresponding atomic system variable.
*/
static bool partial_revokes_update(sys_var *, THD *, enum_var_type) {
  set_mysqld_partial_revokes(opt_partial_revokes);
  return false;
}

/**
  We also modify the global variable outside of sys_var structure.
  Protect the global variable updates through this lock.
*/
static PolyLock_mutex Plock_partial_revokes(&LOCK_partial_revokes);

static Sys_var_bool Sys_partial_revokes(
    "partial_revokes",
    "Access of database objects can be restricted, "
    "even if user has global privileges granted.",
    GLOBAL_VAR(opt_partial_revokes), CMD_LINE(OPT_ARG),
    DEFAULT(DEFAULT_PARTIAL_REVOKES), &Plock_partial_revokes, IN_BINLOG,
    ON_CHECK(check_partial_revokes), ON_UPDATE(partial_revokes_update), nullptr,
    sys_var::PARSE_EARLY);

static bool fix_binlog_cache_size(sys_var *, THD *thd, enum_var_type) {
  check_binlog_cache_size(thd);
  return false;
}

static bool fix_binlog_stmt_cache_size(sys_var *, THD *thd, enum_var_type) {
  check_binlog_stmt_cache_size(thd);
  return false;
}

static Sys_var_ulong Sys_binlog_cache_size(
    "binlog_cache_size",
    "The size of the transactional cache for "
    "updates to transactional engines for the binary log. "
    "If you often use transactions containing many statements, "
    "you can increase this to get more performance",
    GLOBAL_VAR(binlog_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE, ULONG_MAX), DEFAULT(32768), BLOCK_SIZE(IO_SIZE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_binlog_cache_size));

static Sys_var_ulong Sys_binlog_stmt_cache_size(
    "binlog_stmt_cache_size",
    "The size of the statement cache for "
    "updates to non-transactional engines for the binary log. "
    "If you often use statements updating a great number of rows, "
    "you can increase this to get more performance",
    GLOBAL_VAR(binlog_stmt_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE, ULONG_MAX), DEFAULT(32768), BLOCK_SIZE(IO_SIZE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_binlog_stmt_cache_size));

static Sys_var_int32 Sys_binlog_max_flush_queue_time(
    "binlog_max_flush_queue_time",
    "The maximum time that the binary log group commit will keep reading"
    " transactions before it flush the transactions to the binary log (and"
    " optionally sync, depending on the value of sync_binlog).",
    GLOBAL_VAR(opt_binlog_max_flush_queue_time),
    CMD_LINE(REQUIRED_ARG, OPT_BINLOG_MAX_FLUSH_QUEUE_TIME),
    VALID_RANGE(0, 100000), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_long Sys_binlog_group_commit_sync_delay(
    "binlog_group_commit_sync_delay",
    "The number of microseconds the server waits for the "
    "binary log group commit sync queue to fill before "
    "continuing. Default: 0. Min: 0. Max: 1000000.",
    GLOBAL_VAR(opt_binlog_group_commit_sync_delay), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1000000 /* max 1 sec */), DEFAULT(0), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG);

static Sys_var_ulong Sys_binlog_group_commit_sync_no_delay_count(
    "binlog_group_commit_sync_no_delay_count",
    "If there are this many transactions in the commit sync "
    "queue and the server is waiting for more transactions "
    "to be enqueued (as set using --binlog-group-commit-sync-delay), "
    "the commit procedure resumes.",
    GLOBAL_VAR(opt_binlog_group_commit_sync_no_delay_count),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 100000 /* max connections */),
    DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG);

static bool check_outside_trx(sys_var *var, THD *thd, set_var *) {
  if (thd->in_active_multi_stmt_transaction()) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), var->name.str);
    return true;
  }
  if (!thd->owned_gtid_is_empty()) {
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    if (thd->owned_gtid.sidno > 0)
      thd->owned_gtid.to_string(thd->owned_sid, buf);
    else
      strcpy(buf, "ANONYMOUS");
    my_error(ER_CANT_SET_VARIABLE_WHEN_OWNING_GTID, MYF(0), var->name.str, buf);
    return true;
  }
  return false;
}

static bool check_session_admin_outside_trx_outside_sf(sys_var *self, THD *thd,
                                                       set_var *var) {
  if (thd->in_sub_stmt) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), self->name.str);
    return true;
  }
  if (check_outside_trx(self, thd, var)) return true;
  if (self->scope() != sys_var::GLOBAL)
    return check_session_admin(self, thd, var);
  return false;
}

static bool check_explicit_defaults_for_timestamp(sys_var *self, THD *thd,
                                                  set_var *var) {
  // Deprecation warning if switching OFF explicit_defaults_for_timestamp
  if (thd->variables.explicit_defaults_for_timestamp) {
    if (!var->save_result.ulonglong_value)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX,
                          ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
                          self->name.str);
  }
  if (thd->in_sub_stmt) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), self->name.str);
    return true;
  }
  if (thd->in_active_multi_stmt_transaction()) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), self->name.str);
    return true;
  }
  return false;
}

/**
  Check-function to @@GTID_NEXT system variable.

  @param self   a pointer to the sys_var, i.e. gtid_next
  @param thd    a reference to THD object
  @param var    a pointer to the set_var created by the parser.

  @return @c false if the change is allowed, otherwise @c true.
*/

static bool check_gtid_next(sys_var *self, THD *thd, set_var *var) {
  bool is_prepared_trx =
      thd->get_transaction()->xid_state()->has_state(XID_STATE::XA_PREPARED);

  if (thd->in_sub_stmt) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, MYF(0), self->name.str);
    return true;
  }
  if (!is_prepared_trx && thd->in_active_multi_stmt_transaction()) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), self->name.str);
    return true;
  }
  return check_session_admin_or_replication_applier(self, thd, var);
}

static bool check_session_admin_outside_trx_outside_sf_outside_sp(
    sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin_outside_trx_outside_sf(self, thd, var)) return true;
  if (thd->lex->sphead) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_SP, MYF(0), self->name.str);
    return true;
  }
  return false;
}

static bool binlog_format_check(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;

  if (var->type == OPT_GLOBAL || var->type == OPT_PERSIST) {
    /*
      SET @@global.binlog_format and SET @@persist.binlog_format must be
      disallowed if any replication channel has open temporary table(s).
      Otherwise DROP TEMPORARY TABLE is written into binary log on slave
      (which disobeys the simple rule: When @@session.binlog_format=
       ROW/MIXED, the server must not write CREATE/DROP TEMPORARY TABLE
      to the binary log) in the following case:
        slave> SET @@global.binlog_format=STATEMENT;
        slave> START SLAVE;
        master> CREATE TEMPORARY TABLE t1(a INT);
        slave> [wait for t1 to replicate]
        slave> STOP SLAVE;
        slave> SET @@global.binlog_format=ROW / SET @@persist.binlog_format=ROW
        master> DROP TEMPORARY TABLE t1;
        slave> START SLAVE;
      Note: SET @@persist_only.binlog_format is not disallowed if any
      replication channel has temporary table(s), since unlike PERSIST,
      PERSIST_ONLY does not modify the runtime global system variable value.

      SET @@global.binlog_format and SET @@persist.binlog_format must be
      disallowed if any replication channel applier is running, because
      SET @@global.binlog_format does not take effect when any replication
      channel applier is running. SET @@global.binlog_format takes effect
      on the channel until its applier is (re)starting.
      Note: SET @@persist_only.binlog_format is not disallowed if any
      replication channel applier is running, since unlike PERSIST,
      PERSIST_ONLY does not modify the runtime global system variable value.
    */
    enum_slave_channel_status slave_channel_status =
        has_any_slave_channel_open_temp_table_or_is_its_applier_running();
    if (slave_channel_status == SLAVE_CHANNEL_APPLIER_IS_RUNNING) {
      my_error(ER_RUNNING_APPLIER_PREVENTS_SWITCH_GLOBAL_BINLOG_FORMAT, MYF(0));
      return true;
    } else if (slave_channel_status == SLAVE_CHANNEL_HAS_OPEN_TEMPORARY_TABLE) {
      my_error(ER_TEMP_TABLE_PREVENTS_SWITCH_GLOBAL_BINLOG_FORMAT, MYF(0));
      return true;
    }
  }

  if (!var->is_global_persist()) {
    /*
      SET @@session.binlog_format must be disallowed if the session has open
      temporary table(s). Otherwise DROP TEMPORARY TABLE is written into
      binary log (which disobeys the simple rule: When
      @@session.binlog_format=ROW/MIXED, the server must not write
      CREATE/DROP TEMPORARY TABLE to the binary log) in the following case:
        SET @@session.binlog_format=STATEMENT;
        CREATE TEMPORARY TABLE t1 (a INT);
        SET @@session.binlog_format=ROW;
        DROP TEMPORARY TABLE t1;
      And more, if binlog_format=ROW/MIXED and the session has open temporary
      table(s), these CREATE TEMPORARY TABLE are not written into the binlog,
      so we can not switch to STATEMENT.
    */
    if (thd->temporary_tables) {
      my_error(ER_TEMP_TABLE_PREVENTS_SWITCH_SESSION_BINLOG_FORMAT, MYF(0));
      return true;
    }

    /*
      if in a stored function/trigger, it's too late to change mode
    */
    if (thd->in_sub_stmt) {
      my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT, MYF(0));
      return true;
    }
    /*
      Make the session variable 'binlog_format' read-only inside a transaction.
    */
    if (thd->in_active_multi_stmt_transaction()) {
      my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_FORMAT, MYF(0));
      return true;
    }
  }

  /*
    If moving to statement format, and binlog_row_value_options is set,
    generate a warning.
  */
  if (var->save_result.ulonglong_value == BINLOG_FORMAT_STMT) {
    if ((var->is_global_persist() &&
         global_system_variables.binlog_row_value_options != 0) ||
        (!var->is_global_persist() &&
         thd->variables.binlog_row_value_options != 0)) {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_BINLOG_PARTIAL_UPDATES_DISABLED,
                          ER_THD(thd, ER_WARN_BINLOG_PARTIAL_UPDATES_DISABLED),
                          "binlog_format=STATEMENT", "PARTIAL_JSON");
    }
  }

  return false;
}

static bool fix_binlog_format_after_update(sys_var *, THD *thd,
                                           enum_var_type type) {
  if (type == OPT_SESSION) thd->reset_current_stmt_binlog_format_row();
  return false;
}

static bool prevent_global_rbr_exec_mode_idempotent(sys_var *self, THD *,
                                                    set_var *var) {
  if (var->is_global_persist()) {
    my_error(ER_LOCAL_VARIABLE, MYF(0), self->name.str);
    return true;
  }
  return false;
}

static Sys_var_test_flag Sys_core_file("core_file",
                                       "write a core-file on crashes",
                                       TEST_CORE_ON_SIGNAL);

static Sys_var_enum Sys_binlog_format(
    "binlog_format",
    "The format used when writing the binary log. ROW writes each changed "
    "row in a binary format. STATEMENT writes SQL statements. MIXED writes "
    "SQL statements for most statements, and row format for statements that "
    "cannot be replayed in a deterministic manner using SQL. If NDBCLUSTER "
    "is enabled and binlog-format is MIXED, the format switches to row-based "
    "and back implicitly for each query accessing an NDBCLUSTER table."
    " This option is deprecated and will be removed in a future version.",
    SESSION_VAR(binlog_format), CMD_LINE(REQUIRED_ARG, OPT_BINLOG_FORMAT),
    binlog_format_names, DEFAULT(BINLOG_FORMAT_ROW), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(binlog_format_check),
    ON_UPDATE(fix_binlog_format_after_update), DEPRECATED_VAR(""));

static const char *rbr_exec_mode_names[] = {"STRICT", "IDEMPOTENT", nullptr};
static Sys_var_enum rbr_exec_mode(
    "rbr_exec_mode",
    "Modes for how row events should be executed. Legal values "
    "are STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, "
    "the server will not throw errors for operations that are idempotent. "
    "In STRICT mode, server will throw errors for the operations that "
    "cause a conflict.",
    SESSION_VAR(rbr_exec_mode_options), NO_CMD_LINE, rbr_exec_mode_names,
    DEFAULT(RBR_EXEC_MODE_STRICT), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(prevent_global_rbr_exec_mode_idempotent), ON_UPDATE(nullptr));

static bool check_binlog_row_image(sys_var *self [[maybe_unused]], THD *thd,
                                   set_var *var) {
  DBUG_TRACE;
  if (check_session_admin(self, thd, var)) return true;
  if (var->save_result.ulonglong_value == BINLOG_ROW_IMAGE_FULL) {
    if ((var->is_global_persist() &&
         global_system_variables.binlog_row_value_options != 0) ||
        (!var->is_global_persist() &&
         thd->variables.binlog_row_value_options != 0)) {
      push_warning_printf(
          thd, Sql_condition::SL_WARNING,
          ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES,
          ER_THD(thd, ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES),
          "binlog_row_image=FULL", "PARTIAL_JSON");
    }
  }
  return false;
}

static const char *binlog_row_image_names[] = {"MINIMAL", "NOBLOB", "FULL",
                                               NullS};
static Sys_var_enum Sys_binlog_row_image(
    "binlog_row_image",
    "Controls whether rows should be logged in 'FULL', 'NOBLOB' or "
    "'MINIMAL' formats. 'FULL', means that all columns in the before "
    "and after image are logged. 'NOBLOB', means that mysqld avoids logging "
    "blob columns whenever possible (e.g. blob column was not changed or "
    "is not part of primary key). 'MINIMAL', means that a PK equivalent (PK "
    "columns or full row if there is no PK in the table) is logged in the "
    "before image, and only changed columns are logged in the after image. "
    "(Default: FULL).",
    SESSION_VAR(binlog_row_image), CMD_LINE(REQUIRED_ARG),
    binlog_row_image_names, DEFAULT(BINLOG_ROW_IMAGE_FULL), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_binlog_row_image), ON_UPDATE(nullptr));

static const char *binlog_row_metadata_names[] = {"MINIMAL", "FULL", NullS};
static Sys_var_enum Sys_binlog_row_metadata(
    "binlog_row_metadata",
    "Controls how much type information is written to the binary log when "
    "using ROW format. FULL causes all metadata to be logged. MINIMAL means "
    "that only metadata actually needed by replicas is logged.",
    GLOBAL_VAR(binlog_row_metadata), CMD_LINE(REQUIRED_ARG),
    binlog_row_metadata_names, DEFAULT(BINLOG_ROW_METADATA_MINIMAL),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr));

static bool check_binlog_trx_compression(sys_var *self [[maybe_unused]],
                                         THD *thd, set_var *var) {
  DBUG_TRACE;
  if (check_session_admin(self, thd, var)) return true;

  if (!var->is_global_persist() && thd->in_active_multi_stmt_transaction()) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), self->name.str);
    return true;
  }
  return false;
}

static Sys_var_bool Sys_binlog_trx_compression(
    "binlog_transaction_compression",
    "Whether to compress transactions or not. Transactions are compressed "
    "using the ZSTD compression algorythm.",
    SESSION_VAR(binlog_trx_compression), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_binlog_trx_compression));

static Sys_var_uint Sys_binlog_transaction_compression_level_zstd(
    "binlog_transaction_compression_level_zstd",
    "Specifies the transaction compression level for ZSTD "
    "transaction compression in the binary log.",
    SESSION_VAR(binlog_trx_compression_level_zstd), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, 22),
    DEFAULT(binary_log::transaction::compression::Zstd_comp::
                default_compression_level),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_binlog_trx_compression), ON_UPDATE(nullptr));

static bool on_session_track_gtids_update(sys_var *, THD *thd, enum_var_type) {
  thd->session_tracker.get_tracker(SESSION_GTIDS_TRACKER)->update(thd);
  return false;
}

static const char *session_track_gtids_names[] = {"OFF", "OWN_GTID",
                                                  "ALL_GTIDS", NullS};
static Sys_var_enum Sys_session_track_gtids(
    "session_track_gtids",
    "Controls the amount of global transaction ids to be "
    "included in the response packet sent by the server."
    "(Default: OFF).",
    SESSION_VAR(session_track_gtids), CMD_LINE(REQUIRED_ARG),
    session_track_gtids_names, DEFAULT(SESSION_TRACK_GTIDS_OFF), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_outside_trx),
    ON_UPDATE(on_session_track_gtids_update));

static bool binlog_direct_check(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;

  if (var->is_global_persist()) return false;

  /*
    Makes the session variable 'binlog_direct_non_transactional_updates'
    read-only if within a procedure, trigger or function.
  */
  if (thd->in_sub_stmt) {
    my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_DIRECT, MYF(0));
    return true;
  }
  /*
    Makes the session variable 'binlog_direct_non_transactional_updates'
    read-only inside a transaction.
  */
  if (thd->in_active_multi_stmt_transaction()) {
    my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_DIRECT, MYF(0));
    return true;
  }

  return false;
}

static Sys_var_bool Sys_binlog_direct(
    "binlog_direct_non_transactional_updates",
    "Causes updates to non-transactional engines using statement format to "
    "be written directly to binary log, after executing them and before "
    "committing the transaction. Before using this option make sure "
    "that there are no dependencies between transactional and "
    "non-transactional tables such as in the statement INSERT INTO t_myisam "
    "SELECT * FROM t_innodb; otherwise, replicas may diverge.",
    SESSION_VAR(binlog_direct_non_trans_update), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(binlog_direct_check));

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
static Sys_var_bool Sys_explicit_defaults_for_timestamp(
    "explicit_defaults_for_timestamp",
    "This option causes CREATE TABLE to create all TIMESTAMP columns "
    "as NULL with DEFAULT NULL attribute, Without this option, "
    "TIMESTAMP columns are NOT NULL and have implicit DEFAULT clauses. "
    "The old behavior is deprecated. "
    "The variable can only be set by users having the SUPER privilege.",
    SESSION_VAR(explicit_defaults_for_timestamp), CMD_LINE(OPT_ARG),
    DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_explicit_defaults_for_timestamp));

static bool repository_check(sys_var *self, THD *thd, set_var *var,
                             SLAVE_THD_TYPE thread_mask) {
  bool ret = false;
  if (check_session_admin_outside_trx_outside_sf(self, thd, var)) return true;
  Master_info *mi;
  int running = 0;
  const char *msg = nullptr;
  bool rpl_info_option = static_cast<uint>(var->save_result.ulonglong_value);

  /* don't convert if the repositories are same */
  if (rpl_info_option ==
      (0 != (thread_mask == SLAVE_THD_IO ? opt_mi_repository_id
                                         : opt_rli_repository_id)))
    return false;

  channel_map.wrlock();

  /* Repository conversion not possible, when multiple channels exist */
  if (channel_map.get_num_instances(true) > 1) {
    msg = "Repository conversion is possible when only default channel exists";
    my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
    channel_map.unlock();
    return true;
  }

  mi = channel_map.get_default_channel_mi();

  if (mi != nullptr) {
    mi->channel_wrlock();
    lock_slave_threads(mi);
    init_thread_mask(&running, mi, false);
    if (!running) {
      bool is_pos_info_invalid{false};
      switch (thread_mask) {
        case SLAVE_THD_IO:
          is_pos_info_invalid = mi->is_receiver_position_info_invalid();
          mysql_mutex_lock(&mi->data_lock);
          mi->flush_info(true);
          mysql_mutex_unlock(&mi->data_lock);
          if (Rpl_info_factory::change_mi_repository(
                  mi, static_cast<uint>(var->save_result.ulonglong_value),
                  &msg)) {
            ret = true;
            my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
          }
          mi->set_receiver_position_info_invalid(is_pos_info_invalid);
          break;
        case SLAVE_THD_SQL:
          mts_recovery_groups(mi->rli);
          if (!mi->rli->is_mts_recovery()) {
            is_pos_info_invalid =
                mi->rli->is_applier_source_position_info_invalid();
            if (Rpl_info_factory::reset_workers(mi->rli) ||
                mi->rli->flush_info(
                    Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT |
                    Relay_log_info::RLI_FLUSH_IGNORE_GTID_ONLY) ||
                Rpl_info_factory::change_rli_repository(
                    mi->rli,
                    static_cast<uint>(var->save_result.ulonglong_value),
                    &msg)) {
              ret = true;
              my_error(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, MYF(0), msg);
            }
            mi->rli->set_applier_source_position_info_invalid(
                is_pos_info_invalid);
          } else
            LogErr(WARNING_LEVEL, ER_RPL_REPO_HAS_GAPS);
          break;
        default:
          assert(0);
          break;
      }
    } else {
      ret = true;
      my_error(ER_REPLICA_CHANNEL_MUST_STOP, MYF(0), mi->get_channel());
    }
    unlock_slave_threads(mi);
    mi->channel_unlock();
  }
  channel_map.unlock();
  return ret;
}

static bool relay_log_info_repository_check(sys_var *self, THD *thd,
                                            set_var *var) {
  return repository_check(self, thd, var, SLAVE_THD_SQL);
}

static bool master_info_repository_check(sys_var *self, THD *thd,
                                         set_var *var) {
  return repository_check(self, thd, var, SLAVE_THD_IO);
}

static bool replica_parallel_workers_update(sys_var *, THD *thd,
                                            enum_var_type) {
  if (opt_mts_replica_parallel_workers == 0) {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_DEPRECATED_SYNTAX,
                        ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX), "0", "1");
  }
  return false;
}

static const char *repository_names[] = {"FILE", "TABLE",
#ifndef NDEBUG
                                         "DUMMY",
#endif
                                         nullptr};

ulong opt_mi_repository_id = INFO_REPOSITORY_TABLE;
static Sys_var_enum Sys_mi_repository(
    "master_info_repository",
    "The repository format for the replication connection configuration.",
    GLOBAL_VAR(opt_mi_repository_id),
    CMD_LINE(REQUIRED_ARG, OPT_MASTER_INFO_REPOSITORY), repository_names,
    DEFAULT(INFO_REPOSITORY_TABLE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(master_info_repository_check), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

ulong opt_rli_repository_id = INFO_REPOSITORY_TABLE;
static Sys_var_enum Sys_rli_repository(
    "relay_log_info_repository",
    "Defines the type of the repository for the relay log information "
    "and associated workers.",
    GLOBAL_VAR(opt_rli_repository_id),
    CMD_LINE(REQUIRED_ARG, OPT_RELAY_LOG_INFO_REPOSITORY), repository_names,
    DEFAULT(INFO_REPOSITORY_TABLE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(relay_log_info_repository_check), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_bool Sys_binlog_rows_query(
    "binlog_rows_query_log_events",
    "Allow writing of Rows_query_log events into binary log.",
    SESSION_VAR(binlog_rows_query_log_events), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin));

static Sys_var_bool Sys_binlog_order_commits(
    "binlog_order_commits",
    "Issue internal commit calls in the same order as transactions are"
    " written to the binary log. Default is to order commits.",
    GLOBAL_VAR(opt_binlog_order_commits), CMD_LINE(OPT_ARG), DEFAULT(true));

static Sys_var_ulong Sys_bulk_insert_buff_size(
    "bulk_insert_buffer_size",
    "Size of tree cache used in bulk "
    "insert optimisation. Note that this is a limit per thread!",
    HINT_UPDATEABLE SESSION_VAR(bulk_insert_buff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULONG_MAX), DEFAULT(8192 * 1024), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin));

static Sys_var_charptr Sys_character_sets_dir(
    "character_sets_dir", "Directory where character sets are",
    READ_ONLY NON_PERSIST GLOBAL_VAR(charsets_dir), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_ulong Sys_select_into_buffer_size(
    "select_into_buffer_size", "Buffer size for SELECT INTO OUTFILE/DUMPFILE.",
    HINT_UPDATEABLE SESSION_VAR(select_into_buffer_size), CMD_LINE(OPT_ARG),
    VALID_RANGE(IO_SIZE * 2, INT_MAX32), DEFAULT(128 * 1024),
    BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin_no_super));

static Sys_var_bool Sys_select_into_disk_sync(
    "select_into_disk_sync",
    "Synchronize flushed buffer with disk for SELECT INTO OUTFILE/DUMPFILE.",
    HINT_UPDATEABLE SESSION_VAR(select_into_disk_sync), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_uint Sys_select_into_disk_sync_delay(
    "select_into_disk_sync_delay",
    "The delay in milliseconds after each buffer sync "
    "for SELECT INTO OUTFILE/DUMPFILE. Requires select_into_sync_disk = ON.",
    HINT_UPDATEABLE SESSION_VAR(select_into_disk_sync_delay), CMD_LINE(OPT_ARG),
    VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_session_admin_no_super));

static bool check_not_null(sys_var *, THD *, set_var *var) {
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
static bool check_storage_engine(sys_var *self, THD *thd, set_var *var) {
  if (check_not_null(self, thd, var)) return true;

  if (!opt_initialize && !opt_noacl) {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;
    LEX_CSTRING se_name;

    if (var->value) {
      res = var->value->val_str(&str);
      lex_cstring_set(&se_name, res->ptr());
    } else {
      // Use the default value defined by sys_var.
      lex_cstring_set(
          &se_name,
          pointer_cast<const char *>(
              down_cast<Sys_var_plugin *>(self)->global_value_ptr(thd, {})));
    }

    plugin_ref plugin;
    if ((plugin = ha_resolve_by_name(nullptr, &se_name, false))) {
      handlerton *hton = plugin_data<handlerton *>(plugin);
      if (ha_is_storage_engine_disabled(hton))
        LogErr(WARNING_LEVEL, ER_DISABLED_STORAGE_ENGINE_AS_DEFAULT,
               self->name.str, se_name.str);
      plugin_unlock(nullptr, plugin);
    }
  }
  return false;
}

static bool check_charset(sys_var *, THD *thd, set_var *var) {
  if (!var->value) return false;

  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == STRING_RESULT) {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res = var->value->val_str(&str)))
      var->save_result.ptr = nullptr;
    else {
      ErrConvString err(res); /* Get utf8 '\0' terminated string */
      if (!(var->save_result.ptr =
                get_charset_by_csname(err.ptr(), MY_CS_PRIMARY, MYF(0))) &&
          !(var->save_result.ptr = get_old_charset_by_name(err.ptr()))) {
        my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), err.ptr());
        return true;
      }
      warn_on_deprecated_charset(
          thd, static_cast<const CHARSET_INFO *>(var->save_result.ptr),
          err.ptr());
    }
  } else  // INT_RESULT
  {
    int csno = (int)var->value->val_int();
    if (!(var->save_result.ptr = get_charset(csno, MYF(0)))) {
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), llstr(csno, buff));
      return true;
    }
    warn_on_deprecated_charset(
        thd, static_cast<const CHARSET_INFO *>(var->save_result.ptr),
        static_cast<const CHARSET_INFO *>(var->save_result.ptr)->m_coll_name);
  }
  return false;
}
static bool check_charset_not_null(sys_var *self, THD *thd, set_var *var) {
  return check_charset(self, thd, var) || check_not_null(self, thd, var);
}

namespace {
struct Get_name {
  explicit Get_name(const CHARSET_INFO *ci) : m_ci(ci) {}
  const uchar *get_name() const {
    return pointer_cast<const uchar *>(m_ci->m_coll_name);
  }
  const CHARSET_INFO *m_ci;
};

struct Get_csname {
  explicit Get_csname(const CHARSET_INFO *ci) : m_ci(ci) {}
  const uchar *get_name() const {
    return pointer_cast<const uchar *>(m_ci->csname);
  }
  const CHARSET_INFO *m_ci;
};

}  // namespace

static CHARSET_INFO *charset_system_default = &my_charset_utf8mb3_general_ci;

static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_system(
    "character_set_system",
    "The character set used by the server "
    "for storing identifiers",
    READ_ONLY NON_PERSIST GLOBAL_VAR(system_charset_info), NO_CMD_LINE,
    DEFAULT(&charset_system_default));

static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_server(
    "character_set_server", "The default character set",
    SESSION_VAR(collation_server), NO_CMD_LINE, DEFAULT(&default_charset_info),
    NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_charset_not_null));

static bool check_charset_db(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;
  if (check_charset_not_null(self, thd, var)) return true;
  if (!var->value)  // = DEFAULT
    var->save_result.ptr = thd->db_charset;
  return false;
}

static bool update_deprecated_with_removal_message(sys_var *self, THD *thd,
                                                   enum_var_type) {
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                      ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
                      self->name.str);
  return false;
}
static bool update_deprecated(sys_var *self, THD *thd, enum_var_type) {
  push_warning_printf(
      thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
      ER_THD(thd, ER_WARN_DEPRECATED_SYSVAR_UPDATE), self->name.str);
  return false;
}
static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_database(
    "character_set_database", " The character set used by the default database",
    SESSION_VAR(collation_database), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_charset_db), ON_UPDATE(update_deprecated));

static bool check_cs_client(sys_var *self, THD *thd, set_var *var) {
  if (check_charset_not_null(self, thd, var)) return true;

  // We don't currently support any variable-width character set with a minimum
  // length greater than 1. If we ever do, we have to revisit
  // is_supported_parser_charset(). See Item_func_statement_digest::val_str()
  // and Item_func_statement_digest_text::val_str().
  return (static_cast<const CHARSET_INFO *>(var->save_result.ptr))->mbminlen >
         1;
}
static bool fix_thd_charset(sys_var *, THD *thd, enum_var_type type) {
  if (type == OPT_SESSION) thd->update_charset();
  return false;
}
static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_client(
    "character_set_client",
    "The character set for statements "
    "that arrive from the client",
    SESSION_VAR(character_set_client), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_cs_client), ON_UPDATE(fix_thd_charset));

static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_connection(
    "character_set_connection",
    "The character set used for "
    "literals that do not have a character set introducer and for "
    "number-to-string conversion",
    SESSION_VAR(collation_connection), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_charset_not_null), ON_UPDATE(fix_thd_charset));

static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_results(
    "character_set_results",
    "The character set used for returning "
    "query results to the client",
    SESSION_VAR(character_set_results), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_charset));

static bool check_cs_filesystem(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;
  if (check_charset_not_null(self, thd, var)) return true;

  return false;
}

static Sys_var_struct<CHARSET_INFO, Get_csname> Sys_character_set_filesystem(
    "character_set_filesystem", "The filesystem character set",
    SESSION_VAR(character_set_filesystem), NO_CMD_LINE,
    DEFAULT(&character_set_filesystem), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_cs_filesystem), ON_UPDATE(fix_thd_charset));

static const char *completion_type_names[] = {"NO_CHAIN", "CHAIN", "RELEASE",
                                              nullptr};
static Sys_var_enum Sys_completion_type(
    "completion_type",
    "The transaction completion type, one of "
    "NO_CHAIN, CHAIN, RELEASE",
    SESSION_VAR(completion_type), CMD_LINE(REQUIRED_ARG), completion_type_names,
    DEFAULT(0));

static bool check_collation_not_null(sys_var *self, THD *thd, set_var *var) {
  if (!var->value) return false;

  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == STRING_RESULT) {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res = var->value->val_str(&str)))
      var->save_result.ptr = nullptr;
    else {
      ErrConvString err(res); /* Get utf8 '\0'-terminated string */
      if (!(var->save_result.ptr = get_charset_by_name(err.ptr(), MYF(0)))) {
        my_error(ER_UNKNOWN_COLLATION, MYF(0), err.ptr());
        return true;
      }
    }
  } else  // INT_RESULT
  {
    int csno = (int)var->value->val_int();
    if (!(var->save_result.ptr = get_charset(csno, MYF(0)))) {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), llstr(csno, buff));
      return true;
    }
  }
  if (var->save_result.ptr) {
    warn_on_deprecated_collation(
        thd, static_cast<const CHARSET_INFO *>(var->save_result.ptr));
  }

  return check_not_null(self, thd, var);
}
static Sys_var_struct<CHARSET_INFO, Get_name> Sys_collation_connection(
    "collation_connection",
    "The collation of the connection "
    "character set",
    SESSION_VAR(collation_connection), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_collation_not_null), ON_UPDATE(fix_thd_charset));

static bool check_collation_db(sys_var *self, THD *thd, set_var *var) {
  if (check_collation_not_null(self, thd, var)) return true;
  if (!var->value)  // = DEFAULT
    var->save_result.ptr = thd->db_charset;
  return false;
}
static Sys_var_struct<CHARSET_INFO, Get_name> Sys_collation_database(
    "collation_database",
    "The collation of the database "
    "character set",
    SESSION_VAR(collation_database), NO_CMD_LINE,
    DEFAULT(&default_charset_info), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_collation_db), ON_UPDATE(update_deprecated));

static Sys_var_struct<CHARSET_INFO, Get_name> Sys_collation_server(
    "collation_server", "The server default collation",
    SESSION_VAR(collation_server), NO_CMD_LINE, DEFAULT(&default_charset_info),
    NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_collation_not_null));

static const char *concurrent_insert_names[] = {"NEVER", "AUTO", "ALWAYS",
                                                nullptr};
static Sys_var_enum Sys_concurrent_insert(
    "concurrent_insert",
    "Use concurrent insert with MyISAM. Possible "
    "values are NEVER, AUTO, ALWAYS",
    GLOBAL_VAR(myisam_concurrent_insert), CMD_LINE(OPT_ARG),
    concurrent_insert_names, DEFAULT(1));

static Sys_var_ulong Sys_connect_timeout(
    "connect_timeout",
    "The number of seconds the mysqld server is waiting for a connect "
    "packet before responding with 'Bad handshake'",
    GLOBAL_VAR(connect_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(2, LONG_TIMEOUT), DEFAULT(CONNECT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_ulong Sys_information_schema_stats_expiry(
    "information_schema_stats_expiry",
    "The number of seconds after which mysqld server will fetch "
    "data from storage engine and replace the data in cache.",
    SESSION_VAR(information_schema_stats_expiry), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(24 * 60 * 60), BLOCK_SIZE(1));

static Sys_var_charptr Sys_datadir(
    "datadir", "Path to the database root directory",
    READ_ONLY NON_PERSIST GLOBAL_VAR(mysql_real_data_home_ptr),
    CMD_LINE(REQUIRED_ARG, 'h'), IN_FS_CHARSET, DEFAULT(mysql_real_data_home));

#ifndef NDEBUG
static Sys_var_dbug Sys_dbug("debug", "Debug log", sys_var::SESSION,
                             CMD_LINE(OPT_ARG, '#'), DEFAULT(""),
                             NO_MUTEX_GUARD, NOT_IN_BINLOG,
                             ON_CHECK(check_session_admin));
#endif

/**
  @todo
    When updating myisam_delay_key_write, we should do a 'flush tables'
    of all MyISAM tables to ensure that they are reopen with the
    new attribute.
*/
export bool fix_delay_key_write(sys_var *, THD *, enum_var_type) {
  switch (delay_key_write_options) {
    case DELAY_KEY_WRITE_NONE:
      myisam_delay_key_write = false;
      break;
    case DELAY_KEY_WRITE_ON:
      myisam_delay_key_write = true;
      break;
    case DELAY_KEY_WRITE_ALL:
      myisam_delay_key_write = true;
      ha_open_options |= HA_OPEN_DELAY_KEY_WRITE;
      break;
  }
  return false;
}
static const char *delay_key_write_names[] = {"OFF", "ON", "ALL", NullS};
static Sys_var_enum Sys_delay_key_write(
    "delay_key_write", "Type of DELAY_KEY_WRITE",
    GLOBAL_VAR(delay_key_write_options), CMD_LINE(OPT_ARG),
    delay_key_write_names, DEFAULT(DELAY_KEY_WRITE_ON), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(fix_delay_key_write));

static Sys_var_ulong Sys_delayed_insert_limit(
    "delayed_insert_limit",
    "After inserting delayed_insert_limit rows, the INSERT DELAYED "
    "handler will check if there are any SELECT statements pending. "
    "If so, it allows these to execute before continuing. "
    "This variable is deprecated along with INSERT DELAYED.",
    GLOBAL_VAR(delayed_insert_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, ULONG_MAX), DEFAULT(DELAYED_LIMIT), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_ulong Sys_delayed_insert_timeout(
    "delayed_insert_timeout",
    "How long a INSERT DELAYED thread should wait for INSERT statements "
    "before terminating. "
    "This variable is deprecated along with INSERT DELAYED.",
    GLOBAL_VAR(delayed_insert_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(DELAYED_WAIT_TIMEOUT), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_ulong Sys_delayed_queue_size(
    "delayed_queue_size",
    "What size queue (in rows) should be allocated for handling INSERT "
    "DELAYED. If the queue becomes full, any client that does INSERT "
    "DELAYED will wait until there is room in the queue again. "
    "This variable is deprecated along with INSERT DELAYED.",
    GLOBAL_VAR(delayed_queue_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, ULONG_MAX), DEFAULT(DELAYED_QUEUE_SIZE), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static const char *event_scheduler_names[] = {"OFF", "ON", "DISABLED", NullS};
static bool event_scheduler_check(sys_var *, THD *, set_var *var) {
  /* DISABLED is only accepted on the command line */
  if (var->save_result.ulonglong_value == Events::EVENTS_DISABLED) return true;
  if (Events::opt_event_scheduler == Events::EVENTS_DISABLED) {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--event-scheduler=DISABLED or --skip-grant-tables");
    return true;
  }
  return false;
}
static bool event_scheduler_update(sys_var *, THD *, enum_var_type) {
  int err_no = 0;
  ulong opt_event_scheduler_value = Events::opt_event_scheduler;
  mysql_mutex_unlock(&LOCK_global_system_variables);
  /*
    Events::start() is heavyweight. In particular it creates a new THD,
    which takes LOCK_global_system_variables internally.
    Thus we have to release it here.
    We need to re-take it before returning, though.

    Note that since we release LOCK_global_system_variables before calling
    start/stop, there is a possibility that the server variable
    can become out of sync with the real event scheduler state.

    This can happen with two concurrent statements if the first gets
    interrupted after start/stop but before retaking
    LOCK_global_system_variables. However, this problem should be quite
    rare and it's difficult to avoid it without opening up possibilities
    for deadlocks. See bug#51160.
  */
  bool ret = opt_event_scheduler_value == Events::EVENTS_ON
                 ? Events::start(&err_no)
                 : Events::stop();
  mysql_mutex_lock(&LOCK_global_system_variables);
  if (ret) {
    Events::opt_event_scheduler = Events::EVENTS_OFF;
    my_error(ER_EVENT_SET_VAR_ERROR, MYF(0), err_no);
  }
  return ret;
}

static Sys_var_enum Sys_event_scheduler(
    "event_scheduler",
    "Enable the event scheduler. Possible values are "
    "ON, OFF, and DISABLED (keep the event scheduler completely "
    "deactivated, it cannot be activated run-time)",
    GLOBAL_VAR(Events::opt_event_scheduler), CMD_LINE(OPT_ARG),
    event_scheduler_names, DEFAULT(Events::EVENTS_ON), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(event_scheduler_check),
    ON_UPDATE(event_scheduler_update));

static bool check_expire_logs_days(sys_var *, THD *, set_var *var) {
  ulonglong expire_logs_days_value = var->save_result.ulonglong_value;

  if (expire_logs_days_value && binlog_expire_logs_seconds) {
    my_error(ER_BINLOG_EXPIRE_LOG_DAYS_AND_SECS_USED_TOGETHER, MYF(0));
    return true;
  }
  return false;
}

static bool check_expire_logs_seconds(sys_var *, THD *, set_var *var) {
  ulonglong expire_logs_seconds_value = var->save_result.ulonglong_value;

  if (expire_logs_days && expire_logs_seconds_value) {
    my_error(ER_DA_EXPIRE_LOGS_DAYS_IGNORED, MYF(0));
    return true;
  }
  return false;
}

static Sys_var_ulong Sys_expire_logs_days(
    "expire_logs_days",
    "If non-zero, binary logs will be purged after expire_logs_days "
    "days; If this option alone is set on the command line or in a "
    "configuration file, it overrides the default value for "
    "binlog-expire-logs-seconds. If both options are set to nonzero values, "
    "binlog-expire-logs-seconds takes priority. Possible purges happen at "
    "startup and at binary log rotation.",
    GLOBAL_VAR(expire_logs_days), CMD_LINE(REQUIRED_ARG, OPT_EXPIRE_LOGS_DAYS),
    VALID_RANGE(0, 99), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_expire_logs_days), ON_UPDATE(nullptr),
    DEPRECATED_VAR("binlog_expire_logs_seconds"));

static Sys_var_ulong Sys_binlog_expire_logs_seconds(
    "binlog_expire_logs_seconds",
    "If non-zero, binary logs will be purged after binlog_expire_logs_seconds"
    " seconds; If both this option and expire_logs_days are set to non-zero"
    "  values, this option takes priority. Purges happen at"
    " startup and at binary log rotation.",
    GLOBAL_VAR(binlog_expire_logs_seconds),
    CMD_LINE(REQUIRED_ARG, OPT_BINLOG_EXPIRE_LOGS_SECONDS),
    VALID_RANGE(0, 0xFFFFFFFF), DEFAULT(2592000), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_expire_logs_seconds), ON_UPDATE(nullptr));

static Sys_var_bool Sys_binlog_expire_logs_auto_purge(
    "binlog_expire_logs_auto_purge",
    "Controls whether the server shall automatically purge binary log "
    "files or not. If this variable is set to FALSE then the server will "
    "not purge binary log files automatically.",
    GLOBAL_VAR(opt_binlog_expire_logs_auto_purge), CMD_LINE(OPT_ARG),
    DEFAULT(true));

static Sys_var_bool Sys_flush(
    "flush", "Flush MyISAM tables to disk between SQL commands",
    GLOBAL_VAR(myisam_flush), CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_ulong Sys_flush_time(
    "flush_time",
    "A dedicated thread is created to flush all tables at the "
    "given interval",
    GLOBAL_VAR(flush_time), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(0), BLOCK_SIZE(1));

static bool check_ftb_syntax(sys_var *, THD *, set_var *var) {
  return ft_boolean_check_syntax_string(
      (uchar *)(var->save_result.string_value.str));
}
/// @todo make SESSION_VAR (usability enhancement and a fix for a race
/// condition)
static Sys_var_charptr Sys_ft_boolean_syntax(
    "ft_boolean_syntax",
    "List of operators for "
    "MATCH ... AGAINST ( ... IN BOOLEAN MODE)",
    GLOBAL_VAR(ft_boolean_syntax), CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(DEFAULT_FTB_SYNTAX), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_ftb_syntax));

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
    READ_ONLY GLOBAL_VAR(ft_query_expansion_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1000), DEFAULT(20), BLOCK_SIZE(1));

static Sys_var_charptr Sys_ft_stopword_file(
    "ft_stopword_file", "Use stopwords from this file instead of built-in list",
    READ_ONLY NON_PERSIST GLOBAL_VAR(ft_stopword_file), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

static bool check_init_string(sys_var *, THD *, set_var *var) {
  if (var->save_result.string_value.str == nullptr) {
    var->save_result.string_value.str = const_cast<char *>("");
    var->save_result.string_value.length = 0;
  }
  return false;
}
static PolyLock_rwlock PLock_sys_init_connect(&LOCK_sys_init_connect);
static Sys_var_lexstring Sys_init_connect(
    "init_connect",
    "Command(s) that are executed for each "
    "new connection",
    GLOBAL_VAR(opt_init_connect), CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(""), &PLock_sys_init_connect, NOT_IN_BINLOG,
    ON_CHECK(check_init_string));

static Sys_var_charptr Sys_init_file(
    "init_file", "Read SQL commands from this file at startup",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_init_file), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

static PolyLock_rwlock PLock_sys_init_replica(&LOCK_sys_init_replica);
static Sys_var_lexstring Sys_init_replica(
    "init_replica",
    "Command(s) that are executed by the replication applier thread "
    "each time the applier threads start.",
    GLOBAL_VAR(opt_init_replica), CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(""), &PLock_sys_init_replica, NOT_IN_BINLOG,
    ON_CHECK(check_init_string));

static Sys_var_deprecated_alias Sys_init_slave("init_slave", Sys_init_replica);

static Sys_var_ulong Sys_interactive_timeout(
    "interactive_timeout",
    "The number of seconds the server waits for activity on an interactive "
    "connection before closing it",
    SESSION_VAR(net_interactive_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(NET_WAIT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_ulong Sys_join_buffer_size(
    "join_buffer_size", "The size of the buffer that is used for full joins",
    HINT_UPDATEABLE SESSION_VAR(join_buff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(128, ULONG_MAX), DEFAULT(256 * 1024), BLOCK_SIZE(128));

static Sys_var_keycache Sys_key_buffer_size(
    "key_buffer_size",
    "The size of the buffer used for "
    "index blocks for MyISAM tables. Increase this to get better index "
    "handling (for all reads and multiple writes) to as much as you can "
    "afford",
    KEYCACHE_VAR(param_buff_size), CMD_LINE(REQUIRED_ARG, OPT_KEY_BUFFER_SIZE),
    VALID_RANGE(0, SIZE_T_MAX), DEFAULT(KEY_CACHE_SIZE), BLOCK_SIZE(IO_SIZE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_buffer_size));

static Sys_var_keycache Sys_key_cache_block_size(
    "key_cache_block_size", "The default size of key cache blocks",
    KEYCACHE_VAR(param_block_size),
    CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_BLOCK_SIZE),
    VALID_RANGE(512, 1024 * 16), DEFAULT(KEY_CACHE_BLOCK_SIZE), BLOCK_SIZE(512),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_keycache_param));

static Sys_var_keycache Sys_key_cache_division_limit(
    "key_cache_division_limit",
    "The minimum percentage of warm blocks in key cache",
    KEYCACHE_VAR(param_division_limit),
    CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_DIVISION_LIMIT), VALID_RANGE(1, 100),
    DEFAULT(100), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(update_keycache_param));

static Sys_var_keycache Sys_key_cache_age_threshold(
    "key_cache_age_threshold",
    "This characterizes the number of "
    "hits a hot block has to be untouched until it is considered aged "
    "enough to be downgraded to a warm block. This specifies the "
    "percentage ratio of that number of hits to the total number of "
    "blocks in key cache",
    KEYCACHE_VAR(param_age_threshold),
    CMD_LINE(REQUIRED_ARG, OPT_KEY_CACHE_AGE_THRESHOLD),
    VALID_RANGE(100, ULONG_MAX), DEFAULT(300), BLOCK_SIZE(100), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(update_keycache_param));

static Sys_var_bool Sys_large_files_support(
    "large_files_support",
    "Whether mysqld was compiled with options for large file support",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_large_files), NO_CMD_LINE,
    DEFAULT(sizeof(my_off_t) > 4));

static Sys_var_uint Sys_large_page_size(
    "large_page_size",
    "If large page support is enabled, this shows the size of memory pages",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_large_page_size), NO_CMD_LINE,
    VALID_RANGE(0, UINT_MAX), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_bool Sys_large_pages("large_pages",
                                    "Enable support for large pages",
                                    READ_ONLY GLOBAL_VAR(opt_large_pages),
                                    IF_WIN(NO_CMD_LINE, CMD_LINE(OPT_ARG)),
                                    DEFAULT(false));

static Sys_var_charptr Sys_language(
    "lc_messages_dir", "Directory where error messages are",
    READ_ONLY NON_PERSIST GLOBAL_VAR(lc_messages_dir_ptr),
    CMD_LINE(REQUIRED_ARG, OPT_LC_MESSAGES_DIRECTORY), IN_FS_CHARSET,
    DEFAULT(nullptr));

static Sys_var_bool Sys_local_infile("local_infile",
                                     "Enable LOAD DATA LOCAL INFILE",
                                     GLOBAL_VAR(opt_local_infile),
                                     CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_ulong Sys_lock_wait_timeout(
    "lock_wait_timeout",
    "Timeout in seconds to wait for a lock before returning an error.",
    HINT_UPDATEABLE SESSION_VAR(lock_wait_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(LONG_TIMEOUT), BLOCK_SIZE(1));

#ifdef HAVE_MLOCKALL
static Sys_var_bool Sys_locked_in_memory(
    "locked_in_memory", "Whether mysqld was locked in memory with --memlock",
    READ_ONLY NON_PERSIST GLOBAL_VAR(locked_in_memory), NO_CMD_LINE,
    DEFAULT(false));
#endif

/* this says NO_CMD_LINE, as command-line option takes a string, not a bool */
static Sys_var_bool Sys_log_bin("log_bin", "Whether the binary log is enabled",
                                READ_ONLY NON_PERSIST GLOBAL_VAR(opt_bin_log),
                                NO_CMD_LINE, DEFAULT(true));

static bool transaction_write_set_check(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;
  // Can't change the algorithm when group replication is enabled.
  if (is_group_replication_running()) {
    my_message(
        ER_GROUP_REPLICATION_RUNNING,
        "The write set algorithm cannot be changed when Group replication"
        " is running.",
        MYF(0));
    return true;
  }

  if ((var->is_global_persist()) &&
      global_system_variables.binlog_format != BINLOG_FORMAT_ROW) {
    my_error(ER_PREVENTS_VARIABLE_WITHOUT_RBR, MYF(0), self->name.str);
    return true;
  }

  if (var->type == OPT_SESSION &&
      thd->variables.binlog_format != BINLOG_FORMAT_ROW) {
    my_error(ER_PREVENTS_VARIABLE_WITHOUT_RBR, MYF(0), self->name.str);
    return true;
  }
  /*
    if in a stored function/trigger, it's too late to change
  */
  if (thd->in_sub_stmt) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), self->name.str);
    return true;
  }
  /*
    Make the session variable 'transaction_write_set_extraction' read-only
    inside a transaction.
  */
  if (thd->in_active_multi_stmt_transaction()) {
    my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0), self->name.str);
    return true;
  }
  /*
    Disallow changing variable 'transaction_write_set_extraction' while
    binlog_transaction_dependency_tracking is different from COMMIT_ORDER.
  */
  if (mysql_bin_log.m_dependency_tracker.m_opt_tracking_mode !=
      DEPENDENCY_TRACKING_COMMIT_ORDER) {
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
    SESSION_VAR(transaction_write_set_extraction),
    CMD_LINE(OPT_ARG, OPT_TRANSACTION_WRITE_SET_EXTRACTION),
    transaction_write_set_hashing_algorithms, DEFAULT(HASH_ALGORITHM_XXHASH64),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(transaction_write_set_check),
    ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_ulong Sys_rpl_stop_replica_timeout(
    "rpl_stop_replica_timeout",
    "Timeout in seconds to wait for replication threads to stop, before "
    "STOP REPLICA returns a warning.",
    GLOBAL_VAR(rpl_stop_replica_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(2, LONG_TIMEOUT), DEFAULT(LONG_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_deprecated_alias Sys_rpl_stop_slave_timeout(
    "rpl_stop_slave_timeout", Sys_rpl_stop_replica_timeout);

static Sys_var_enum Sys_binlog_error_action(
    "binlog_error_action",
    "When statements cannot be written to the binary log due to a fatal "
    "error, this option determines whether the server ignores the error and "
    "closes the binary log, or aborts.",
    GLOBAL_VAR(binlog_error_action), CMD_LINE(REQUIRED_ARG),
    binlog_error_action_list, DEFAULT(ABORT_SERVER));

static Sys_var_bool Sys_trust_function_creators(
    "log_bin_trust_function_creators",
    "If set to FALSE (the default), then when --log-bin is used, creation "
    "of a stored function (or trigger) is allowed only to users having the "
    "SUPER privilege and only if this stored function (trigger) may not "
    "break binary logging. Note that if ALL connections to this server "
    "ALWAYS use row-based binary logging, the security issues do not "
    "exist and the binary logging cannot break, so you can safely set "
    "this to TRUE. This variable is deprecated and will be removed in a "
    "future version.",
    GLOBAL_VAR(trust_function_creators), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_bool Sys_check_proxy_users(
    "check_proxy_users",
    "If set to FALSE (the default), then proxy user identity will not be "
    "mapped for authentication plugins which support mapping from grant "
    "tables.  When set to TRUE, users associated with authentication "
    "plugins which signal proxy user mapping should be done according to "
    "GRANT PROXY privilege definition.",
    GLOBAL_VAR(check_proxy_users), CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_bool Sys_mysql_native_password_proxy_users(
    "mysql_native_password_proxy_users",
    "If set to FALSE (the default), then the mysql_native_password "
    "plugin will not signal for authenticated users to be checked for "
    "mapping "
    "to proxy users.  When set to TRUE, the plugin will flag associated "
    "authenticated accounts to be mapped to proxy users when the server "
    "option "
    "check_proxy_users is enabled.",
    GLOBAL_VAR(mysql_native_password_proxy_users), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_bool Sys_sha256_password_proxy_users(
    "sha256_password_proxy_users",
    "If set to FALSE (the default), then the sha256_password authentication "
    "plugin will not signal for authenticated users to be checked for "
    "mapping "
    "to proxy users.  When set to TRUE, the plugin will flag associated "
    "authenticated accounts to be mapped to proxy users when the server "
    "option "
    "check_proxy_users is enabled.",
    GLOBAL_VAR(sha256_password_proxy_users), CMD_LINE(OPT_ARG), DEFAULT(false));

static bool check_log_bin_use_v1_row_events(sys_var *, THD *thd, set_var *var) {
  if (var->save_result.ulonglong_value == 1 &&
      global_system_variables.binlog_row_value_options != 0)
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED,
                        ER_THD(thd, ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED),
                        "binlog_row_value_options=PARTIAL_JSON");
  return false;
}

static Sys_var_bool Sys_log_bin_use_v1_row_events(
    "log_bin_use_v1_row_events",
    "If equal to 1 then version 1 row events are written to a row based "
    "binary log.  If equal to 0, then the latest version of events are "
    "written.  "
    "This option is useful during some upgrades.",
    NON_PERSIST GLOBAL_VAR(log_bin_use_v1_row_events),
    CMD_LINE(OPT_ARG, OPT_LOG_BIN_USE_V1_ROW_EVENTS), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_log_bin_use_v1_row_events),
    ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_charptr Sys_log_error(
    "log_error", "Error log file",
    READ_ONLY NON_PERSIST GLOBAL_VAR(log_error_dest),
    CMD_LINE(OPT_ARG, OPT_LOG_ERROR), IN_FS_CHARSET,
    DEFAULT(disabled_my_option), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr, sys_var::PARSE_EARLY);

static bool check_log_error_services(sys_var *self, THD *thd, set_var *var) {
  // test whether syntax is OK and services exist
  size_t pos;
  log_error_stack_error ret;

  if (var->save_result.string_value.str == nullptr) return true;

  ret = log_builtins_error_stack(var->save_result.string_value.str, true, &pos);

  if (strlen(var->save_result.string_value.str) < 1) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_EMPTY_PIPELINE_FOR_ERROR_LOG_SERVICE,
        ER_THD(thd, ER_EMPTY_PIPELINE_FOR_ERROR_LOG_SERVICE), self->name.str);
  } else if (ret != LOG_ERROR_STACK_SUCCESS) {
    int err_code = 0;
    switch (ret) {
      case LOG_ERROR_STACK_NO_PFS_SUPPORT:
        err_code = ER_DA_ERROR_LOG_TABLE_DISABLED;
        break;
      case LOG_ERROR_STACK_NO_LOG_PARSER:
        err_code = ER_DA_NO_ERROR_LOG_PARSER_CONFIGURED;
        break;
      case LOG_ERROR_MULTIPLE_FILTERS:
        err_code = ER_DA_ERROR_LOG_MULTIPLE_FILTERS;
        break;
      default:
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CANT_SET_ERROR_LOG_SERVICE,
            ER_THD(thd, ER_CANT_SET_ERROR_LOG_SERVICE), self->name.str,
            &((char *)var->save_result.string_value.str)[pos]);
        return true;
    }

    push_warning(thd, Sql_condition::SL_NOTE, err_code,
                 ER_THD_NONCONST(thd, err_code));
    return false;
  }

  return false;
}

static bool fix_log_error_services(sys_var *self [[maybe_unused]], THD *thd,
                                   enum_var_type type [[maybe_unused]]) {
  bool ret = false;
  // syntax is OK and services exist; try to initialize them!
  size_t pos;

  char *pipeline_config =
      my_strdup(PSI_NOT_INSTRUMENTED, opt_log_error_services, MYF(0));

  if (pipeline_config == nullptr) return true;

  /*
    Temporarily release mutex.
    This solves two issues:

    a) Setting up the error-logger may implicitly load external
       logging components. The init-function of such a component
       may try to install a system-variable and then ask the system
       for a (persisted / passed on the command-line / ...) initial
       value for said variable. The function in the component framework
       that tries to obtain this value tries to obtain the mutex
       LOCK_global_system_variables.

       Note that implicit loading is attempted during the pre-check
       phase and thus should already have happened at this stage
       and no longer be a concern.

    b) This function is called with the mutex held.
       log_builtins_error_stack() will obtain an exclusive lock on
       THR_LOCK_log_stack while it re-configures the error-logger.
       A different session might run FLUSH ERROR LOGS at the same time.
       This obtains THR_LOCK_log_stack first; an individual component's
       flush function might then try to re-install its system-variables
       on flush, which would try to obtain LOCK_global_system_variables
       as per above. I.e. both functions would try to obtain the two
       locks in a different order.

       Note that components should not behave that way; they should
       install/uninstall their variables on init/exit, not on open/close.

     Both issues are admittedly unlikely, but guarding against them is cheap.
  */
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (log_builtins_error_stack(pipeline_config, false, &pos) < 0) {
    if (pos < strlen(pipeline_config)) /* purecov: begin inspected */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_CANT_START_ERROR_LOG_SERVICE,
                          ER_THD(thd, ER_CANT_START_ERROR_LOG_SERVICE),
                          self->name.str, &((char *)pipeline_config)[pos]);
    ret = true; /* purecov: end */
  }

  my_free(pipeline_config);

  mysql_mutex_lock(&LOCK_global_system_variables);

  return ret;
}

static Sys_var_charptr Sys_log_error_services(
    "log_error_services",
    "Services that should be called when an error event is received",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_log_error_services),
    CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(LOG_ERROR_SERVICES_DEFAULT), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_log_error_services), ON_UPDATE(fix_log_error_services),
    /*
      We parse it early so it goes into one logical chunk with log_error
      and log_timestamps, but we don't activate it immediately. We need
      to wait until component_infrastructure_init() has run, but want to
      set up logging services before get_options() is run. That way, any
      loadable components are ready in case component system variables
      are set from get_options().
    */
    nullptr, sys_var::PARSE_EARLY);

static bool check_log_error_suppression_list(sys_var *self, THD *thd,
                                             set_var *var) {
  int i;

  if (var->save_result.string_value.str == nullptr) return true;

  if ((i = log_builtins_filter_parse_suppression_list(
           var->save_result.string_value.str, false)) < 0) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_CANT_SET_ERROR_SUPPRESSION_LIST,
        ER_THD(thd, ER_CANT_SET_ERROR_SUPPRESSION_LIST), self->name.str,
        &((char *)var->save_result.string_value.str)[-(i + 1)]);
    return true;
  }

  return false;
}

static bool fix_log_error_suppression_list(sys_var *self [[maybe_unused]],
                                           THD *thd [[maybe_unused]],
                                           enum_var_type type
                                           [[maybe_unused]]) {
  // syntax is OK and errcodes have messages; try to make filter rules for
  // them!
  int rr = log_builtins_filter_parse_suppression_list(
      opt_log_error_suppression_list, true);
  return (rr < 0) ? true : false;
}

static Sys_var_charptr Sys_log_error_suppression_list(
    "log_error_suppression_list",
    "Comma-separated list of error-codes. Error messages corresponding to "
    "these codes will not be included in the error log. Only events with a "
    "severity of Warning or Information can be suppressed; events with "
    "System "
    "or Error severity will always be included. Requires the filter "
    "\'log_filter_internal\' to be set in @@global.log_error_services, which "
    "is the default.",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_log_error_suppression_list),
    CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET, DEFAULT(""), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_log_error_suppression_list),
    ON_UPDATE(fix_log_error_suppression_list));

static Sys_var_bool Sys_log_queries_not_using_indexes(
    "log_queries_not_using_indexes",
    "Log queries that are executed without benefit of any index to the "
    "slow log if it is open",
    GLOBAL_VAR(opt_log_queries_not_using_indexes), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_bool Sys_log_slow_admin_statements(
    "log_slow_admin_statements",
    "Log slow OPTIMIZE, ANALYZE, ALTER and other administrative statements "
    "to "
    "the slow log if it is open.",
    GLOBAL_VAR(opt_log_slow_admin_statements), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_bool Sys_log_slow_replica_statements(
    "log_slow_replica_statements",
    "Log slow statements executed by the replication applier threads to the "
    "slow log if it is open.",
    GLOBAL_VAR(opt_log_slow_replica_statements), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_deprecated_alias Sys_log_slow_slave_statements(
    "log_slow_slave_statements", Sys_log_slow_replica_statements);

static bool update_log_throttle_queries_not_using_indexes(sys_var *, THD *thd,
                                                          enum_var_type) {
  // Check if we should print a summary of any suppressed lines to the slow
  // log now since opt_log_throttle_queries_not_using_indexes was changed.
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
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, ULONG_MAX), DEFAULT(0),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_log_throttle_queries_not_using_indexes));

static bool update_log_error_verbosity(sys_var *, THD *, enum_var_type) {
  return (log_builtins_filter_update_verbosity(log_error_verbosity) < 0);
}

static Sys_var_ulong Sys_log_error_verbosity(
    "log_error_verbosity",
    "How detailed the error log should be. "
    "1, log errors only. "
    "2, log errors and warnings. "
    "3, log errors, warnings, and notes. "
    "Messages sent to the client are unaffected by this setting.",
    PERSIST_AS_READONLY GLOBAL_VAR(log_error_verbosity), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, 3), DEFAULT(2), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(update_log_error_verbosity));

static Sys_var_enum Sys_log_timestamps(
    "log_timestamps",
    "UTC to timestamp log files in zulu time, for more concise timestamps "
    "and easier correlation of logs from servers from multiple time zones, "
    "or SYSTEM to use the system's local time. "
    "This affects only log files, not log tables, as the timestamp columns "
    "of the latter can be converted at will.",
    GLOBAL_VAR(opt_log_timestamps), CMD_LINE(REQUIRED_ARG),
    timestamp_type_names, DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr,
    /* log_error is an early option, so its timestamp format should be, too. */
    sys_var::PARSE_EARLY);

static Sys_var_bool Sys_log_statements_unsafe_for_binlog(
    "log_statements_unsafe_for_binlog",
    "Log statements considered unsafe when using statement based binary "
    "logging. This variable is deprecated and will be removed in a "
    "future version.",
    GLOBAL_VAR(opt_log_unsafe_statements), CMD_LINE(OPT_ARG), DEFAULT(true),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static bool update_cached_long_query_time(sys_var *, THD *thd,
                                          enum_var_type type) {
  if (type == OPT_SESSION)
    thd->variables.long_query_time =
        double2ulonglong(thd->variables.long_query_time_double * 1e6);
  else
    global_system_variables.long_query_time =
        double2ulonglong(global_system_variables.long_query_time_double * 1e6);
  return false;
}

static Sys_var_double Sys_long_query_time(
    "long_query_time",
    "Log all queries that have taken more than long_query_time seconds "
    "to execute to file. The argument will be treated as a decimal value "
    "with microsecond precision",
    SESSION_VAR(long_query_time_double), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(10), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(update_cached_long_query_time));

static bool fix_low_prio_updates(sys_var *, THD *thd, enum_var_type type) {
  if (type == OPT_SESSION) {
    thd->update_lock_default =
        (thd->variables.low_priority_updates ? TL_WRITE_LOW_PRIORITY
                                             : TL_WRITE);
    thd->insert_lock_default =
        (thd->variables.low_priority_updates ? TL_WRITE_LOW_PRIORITY
                                             : TL_WRITE_CONCURRENT_INSERT);
  } else
    thr_upgraded_concurrent_insert_lock =
        (global_system_variables.low_priority_updates ? TL_WRITE_LOW_PRIORITY
                                                      : TL_WRITE);
  return false;
}
static Sys_var_bool Sys_low_priority_updates(
    "low_priority_updates",
    "INSERT/DELETE/UPDATE has lower priority than selects",
    SESSION_VAR(low_priority_updates), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin_no_super),
    ON_UPDATE(fix_low_prio_updates));

static Sys_var_bool Sys_lower_case_file_system(
    "lower_case_file_system",
    "Case sensitivity of file names on the file system where the "
    "data directory is located",
    READ_ONLY NON_PERSIST GLOBAL_VAR(lower_case_file_system), NO_CMD_LINE,
    DEFAULT(false));

static Sys_var_uint Sys_lower_case_table_names(
    "lower_case_table_names",
    "If set to 1 table names are stored in lowercase on disk and table "
    "names will be case-insensitive.  Should be set to 2 if you are using "
    "a case insensitive file system",
    READ_ONLY GLOBAL_VAR(lower_case_table_names),
    CMD_LINE(OPT_ARG, OPT_LOWER_CASE_TABLE_NAMES), VALID_RANGE(0, 2),
#ifdef FN_NO_CASE_SENSE
    DEFAULT(1),
#else
    DEFAULT(0),
#endif
    BLOCK_SIZE(1));

static bool session_readonly(sys_var *self, THD *, set_var *var) {
  if (var->is_global_persist()) return false;
  my_error(ER_VARIABLE_IS_READONLY, MYF(0), "SESSION", self->name.str,
           "GLOBAL");
  return true;
}

static bool check_max_allowed_packet(sys_var *self, THD *thd, set_var *var) {
  longlong val;
  if (session_readonly(self, thd, var)) return true;

  val = var->save_result.ulonglong_value;
  if (val < (longlong)global_system_variables.net_buffer_length) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, WARN_OPTION_BELOW_LIMIT,
                        ER_THD(thd, WARN_OPTION_BELOW_LIMIT),
                        "max_allowed_packet", "net_buffer_length");
  }
  return false;
}

static Sys_var_ulong Sys_max_allowed_packet(
    "max_allowed_packet",
    "Max packet length to send to or receive from the server",
    SESSION_VAR(max_allowed_packet), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, 1024 * 1024 * 1024), DEFAULT(64 * 1024 * 1024),
    BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_max_allowed_packet));

static Sys_var_ulong Sys_replica_max_allowed_packet(
    "replica_max_allowed_packet",
    "The maximum size of packets sent from an upstream source server to this "
    "server.",
    GLOBAL_VAR(replica_max_allowed_packet), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, binary_log::max_log_event_size),
    DEFAULT(binary_log::max_log_event_size), BLOCK_SIZE(1024));

static Sys_var_deprecated_alias Sys_slave_max_allowed_packet(
    "slave_max_allowed_packet", Sys_replica_max_allowed_packet);

static Sys_var_ulonglong Sys_max_binlog_cache_size(
    "max_binlog_cache_size", "Sets the total size of the transactional cache",
    GLOBAL_VAR(max_binlog_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE, ULLONG_MAX), DEFAULT((ULLONG_MAX / IO_SIZE) * IO_SIZE),
    BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_binlog_cache_size));

static Sys_var_ulonglong Sys_max_binlog_stmt_cache_size(
    "max_binlog_stmt_cache_size", "Sets the total size of the statement cache",
    GLOBAL_VAR(max_binlog_stmt_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE, ULLONG_MAX), DEFAULT((ULLONG_MAX / IO_SIZE) * IO_SIZE),
    BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_binlog_stmt_cache_size));

static bool fix_max_binlog_size(sys_var *, THD *, enum_var_type) {
  mysql_bin_log.set_max_size(max_binlog_size);
  /*
    For multisource replication, this max size is set to all relay logs
    per channel. So, run through them
  */
  if (!max_relay_log_size) {
    Master_info *mi = nullptr;

    channel_map.wrlock();
    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      mi = it->second;
      if (mi != nullptr) mi->rli->relay_log.set_max_size(max_binlog_size);
    }
    channel_map.unlock();
  }
  return false;
}
static Sys_var_ulong Sys_max_binlog_size(
    "max_binlog_size",
    "Binary log will be rotated automatically when the size exceeds this "
    "value. Will also apply to relay logs if max_relay_log_size is 0",
    GLOBAL_VAR(max_binlog_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE, 1024 * 1024L * 1024L), DEFAULT(1024 * 1024L * 1024L),
    BLOCK_SIZE(IO_SIZE), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_max_binlog_size));

static Sys_var_ulong Sys_max_connections(
    "max_connections", "The number of simultaneous clients allowed",
    GLOBAL_VAR(max_connections), CMD_LINE(REQUIRED_ARG), VALID_RANGE(1, 100000),
    DEFAULT(MAX_CONNECTIONS_DEFAULT), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr,
    /* max_connections is used as a sizing hint by the performance schema. */
    sys_var::PARSE_EARLY);

static Sys_var_ulong Sys_max_connect_errors(
    "max_connect_errors",
    "If there is more than this number of interrupted connections from "
    "a host this host will be blocked from further connections",
    GLOBAL_VAR(max_connect_errors), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, ULONG_MAX), DEFAULT(100), BLOCK_SIZE(1));

static Sys_var_long Sys_max_digest_length(
    "max_digest_length", "Maximum length considered for digest text.",
    READ_ONLY GLOBAL_VAR(max_digest_length), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024 * 1024), DEFAULT(1024), BLOCK_SIZE(1));

static bool check_max_delayed_threads(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin_no_super(self, thd, var)) return true;
  return (!var->is_global_persist()) && var->save_result.ulonglong_value != 0 &&
         var->save_result.ulonglong_value !=
             global_system_variables.max_insert_delayed_threads;
}

// Alias for max_delayed_threads
static Sys_var_ulong Sys_max_insert_delayed_threads(
    "max_insert_delayed_threads",
    "Don't start more than this number of threads to handle INSERT "
    "DELAYED statements. If set to zero INSERT DELAYED will be not used. "
    "This variable is deprecated along with INSERT DELAYED.",
    SESSION_VAR(max_insert_delayed_threads), NO_CMD_LINE, VALID_RANGE(0, 16384),
    DEFAULT(20), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_max_delayed_threads), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_ulong Sys_max_delayed_threads(
    "max_delayed_threads",
    "Don't start more than this number of threads to handle INSERT "
    "DELAYED statements. If set to zero INSERT DELAYED will be not used. "
    "This variable is deprecated along with INSERT DELAYED.",
    SESSION_VAR(max_insert_delayed_threads), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 16384), DEFAULT(20), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_max_delayed_threads), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_ulong Sys_max_error_count(
    "max_error_count", "Max number of errors/warnings to store for a statement",
    HINT_UPDATEABLE SESSION_VAR(max_error_count), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 65535), DEFAULT(DEFAULT_ERROR_COUNT), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin_no_super));

static Sys_var_ulonglong Sys_max_heap_table_size(
    "max_heap_table_size",
    "Don't allow creation of heap tables bigger than this",
    HINT_UPDATEABLE SESSION_VAR(max_heap_table_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(16384, (ulonglong) ~(intptr)0), DEFAULT(16 * 1024 * 1024),
    BLOCK_SIZE(1024));

// relies on assert(sizeof(my_thread_id) == 4);
static Sys_var_uint Sys_pseudo_thread_id(
    "pseudo_thread_id", "This variable is for internal server use",
    SESSION_ONLY(pseudo_thread_id), NO_CMD_LINE, VALID_RANGE(0, UINT_MAX32),
    DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_session_admin));

static bool fix_max_join_size(sys_var *self, THD *thd, enum_var_type type) {
  System_variables *sv = (self->is_global_persist(type))
                             ? &global_system_variables
                             : &thd->variables;
  if (sv->max_join_size == HA_POS_ERROR)
    sv->option_bits |= OPTION_BIG_SELECTS;
  else
    sv->option_bits &= ~OPTION_BIG_SELECTS;
  return false;
}
static Sys_var_harows Sys_max_join_size(
    "max_join_size",
    "Joins that are probably going to read more than max_join_size "
    "records return an error",
    HINT_UPDATEABLE SESSION_VAR(max_join_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, HA_POS_ERROR), DEFAULT(HA_POS_ERROR), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_max_join_size));

static Sys_var_ulong Sys_max_seeks_for_key(
    "max_seeks_for_key",
    "Limit assumed max number of seeks when looking up rows based on a key",
    HINT_UPDATEABLE SESSION_VAR(max_seeks_for_key), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, ULONG_MAX), DEFAULT(ULONG_MAX), BLOCK_SIZE(1));

static Sys_var_ulong Sys_max_length_for_sort_data(
    "max_length_for_sort_data",
    "This variable is deprecated and will be removed in a future release.",
    HINT_UPDATEABLE SESSION_VAR(max_length_for_sort_data),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(4, 8192 * 1024L), DEFAULT(4096),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_ulong Sys_max_points_in_geometry(
    "max_points_in_geometry", "Maximum number of points in a geometry",
    HINT_UPDATEABLE SESSION_VAR(max_points_in_geometry), CMD_LINE(OPT_ARG),
    VALID_RANGE(3, 1024 * 1024L), DEFAULT(64 * 1024), BLOCK_SIZE(1));

static PolyLock_mutex PLock_prepared_stmt_count(&LOCK_prepared_stmt_count);

static Sys_var_ulong Sys_max_prepared_stmt_count(
    "max_prepared_stmt_count",
    "Maximum number of prepared statements in the server",
    GLOBAL_VAR(max_prepared_stmt_count), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, num_prepared_stmt_limit), DEFAULT(16382), BLOCK_SIZE(1),
    &PLock_prepared_stmt_count, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), nullptr,
    /* max_prepared_stmt_count is used as a sizing hint by the performance
       schema. */
    sys_var::PARSE_EARLY);

static bool fix_max_relay_log_size(sys_var *, THD *, enum_var_type) {
  Master_info *mi = nullptr;

  channel_map.wrlock();
  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;

    if (mi != nullptr)
      mi->rli->relay_log.set_max_size(max_relay_log_size ? max_relay_log_size
                                                         : max_binlog_size);
  }
  channel_map.unlock();
  return false;
}
static Sys_var_ulong Sys_max_relay_log_size(
    "max_relay_log_size",
    "If non-zero: relay log will be rotated automatically when the "
    "size exceeds this value; if zero: when the size "
    "exceeds max_binlog_size",
    GLOBAL_VAR(max_relay_log_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1024L * 1024 * 1024), DEFAULT(0), BLOCK_SIZE(IO_SIZE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_max_relay_log_size));

static Sys_var_ulong Sys_max_sort_length(
    "max_sort_length",
    "The number of bytes to use when sorting long values with PAD SPACE "
    "collations (only the first max_sort_length bytes of each value are "
    "used; the rest are ignored)",
    HINT_UPDATEABLE SESSION_VAR(max_sort_length), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(4, 8192 * 1024L), DEFAULT(1024), BLOCK_SIZE(1));

static Sys_var_ulong Sys_max_sp_recursion_depth(
    "max_sp_recursion_depth", "Maximum stored procedure recursion depth",
    SESSION_VAR(max_sp_recursion_depth), CMD_LINE(OPT_ARG), VALID_RANGE(0, 255),
    DEFAULT(0), BLOCK_SIZE(1));

// non-standard session_value_ptr() here
static Sys_var_max_user_conn Sys_max_user_connections(
    "max_user_connections",
    "The maximum number of active connections for a single user "
    "(0 = no limit)",
    SESSION_VAR(max_user_connections), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(session_readonly));

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
    VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_session_admin_no_super));

#ifdef _WIN32
static Sys_var_bool Sys_named_pipe("named_pipe", "Enable the named pipe (NT)",
                                   READ_ONLY NON_PERSIST
                                       GLOBAL_VAR(opt_enable_named_pipe),
                                   CMD_LINE(OPT_ARG), DEFAULT(false));

static PolyLock_rwlock PLock_named_pipe_full_access_group(
    &LOCK_named_pipe_full_access_group);
static bool check_named_pipe_full_access_group(sys_var *self, THD *,
                                               set_var *var) {
  if (!var->value) return false;  // DEFAULT is ok

  if (!is_valid_named_pipe_full_access_group(
          var->save_result.string_value.str)) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str,
             var->save_result.string_value.str);
    return true;
  }
  return false;
}
static bool fix_named_pipe_full_access_group(sys_var *, THD *, enum_var_type) {
  return update_named_pipe_full_access_group(named_pipe_full_access_group);
}
static Sys_var_charptr Sys_named_pipe_full_access_group(
    "named_pipe_full_access_group",
    "Name of Windows group granted full access to the named pipe",
    GLOBAL_VAR(named_pipe_full_access_group),
    CMD_LINE(REQUIRED_ARG, OPT_NAMED_PIPE_FULL_ACCESS_GROUP), IN_FS_CHARSET,
    DEFAULT(DEFAULT_NAMED_PIPE_FULL_ACCESS_GROUP),
    &PLock_named_pipe_full_access_group, NOT_IN_BINLOG,
    ON_CHECK(check_named_pipe_full_access_group),
    ON_UPDATE(fix_named_pipe_full_access_group));
#endif

static bool check_net_buffer_length(sys_var *self, THD *thd, set_var *var) {
  longlong val;
  if (session_readonly(self, thd, var)) return true;

  val = var->save_result.ulonglong_value;
  if (val > (longlong)global_system_variables.max_allowed_packet) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, WARN_OPTION_BELOW_LIMIT,
                        ER_THD(thd, WARN_OPTION_BELOW_LIMIT),
                        "max_allowed_packet", "net_buffer_length");
  }
  return false;
}
static Sys_var_ulong Sys_net_buffer_length(
    "net_buffer_length", "Buffer length for TCP/IP and socket communication",
    SESSION_VAR(net_buffer_length), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, 1024 * 1024), DEFAULT(16384), BLOCK_SIZE(1024),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_net_buffer_length));

static bool fix_net_read_timeout(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type)) {
    // net_buffer_length is a specific property for the classic protocols
    if (!thd->is_classic_protocol()) {
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
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_net_read_timeout));

static bool fix_net_write_timeout(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type)) {
    // net_read_timeout is a specific property for the classic protocols
    if (!thd->is_classic_protocol()) {
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
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_net_write_timeout));

static bool fix_net_retry_count(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type)) {
    // net_write_timeout is a specific property for the classic protocols
    if (!thd->is_classic_protocol()) {
      my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
      return true;
    }
    thd->get_protocol_classic()->get_net()->retry_count =
        thd->variables.net_retry_count;
  }
  return false;
}
static Sys_var_ulong Sys_net_retry_count(
    "net_retry_count",
    "If a read on a communication port is interrupted, retry this "
    "many times before giving up",
    SESSION_VAR(net_retry_count), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, ULONG_MAX), DEFAULT(MYSQLD_NET_RETRY_COUNT), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_net_retry_count));

static Sys_var_bool Sys_new_mode("new",
                                 "Use very new possible \"unsafe\" functions",
                                 SESSION_VAR(new_mode), CMD_LINE(OPT_ARG, 'n'),
                                 DEFAULT(false));

static Sys_var_bool Sys_old_mode("old", "Use compatible behavior",
                                 READ_ONLY GLOBAL_VAR(old_mode),
                                 CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_bool Sys_old_alter_table("old_alter_table",
                                        "Use old, non-optimized alter table",
                                        SESSION_VAR(old_alter_table),
                                        CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_ulong Sys_open_files_limit(
    "open_files_limit",
    "If this is not 0, then mysqld will use this value to reserve file "
    "descriptors to use with setrlimit(). If this value is 0 then mysqld "
    "will reserve max_connections*5 or max_connections + table_open_cache*2 "
    "(whichever is larger) number of file descriptors",
    READ_ONLY GLOBAL_VAR(open_files_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, OS_FILE_LIMIT), DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr,
    /* open_files_limit is used as a sizing hint by the performance schema. */
    sys_var::PARSE_EARLY);

/// @todo change to enum
static Sys_var_ulong Sys_optimizer_prune_level(
    "optimizer_prune_level",
    "Controls the heuristic(s) applied during query optimization to prune "
    "less-promising partial plans from the optimizer search space. "
    "Meaning: 0 - do not apply any heuristic, thus perform exhaustive "
    "search; 1 - prune plans based on number of retrieved rows",
    HINT_UPDATEABLE SESSION_VAR(optimizer_prune_level), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 1), DEFAULT(1), BLOCK_SIZE(1));

static Sys_var_ulong Sys_optimizer_search_depth(
    "optimizer_search_depth",
    "Maximum depth of search performed by the query optimizer. Values "
    "larger than the number of relations in a query result in better "
    "query plans, but take longer to compile a query. Values smaller "
    "than the number of tables in a relation result in faster "
    "optimization, but may produce very bad query plans. If set to 0, "
    "the system will automatically pick a reasonable value",
    HINT_UPDATEABLE SESSION_VAR(optimizer_search_depth), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, MAX_TABLES + 1), DEFAULT(MAX_TABLES + 1), BLOCK_SIZE(1));

static Sys_var_ulong Sys_optimizer_max_subgraph_pairs(
    "optimizer_max_subgraph_pairs",
    "Maximum depth of subgraph pairs a query can have before the "
    "hypergraph join optimizer starts reducing the search space "
    "heuristically. Larger values may result in better query plans "
    "for large queries, but also more time and memory spent during planning. "
    "Increasing this larger than the actual number of subgraph pairs "
    "in the query will have no further effect. "
    "Ignored by the old (non-hypergraph) join optimizer",
    HINT_UPDATEABLE SESSION_VAR(optimizer_max_subgraph_pairs),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(1, INT_MAX), DEFAULT(100000),
    BLOCK_SIZE(1));

static Sys_var_ulong Sys_range_optimizer_max_mem_size(
    "range_optimizer_max_mem_size",
    "Maximum amount of memory used by the range optimizer "
    "to allocate predicates during range analysis. "
    "The larger the number, more memory may be consumed during "
    "range analysis. If the value is too low to completed range "
    "optimization of a query, index range scan will not be "
    "considered for this query. A value of 0 means range optimizer "
    "does not have any cap on memory. ",
    HINT_UPDATEABLE SESSION_VAR(range_optimizer_max_mem_size),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, ULONG_MAX), DEFAULT(8388608),
    BLOCK_SIZE(1));

static bool limit_parser_max_mem_size(sys_var *, THD *thd, set_var *var) {
  if (var->is_global_persist()) return false;
  ulonglong val = var->save_result.ulonglong_value;
  if (val > global_system_variables.parser_max_mem_size) {
    if (thd->security_context()->check_access(SUPER_ACL)) return false;
    var->save_result.ulonglong_value =
        global_system_variables.parser_max_mem_size;
    return throw_bounds_warning(thd, "parser_max_mem_size",
                                true,  // fixed
                                true,  // is_unsigned
                                val);
  }
  return false;
}

constexpr size_t max_mem_sz = std::numeric_limits<size_t>::max();

static Sys_var_ulonglong Sys_histogram_generation_max_mem_size(
    "histogram_generation_max_mem_size",
    "Maximum amount of memory available for generating histograms",
    SESSION_VAR(histogram_generation_max_mem_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1000000, max_mem_sz), DEFAULT(20000000), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin),
    ON_UPDATE(nullptr));

/*
  Need at least 400Kb to get through bootstrap.
  Need at least 8Mb to get through mtr check testcase, which does
    SELECT * FROM INFORMATION_SCHEMA.VIEWS
*/
static Sys_var_ulonglong Sys_parser_max_mem_size(
    "parser_max_mem_size", "Maximum amount of memory available to the parser",
    SESSION_VAR(parser_max_mem_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(10 * 1000 * 1000, max_mem_sz), DEFAULT(max_mem_sz),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(limit_parser_max_mem_size), ON_UPDATE(nullptr));

/*
  There is no call on Sys_var_integer::do_check() for 'set xxx=default';
  The predefined default for parser_max_mem_size is "infinite".
  Update it in case we have seen option maximum-parser-max-mem-size
  Also update global_system_variables, so 'SELECT parser_max_mem_size'
  reports correct data.
*/
export void update_parser_max_mem_size() {
  const ulonglong max_max = max_system_variables.parser_max_mem_size;
  if (max_max == max_mem_sz) return;
  // In case parser-max-mem-size is also set:
  const ulonglong new_val =
      std::min(max_max, global_system_variables.parser_max_mem_size);
  Sys_parser_max_mem_size.update_default(new_val);
  global_system_variables.parser_max_mem_size = new_val;
}

static bool check_optimizer_switch(sys_var *, THD *thd [[maybe_unused]],
                                   set_var *var) {
  const bool current_hypergraph_optimizer =
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);
  const bool want_hypergraph_optimizer =
      var->save_result.ulonglong_value & OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER;

  if (current_hypergraph_optimizer && !want_hypergraph_optimizer) {
    // Don't turn off the hypergraph optimizer on set optimizer_switch=DEFAULT.
    // This is so that mtr --hypergraph should not be easily cancelled in the
    // middle of a test, unless the test explicitly meant it.
    if (var->value == nullptr) {
      var->save_result.ulonglong_value |= OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER;
    }
  } else if (!current_hypergraph_optimizer && want_hypergraph_optimizer) {
#ifdef WITH_HYPERGRAPH_OPTIMIZER
    // Allow, with a warning.
    push_warning(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                 ER_THD(thd, ER_WARN_HYPERGRAPH_EXPERIMENTAL));
    return false;
#else
    // Disallow; the hypergraph optimizer is not ready for production yet.
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "use in non-debug builds");
    return true;
#endif
  }
  return false;
}

/**
  @note
  @b BEWARE! These must have the same order as the \#defines in sql_const.h!
*/
static const char *optimizer_switch_names[] = {
    "index_merge",
    "index_merge_union",
    "index_merge_sort_union",
    "index_merge_intersection",
    "engine_condition_pushdown",
    "index_condition_pushdown",
    "mrr",
    "mrr_cost_based",
    "block_nested_loop",
    "batched_key_access",
    "materialization",
    "semijoin",
    "loosescan",
    "firstmatch",
    "duplicateweedout",
    "subquery_materialization_cost_based",
    "use_index_extensions",
    "condition_fanout_filter",
    "derived_merge",
    "use_invisible_indexes",
    "skip_scan",
    "hash_join",
    "subquery_to_derived",
    "prefer_ordering_index",
    "hypergraph_optimizer",  // Deliberately not documented below.
    "derived_condition_pushdown",
    "default",
    NullS};
static Sys_var_flagset Sys_optimizer_switch(
    "optimizer_switch",
    "optimizer_switch=option=val[,option=val...], where option is one of "
    "{index_merge, index_merge_union, index_merge_sort_union, "
    "index_merge_intersection, engine_condition_pushdown, "
    "index_condition_pushdown, mrr, mrr_cost_based"
    ", materialization, semijoin, loosescan, firstmatch, duplicateweedout,"
    " subquery_materialization_cost_based, skip_scan,"
    " block_nested_loop, batched_key_access, use_index_extensions,"
    " condition_fanout_filter, derived_merge, hash_join,"
    " subquery_to_derived, prefer_ordering_index,"
    " derived_condition_pushdown} and val is one of "
    "{on, off, default}",
    HINT_UPDATEABLE SESSION_VAR(optimizer_switch), CMD_LINE(REQUIRED_ARG),
    optimizer_switch_names, DEFAULT(OPTIMIZER_SWITCH_DEFAULT), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_optimizer_switch), ON_UPDATE(nullptr));

static PolyLock_mutex PLock_global_conn_mem_limit(&LOCK_global_conn_mem_limit);

static Sys_var_ulonglong Sys_global_connection_memory_limit(
    "global_connection_memory_limit",
    "Maximum amount of memory all connections can consume",
    GLOBAL_VAR(global_conn_mem_limit), CMD_LINE(REQUIRED_ARG),
#ifndef NDEBUG
    VALID_RANGE(1, max_mem_sz), DEFAULT(max_mem_sz),
#else
    VALID_RANGE(1024 * 1024 * 16, max_mem_sz), DEFAULT(max_mem_sz),
#endif
    BLOCK_SIZE(1), &PLock_global_conn_mem_limit, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr));

static Sys_var_ulonglong Sys_connection_memory_limit(
    "connection_memory_limit",
    "Maximum amount of memory connection can consume",
    SESSION_VAR(conn_mem_limit), CMD_LINE(REQUIRED_ARG),
#ifndef NDEBUG
    VALID_RANGE(1, max_mem_sz), DEFAULT(max_mem_sz),
#else
    VALID_RANGE(1024 * 1024 * 2, max_mem_sz), DEFAULT(max_mem_sz),
#endif
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin),
    ON_UPDATE(nullptr));

static Sys_var_ulong Sys_connection_memory_chunk_size(
    "connection_memory_chunk_size",
    "Chunk size regulating frequency of updating the global memory counter",
    SESSION_VAR(conn_mem_chunk_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, 1024 * 1024 * 512), DEFAULT(8192), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin),
    ON_UPDATE(nullptr));

static Sys_var_bool Sys_connection_global_memory_tracking(
    "global_connection_memory_tracking",
    "Enable updating the global memory counter and checking "
    "the global connection memory limit exceeding",
    SESSION_VAR(conn_global_mem_tracking), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin),
    ON_UPDATE(nullptr));

static Sys_var_bool Sys_var_end_markers_in_json(
    "end_markers_in_json",
    "In JSON output (\"EXPLAIN FORMAT=JSON\" and optimizer trace), "
    "if variable is set to 1, repeats the structure's key (if it has one) "
    "near the closing bracket",
    HINT_UPDATEABLE SESSION_VAR(end_markers_in_json), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_flagset Sys_optimizer_trace(
    "optimizer_trace",
    "Controls tracing of the Optimizer:"
    " optimizer_trace=option=val[,option=val...], where option is one of"
    " {enabled, one_line}"
    " and val is one of {on, default}",
    SESSION_VAR(optimizer_trace), CMD_LINE(REQUIRED_ARG),
    Opt_trace_context::flag_names, DEFAULT(Opt_trace_context::FLAG_DEFAULT));
// @see set_var::is_var_optimizer_trace()
export sys_var *Sys_optimizer_trace_ptr = &Sys_optimizer_trace;

/**
  Note how "misc" is not here: it is not accessible to the user; disabling
  "misc" would disable the top object, which would make an empty trace.
*/
static Sys_var_flagset Sys_optimizer_trace_features(
    "optimizer_trace_features",
    "Enables/disables tracing of selected features of the Optimizer:"
    " optimizer_trace_features=option=val[,option=val...], where option is "
    "one "
    "of"
    " {greedy_search, range_optimizer, dynamic_range, repeated_subselect}"
    " and val is one of {on, off, default}",
    SESSION_VAR(optimizer_trace_features), CMD_LINE(REQUIRED_ARG),
    Opt_trace_context::feature_names,
    DEFAULT(Opt_trace_context::default_features));

/** Delete all old optimizer traces */
static bool optimizer_trace_update(sys_var *, THD *thd, enum_var_type) {
  thd->opt_trace.reset();
  return false;
}

static Sys_var_long Sys_optimizer_trace_offset(
    "optimizer_trace_offset",
    "Offset of first optimizer trace to show; see manual",
    SESSION_VAR(optimizer_trace_offset), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(LONG_MIN, LONG_MAX), DEFAULT(-1), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(optimizer_trace_update));

static Sys_var_long Sys_optimizer_trace_limit(
    "optimizer_trace_limit", "Maximum number of shown optimizer traces",
    SESSION_VAR(optimizer_trace_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, LONG_MAX), DEFAULT(1), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(optimizer_trace_update));

static Sys_var_ulong Sys_optimizer_trace_max_mem_size(
    "optimizer_trace_max_mem_size",
    "Maximum allowed cumulated size of stored optimizer traces",
    SESSION_VAR(optimizer_trace_max_mem_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULONG_MAX), DEFAULT(1024 * 1024), BLOCK_SIZE(1));

static Sys_var_charptr Sys_pid_file(
    "pid_file", "Pid file used by safe_mysqld",
    READ_ONLY NON_PERSIST GLOBAL_VAR(pidfile_name_ptr), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(pidfile_name));

static Sys_var_charptr Sys_plugin_dir(
    "plugin_dir", "Directory for plugins",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_plugin_dir_ptr),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_uint Sys_port(
    "port",
    "Port number to use for connection or 0 to default to, "
    "my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
    "/etc/services, "
#endif
    "built-in default (" STRINGIFY_ARG(MYSQL_PORT) "), whatever comes first",
    READ_ONLY NON_PERSIST GLOBAL_VAR(mysqld_port), CMD_LINE(REQUIRED_ARG, 'P'),
    VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_ulong Sys_preload_buff_size(
    "preload_buffer_size",
    "The size of the buffer that is allocated when preloading indexes",
    SESSION_VAR(preload_buff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, 1024 * 1024 * 1024), DEFAULT(32768), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_session_admin_no_super));

static Sys_var_uint Sys_protocol_version(
    "protocol_version",
    "The version of the client/server protocol used by the MySQL server",
    READ_ONLY NON_PERSIST GLOBAL_VAR(protocol_version), NO_CMD_LINE,
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
    HINT_UPDATEABLE SESSION_VAR(read_buff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE * 2, INT_MAX32), DEFAULT(128 * 1024),
    BLOCK_SIZE(IO_SIZE));

static bool check_read_only(sys_var *, THD *thd, set_var *) {
  /* Prevent self dead-lock */
  if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction()) {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    return true;
  }
  return false;
}

static bool check_require_secure_transport(sys_var *, THD *,
                                           set_var *var [[maybe_unused]]) {
#if !defined(_WIN32)
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

  if (!var->save_result.ulonglong_value) return false;
  if (have_ssl() || opt_enable_shared_memory) return false;
  /* reject if SSL and shared memory are both disabled: */
  my_error(ER_NO_SECURE_TRANSPORTS_CONFIGURED, MYF(0));
  return true;

#endif
}

static void event_scheduler_restart(THD *thd) {
  /*
    Restart event scheduler if needed.

    At present, turning on SUPER_READ_ONLY means that we
    can no longer acquire an MDL to update mysql.*.
    As a result of this, updating the "last run at ..."
    timestamp of events fails, and the event scheduler
    shuts down when trying to do so.

    As a convenience, we restart the event scheduler when
    [SUPER_]READ_ONLY is turned off while the scheduler is
    enabled (in the settings), but not actually running.
  */
  if (Events::opt_event_scheduler == Events::EVENTS_ON) {
    bool evsched_error;       // Did we fail to start the event scheduler?
    int evsched_errcode = 0;  // If we failed, what was the actual error code?

    /*
      We must not hold the lock while starting the event scheduler,
      as that will internally try to take the lock while creating a THD.
    */
    mysql_mutex_unlock(&LOCK_global_system_variables);
    evsched_error = Events::start(&evsched_errcode);
    mysql_mutex_lock(&LOCK_global_system_variables);

    if (evsched_error) {
      /*
        The user requested a change of super_read_only.
        That change succeeded, so we do not signal a failure here,
        since it is only the side-effect/convenience of restarting
        the event scheduler that failed.
        We do however notify them of that failure, since we're
        just that nice.
        We also do not modify opt_event_scheduler, since user
        intent has not changed. If this policy ever changes,
        opt_event_scheduler should probably be unset when the
        event scheduler shuts down.
      */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_EVENT_SET_VAR_ERROR,
                          ER_THD(thd, ER_EVENT_SET_VAR_ERROR), evsched_errcode);
    }
  }
}

static bool fix_read_only(sys_var *self, THD *thd, enum_var_type) {
  bool result = true;
  bool new_read_only = read_only;  // make a copy before releasing a mutex
  DBUG_TRACE;

  /*
    If we're not newly turning on READ_ONLY, we don't have to worry
    about locks.
  */
  if (read_only == false || read_only == opt_readonly) {
    opt_readonly = read_only;

    /*
      If we're turning off READ_ONLY here, turn off
      SUPER_READ_ONLY as well (if on).
    */
    if (opt_super_readonly && !read_only) {
      opt_super_readonly = false;
      super_read_only = false;

      // Do this last as it temporarily releases the global sys-var lock.
      event_scheduler_restart(thd);
    }
    return false;
  }

  /*
    Check whether we can change read_only state without causing a deadlock.

    Not to be confused with check_readonly(), which checks in a
    standardized way whether the current settings of opt_readonly
    and opt_super_readonly prohibit certain operations.
  */
  if (check_read_only(self, thd, nullptr))  // just in case
    goto end;

  if (thd->global_read_lock.is_acquired()) {
    /*
      This connection already holds the global read lock.
      This can be the case with:
      - FLUSH TABLES WITH READ LOCK
      - SET GLOBAL READ_ONLY = 1
    */
    opt_readonly = read_only;

    if (opt_super_readonly && !read_only) {
      opt_super_readonly = false;
      super_read_only = false;

      // Do this last as it temporarily releases the global sys-var lock.
      event_scheduler_restart(thd);
    }
    return false;
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

  read_only = opt_readonly;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (thd->global_read_lock.lock_global_read_lock(thd))
    goto end_with_mutex_unlock;

  if ((result = thd->global_read_lock.make_global_read_lock_block_commit(thd)))
    goto end_with_read_lock;

  /* Change the opt_readonly system variable, safe because the lock is held */
  opt_readonly = new_read_only;

  result = false;

end_with_read_lock:
  /* Release the lock */
  thd->global_read_lock.unlock_global_read_lock(thd);
end_with_mutex_unlock:
  mysql_mutex_lock(&LOCK_global_system_variables);
end:
  read_only = opt_readonly;
  return result;
}

static bool fix_super_read_only(sys_var *, THD *thd, enum_var_type type) {
  DBUG_TRACE;

  /* return if no changes: */
  if (super_read_only == opt_super_readonly) return false;

  /* return immediately if turning super_read_only OFF: */
  if (super_read_only == false) {
    opt_super_readonly = false;

    // Do this last as it temporarily releases the global sys-var lock.
    event_scheduler_restart(thd);

    return false;
  }
  bool result = true;
  bool new_super_read_only =
      super_read_only; /* make a copy before releasing a mutex */

  /* set read_only to ON if it is OFF, letting fix_read_only()
     handle its own locking needs
  */
  if (!opt_readonly) {
    read_only = true;
    if ((result = fix_read_only(nullptr, thd, type))) goto end;
  }

  /* if we already have global read lock, set super_read_only
     and return immediately:
  */
  if (thd->global_read_lock.is_acquired()) {
    opt_super_readonly = super_read_only;
    return false;
  }

  /* now we're turning ON super_read_only: */
  super_read_only = opt_super_readonly;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (thd->global_read_lock.lock_global_read_lock(thd))
    goto end_with_mutex_unlock;

  if ((result = thd->global_read_lock.make_global_read_lock_block_commit(thd)))
    goto end_with_read_lock;
  opt_super_readonly = new_super_read_only;
  result = false;

end_with_read_lock:
  /* Release the lock */
  thd->global_read_lock.unlock_global_read_lock(thd);
end_with_mutex_unlock:
  mysql_mutex_lock(&LOCK_global_system_variables);
end:
  super_read_only = opt_super_readonly;
  return result;
}

static Sys_var_bool Sys_require_secure_transport(
    "require_secure_transport",
    "When this option is enabled, connections attempted using insecure "
    "transport will be rejected.  Secure transports are SSL/TLS, "
    "Unix socket or Shared Memory (on Windows).",
    GLOBAL_VAR(opt_require_secure_transport), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_require_secure_transport),
    ON_UPDATE(nullptr));

/**
  The read_only boolean is always equal to the opt_readonly boolean except
  during fix_read_only(); when that function is entered, opt_readonly is
  the pre-update value and read_only is the post-update value.
  fix_read_only() compares them and runs needed operations for the
  transition (especially when transitioning from false to true) and
  synchronizes both booleans in the end.
*/
static Sys_var_bool Sys_readonly(
    "read_only",
    "Make all non-temporary tables read-only, with the exception for "
    "replication applier threads and users with the SUPER privilege.",
    GLOBAL_VAR(read_only), CMD_LINE(OPT_ARG), DEFAULT(false), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_read_only), ON_UPDATE(fix_read_only));

/**
Setting super_read_only to ON triggers read_only to also be set to ON.
*/
static Sys_var_bool Sys_super_readonly(
    "super_read_only",
    "Make all non-temporary tables read-only, with the exception for "
    "replication applier threads.  Users with the SUPER privilege are "
    "affected, unlike read_only.  Setting super_read_only to ON "
    "also sets read_only to ON.",
    GLOBAL_VAR(super_read_only), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_super_read_only));

// Small lower limit to be able to test MRR
static Sys_var_ulong Sys_read_rnd_buff_size(
    "read_rnd_buffer_size",
    "When reading rows in sorted order after a sort, the rows are read "
    "through this buffer to avoid a disk seeks",
    HINT_UPDATEABLE SESSION_VAR(read_rnd_buff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, INT_MAX32), DEFAULT(256 * 1024), BLOCK_SIZE(1));

static Sys_var_ulong Sys_div_precincrement(
    "div_precision_increment",
    "Precision of the result of '/' "
    "operator will be increased on that value",
    HINT_UPDATEABLE SESSION_VAR(div_precincrement), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, DECIMAL_MAX_SCALE), DEFAULT(4), BLOCK_SIZE(1));

static Sys_var_uint Sys_eq_range_index_dive_limit(
    "eq_range_index_dive_limit",
    "The optimizer will use existing index statistics instead of "
    "doing index dives for equality ranges if the number of equality "
    "ranges for the index is larger than or equal to this number. "
    "If set to 0, index dives are always used.",
    HINT_UPDATEABLE SESSION_VAR(eq_range_index_dive_limit),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, UINT_MAX32), DEFAULT(200),
    BLOCK_SIZE(1));

static Sys_var_ulong Sys_range_alloc_block_size(
    "range_alloc_block_size",
    "Allocation block size for storing ranges during optimization",
    HINT_UPDATEABLE SESSION_VAR(range_alloc_block_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(RANGE_ALLOC_BLOCK_SIZE, UINT32_MAX),
    DEFAULT(RANGE_ALLOC_BLOCK_SIZE), BLOCK_SIZE(1024));

static bool fix_thd_mem_root(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type))
    thd->mem_root->set_block_size(thd->variables.query_alloc_block_size);
  return false;
}
static Sys_var_ulong Sys_query_alloc_block_size(
    "query_alloc_block_size",
    "Allocation block size for query parsing and execution",
    SESSION_VAR(query_alloc_block_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, UINT_MAX32), DEFAULT(QUERY_ALLOC_BLOCK_SIZE),
    BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_thd_mem_root));

static Sys_var_ulong Sys_query_prealloc_size(
    "query_prealloc_size", "Persistent buffer for query parsing and execution",
    SESSION_VAR(query_prealloc_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(QUERY_ALLOC_PREALLOC_SIZE, ULONG_MAX),
    DEFAULT(QUERY_ALLOC_PREALLOC_SIZE), BLOCK_SIZE(1024), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), DEPRECATED_VAR(""));

#if defined(_WIN32)
static Sys_var_bool Sys_shared_memory(
    "shared_memory", "Enable the shared memory",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_enable_shared_memory),
    CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_charptr Sys_shared_memory_base_name(
    "shared_memory_base_name", "Base name of shared memory",
    READ_ONLY NON_PERSIST GLOBAL_VAR(shared_memory_base_name),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(0));
#endif

// this has to be NO_CMD_LINE as the command-line option has a different name
static Sys_var_bool Sys_skip_external_locking(
    "skip_external_locking", "Don't use system (external) locking",
    READ_ONLY NON_PERSIST GLOBAL_VAR(my_disable_locking), NO_CMD_LINE,
    DEFAULT(true));

static Sys_var_bool Sys_skip_networking(
    "skip_networking", "Don't allow connection with TCP/IP",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_disable_networking), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_bool Sys_skip_name_resolve(
    "skip_name_resolve",
    "Don't resolve hostnames. All hostnames are IP's or 'localhost'.",
    READ_ONLY GLOBAL_VAR(opt_skip_name_resolve),
    CMD_LINE(OPT_ARG, OPT_SKIP_RESOLVE), DEFAULT(false));

static Sys_var_bool Sys_skip_show_database(
    "skip_show_database", "Don't allow 'SHOW DATABASE' commands",
    READ_ONLY GLOBAL_VAR(opt_skip_show_db), CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_charptr Sys_socket(
    "socket", "Socket file to use for connection",
    READ_ONLY NON_PERSIST GLOBAL_VAR(mysqld_unix_port), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_ulong Sys_thread_stack(
    "thread_stack", "The stack size for each thread",
    READ_ONLY GLOBAL_VAR(my_thread_stack_size), CMD_LINE(REQUIRED_ARG),
#if defined(__clang__) && defined(HAVE_UBSAN)
    // Clang with DEBUG needs more stack, esp. with UBSAN.
    VALID_RANGE(DEFAULT_THREAD_STACK, ULONG_MAX),
#else
    VALID_RANGE(128 * 1024, ULONG_MAX),
#endif
    DEFAULT(DEFAULT_THREAD_STACK), BLOCK_SIZE(1024));

static Sys_var_charptr Sys_tmpdir(
    "tmpdir",
    "Path for temporary files. Several paths may "
    "be specified, separated by a "
#if defined(_WIN32)
    "semicolon (;)"
#else
    "colon (:)"
#endif
    ", in this case they are used in a round-robin fashion",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_mysql_tmpdir),
    CMD_LINE(REQUIRED_ARG, 't'), IN_FS_CHARSET, DEFAULT(nullptr));

static bool fix_trans_mem_root(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type))
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
    BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_trans_mem_root));

static Sys_var_ulong Sys_trans_prealloc_size(
    "transaction_prealloc_size",
    "Persistent buffer for transactions to be stored in binary log",
    SESSION_VAR(trans_prealloc_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, 128 * 1024), DEFAULT(TRANS_ALLOC_PREALLOC_SIZE),
    BLOCK_SIZE(1024), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static const char *thread_handling_names[] = {
    "one-thread-per-connection", "no-threads", "loaded-dynamically", nullptr};
static Sys_var_enum Sys_thread_handling(
    "thread_handling",
    "Define threads usage for handling queries, one of "
    "one-thread-per-connection, no-threads, loaded-dynamically",
    READ_ONLY GLOBAL_VAR(Connection_handler_manager::thread_handling),
    CMD_LINE(REQUIRED_ARG), thread_handling_names, DEFAULT(0));

static Sys_var_charptr Sys_secure_file_priv(
    "secure_file_priv",
    "Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files "
    "within specified directory",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_secure_file_priv),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT(DEFAULT_SECURE_FILE_PRIV_DIR));

static bool fix_server_id(sys_var *, THD *thd, enum_var_type) {
  // server_id is 'MYSQL_PLUGIN_IMPORT ulong'
  // So we cast here, rather than change its type.
  server_id_supplied = true;
  thd->server_id = static_cast<uint32>(server_id);
  return false;
}
static Sys_var_ulong Sys_server_id(
    "server_id",
    "Uniquely identifies the server instance in the community of "
    "replication partners",
    PERSIST_AS_READONLY GLOBAL_VAR(server_id),
    CMD_LINE(REQUIRED_ARG, OPT_SERVER_ID), VALID_RANGE(0, UINT_MAX32),
    DEFAULT(1), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_server_id));

static Sys_var_charptr Sys_server_uuid(
    "server_uuid", "Uniquely identifies the server instance in the universe",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_uuid_ptr), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(server_uuid));

#if defined(HAVE_BUILD_ID_SUPPORT)
static Sys_var_charptr Sys_server_build_id(
    "build_id",
    "A unique Build ID generated by the linker, a 160 bit sha1 signature.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_build_id_ptr), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(server_build_id));
#endif

static Sys_var_uint Sys_server_id_bits(
    "server_id_bits", "Set number of significant bits in server-id",
    GLOBAL_VAR(opt_server_id_bits), CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 32),
    DEFAULT(32), BLOCK_SIZE(1));

static Sys_var_int32 Sys_regexp_time_limit(
    "regexp_time_limit",
    "Timeout for regular expressions matches, in steps of the match "
    "engine, typically on the order of milliseconds.",
    GLOBAL_VAR(opt_regexp_time_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, INT32_MAX), DEFAULT(32), BLOCK_SIZE(1));

static Sys_var_int32 Sys_regexp_stack_limit(
    "regexp_stack_limit", "Stack size limit for regular expressions matches",
    GLOBAL_VAR(opt_regexp_stack_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, INT32_MAX), DEFAULT(8000000), BLOCK_SIZE(1));

static Sys_var_bool Sys_replica_compressed_protocol(
    "replica_compressed_protocol",
    "Use compression in the source/replica protocol.",
    GLOBAL_VAR(opt_replica_compressed_protocol), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_deprecated_alias Sys_slave_compressed_protocol(
    "slave_compressed_protocol", Sys_replica_compressed_protocol);

static const char *replica_exec_mode_names[] = {"STRICT", "IDEMPOTENT",
                                                nullptr};
static Sys_var_enum Sys_replica_exec_mode(
    "replica_exec_mode",
    "Modes for how replication events should be executed. Legal values "
    "are STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, "
    "replication will ignore duplicate key errors and key not found errors. "
    "In STRICT mode, replication will stop at those errors.",
    GLOBAL_VAR(replica_exec_mode_options), CMD_LINE(REQUIRED_ARG),
    replica_exec_mode_names, DEFAULT(RBR_EXEC_MODE_STRICT));

static Sys_var_deprecated_alias Sys_slave_exec_mode("slave_exec_mode",
                                                    Sys_replica_exec_mode);

const char *replica_type_conversions_name[] = {
    "ALL_LOSSY", "ALL_NON_LOSSY", "ALL_UNSIGNED", "ALL_SIGNED", nullptr};
static Sys_var_set Sys_replica_type_conversions(
    "replica_type_conversions",
    "Set of type conversions that may be used by the replication applier "
    "thread for row events. Allowed values are:"
    " ALL_LOSSY to enable lossy conversions,"
    " ALL_NON_LOSSY to enable non-lossy conversions,"
    " ALL_UNSIGNED to treat all integer column type data to be unsigned "
    "values, and"
    " ALL_SIGNED to treat all integer column type data to be signed values."
    " Default treatment is ALL_SIGNED. If ALL_SIGNED and ALL_UNSIGNED both "
    "are"
    " specified, ALL_SIGNED will take higher priority than ALL_UNSIGNED."
    " If the variable is assigned the empty set, no conversions are"
    " allowed and it is expected that the types match exactly.",
    GLOBAL_VAR(replica_type_conversions_options), CMD_LINE(REQUIRED_ARG),
    replica_type_conversions_name, DEFAULT(0));

static Sys_var_deprecated_alias Sys_slave_type_conversions(
    "slave_type_conversions", Sys_replica_type_conversions);

static Sys_var_bool Sys_replica_sql_verify_checksum(
    "replica_sql_verify_checksum",
    "Force checksum verification of replication events after reading them "
    "from relay log. Note: The replica always verifies checksums for events "
    "received from the network, if the event has a checksum at all, before "
    "it writes the event to the relay log. Enabled by default.",
    GLOBAL_VAR(opt_replica_sql_verify_checksum), CMD_LINE(OPT_ARG),
    DEFAULT(true));

static Sys_var_deprecated_alias Sys_slave_sql_verify_checksum(
    "slave_sql_verify_checksum", Sys_replica_sql_verify_checksum);

static bool check_not_null_not_empty(sys_var *self, THD *thd, set_var *var) {
  String str, *res;
  /* null value is not allowed */
  if (check_not_null(self, thd, var)) return true;

  /** empty value ('') is not allowed */
  res = var->value ? var->value->val_str(&str) : nullptr;
  if (res && res->is_empty()) return true;

  return false;
}

static bool check_slave_stopped(sys_var *self, THD *thd, set_var *var) {
  bool result = false;
  Master_info *mi = nullptr;

  if (check_not_null_not_empty(self, thd, var)) return true;

  channel_map.wrlock();

  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;
    if (mi) {
      mysql_mutex_lock(&mi->rli->run_lock);
      if (mi->rli->slave_running) {
        my_error(ER_REPLICA_SQL_THREAD_MUST_STOP, MYF(0));
        result = true;
      }
      mysql_mutex_unlock(&mi->rli->run_lock);
    }
  }
  channel_map.unlock();
  return result;
}

static const char *slave_rows_search_algorithms_names[] = {
    "TABLE_SCAN", "INDEX_SCAN", "HASH_SCAN", nullptr};
static Sys_var_set Slave_rows_search_algorithms(
    "slave_rows_search_algorithms",
    "The set of algorithms used by the replication applier while searching the "
    "table for rows to update or delete. Possible values are: INDEX_SCAN, "
    "TABLE_SCAN and HASH_SCAN. Any combination is allowed, and the applier "
    "picks the most efficient among them for any given scenario. "
    "(Default: INDEX_SCAN, HASH_SCAN).",
    GLOBAL_VAR(slave_rows_search_algorithms_options),
    CMD_LINE(REQUIRED_ARG, OPT_SLAVE_ROWS_SEARCH_ALGORITHMS),
    slave_rows_search_algorithms_names,
    DEFAULT(SLAVE_ROWS_INDEX_SCAN | SLAVE_ROWS_HASH_SCAN), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_not_null_not_empty), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static const char *mts_parallel_type_names[] = {"DATABASE", "LOGICAL_CLOCK",
                                                nullptr};
static Sys_var_enum Sys_replica_parallel_type(
    "replica_parallel_type",
    "The method used by the replication applier to parallelize "
    "transactions. DATABASE, indicates that it "
    "may apply transactions in parallel in case they update different "
    "databases. LOGICAL_CLOCK, which is the default, indicates that it decides "
    "whether two "
    "transactions can be applied in parallel using the logical timestamps "
    "computed by the source, according to "
    "binlog_transaction_dependency_tracking.",
    PERSIST_AS_READONLY GLOBAL_VAR(mts_parallel_option),
    CMD_LINE(REQUIRED_ARG, OPT_REPLICA_PARALLEL_TYPE), mts_parallel_type_names,
    DEFAULT(MTS_PARALLEL_TYPE_LOGICAL_CLOCK), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_slave_stopped), ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_deprecated_alias Sys_slave_parallel_type(
    "slave_parallel_type", Sys_replica_parallel_type);

static bool check_binlog_transaction_dependency_tracking(sys_var *, THD *,
                                                         set_var *var) {
  if (global_system_variables.transaction_write_set_extraction ==
          HASH_ALGORITHM_OFF &&
      var->save_result.ulonglong_value != DEPENDENCY_TRACKING_COMMIT_ORDER) {
    my_error(ER_WRONG_USAGE, MYF(0),
             "binlog_transaction_dependency_tracking (!= COMMIT_ORDER)",
             "transaction_write_set_extraction (= OFF)");

    return true;
  }
  return false;
}

static bool update_binlog_transaction_dependency_tracking(sys_var *, THD *,
                                                          enum_var_type) {
  /*
    the writeset_history_start needs to be set to 0 whenever there is a
    change in the transaction dependency source so that WS and COMMIT
    transition smoothly.
  */
  mysql_bin_log.m_dependency_tracker.tracking_mode_changed();
  return false;
}

static PolyLock_mutex PLock_slave_trans_dep_tracker(
    &LOCK_replica_trans_dep_tracker);
static const char *opt_binlog_transaction_dependency_tracking_names[] = {
    "COMMIT_ORDER", "WRITESET", "WRITESET_SESSION", NullS};
static Sys_var_enum Binlog_transaction_dependency_tracking(
    "binlog_transaction_dependency_tracking",
    "Selects the source of dependency information from which to "
    "compute logical timestamps, which replicas can use to decide which "
    "transactions can be executed in parallel when using "
    "replica_parallel_type=LOGICAL_CLOCK. "
    "Possible values are COMMIT_ORDER, WRITESET and WRITESET_SESSION.",
    GLOBAL_VAR(mysql_bin_log.m_dependency_tracker.m_opt_tracking_mode),
    CMD_LINE(REQUIRED_ARG), opt_binlog_transaction_dependency_tracking_names,
    DEFAULT(DEPENDENCY_TRACKING_COMMIT_ORDER), &PLock_slave_trans_dep_tracker,
    NOT_IN_BINLOG, ON_CHECK(check_binlog_transaction_dependency_tracking),
    ON_UPDATE(update_binlog_transaction_dependency_tracking));
static Sys_var_ulong Binlog_transaction_dependency_history_size(
    "binlog_transaction_dependency_history_size",
    "Maximum number of rows to keep in the writeset history.",
    GLOBAL_VAR(mysql_bin_log.m_dependency_tracker.get_writeset()
                   ->m_opt_max_history_size),
    CMD_LINE(REQUIRED_ARG, 0), VALID_RANGE(1, 1000000), DEFAULT(25000),
    BLOCK_SIZE(1), &PLock_slave_trans_dep_tracker, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr));

static Sys_var_bool Sys_replica_preserve_commit_order(
    "replica_preserve_commit_order",
    "Force replication worker threads to commit in the same order as on the "
    "source. Enabled by default",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_replica_preserve_commit_order),
    CMD_LINE(OPT_ARG, OPT_REPLICA_PRESERVE_COMMIT_ORDER), DEFAULT(true),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_slave_stopped),
    ON_UPDATE(nullptr));

static Sys_var_deprecated_alias Sys_slave_preserve_commit_order(
    "slave_preserve_commit_order", Sys_replica_preserve_commit_order);

bool Sys_var_charptr::global_update(THD *, set_var *var) {
  char *new_val, *ptr = var->save_result.string_value.str;
  size_t len = var->save_result.string_value.length;
  if (ptr) {
    new_val = (char *)my_memdup(key_memory_Sys_var_charptr_value, ptr, len + 1,
                                MYF(MY_WME));
    if (!new_val) return true;
    new_val[len] = 0;
  } else
    new_val = nullptr;
  if (flags & ALLOCATED) my_free(global_var(char *));
  flags |= ALLOCATED;
  global_var(char *) = new_val;
  return false;
}

bool Sys_var_enum_binlog_checksum::global_update(THD *thd, set_var *var) {
  bool check_purge = false;

  /*
    SET binlog_checksome command should ignore 'read-only' and
    'super_read_only' options so that it can update 'mysql.gtid_executed'
    replication repository table.
  */
  thd->set_skip_readonly_check();
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  if (mysql_bin_log.is_open()) {
    bool alg_changed =
        (binlog_checksum_options != (uint)var->save_result.ulonglong_value);
    if (alg_changed)
      mysql_bin_log.checksum_alg_reset =
          (uint8)var->save_result.ulonglong_value;
    mysql_bin_log.rotate(true, &check_purge);
    if (alg_changed)
      mysql_bin_log.checksum_alg_reset =
          binary_log::BINLOG_CHECKSUM_ALG_UNDEF;  // done
  } else {
    binlog_checksum_options =
        static_cast<ulong>(var->save_result.ulonglong_value);
  }
  assert(binlog_checksum_options == var->save_result.ulonglong_value);
  assert(mysql_bin_log.checksum_alg_reset ==
         binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());

  if (check_purge) mysql_bin_log.auto_purge();

  return false;
}

bool Sys_var_gtid_next::session_update(THD *thd, set_var *var) {
  DBUG_TRACE;
  char buf[Gtid::MAX_TEXT_LENGTH + 1];
  // Get the value
  String str(buf, sizeof(buf), &my_charset_latin1);
  char *res = nullptr;
  if (!var->value) {
    // set session gtid_next= default
    assert(var->save_result.string_value.str);
    assert(var->save_result.string_value.length);
    res = var->save_result.string_value.str;
  } else if (var->value->val_str(&str))
    res = var->value->val_str(&str)->c_ptr_safe();
  if (!res) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, "NULL");
    return true;
  }
  global_sid_lock->rdlock();
  Gtid_specification spec;
  if (spec.parse(global_sid_map, res) != RETURN_STATUS_OK) {
    global_sid_lock->unlock();
    return true;
  }

  bool ret = set_gtid_next(thd, spec);
  // set_gtid_next releases global_sid_lock
  return ret;
}

#ifdef HAVE_GTID_NEXT_LIST
bool Sys_var_gtid_set::session_update(THD *thd, set_var *var) {
  DBUG_TRACE;
  Gtid_set_or_null *gsn = (Gtid_set_or_null *)session_var_ptr(thd);
  char *value = var->save_result.string_value.str;
  if (value == NULL)
    gsn->set_null();
  else {
    Gtid_set *gs = gsn->set_non_null(global_sid_map);
    if (gs == NULL) {
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // allocation failed
      return true;
    }
    /*
      If string begins with '+', add to the existing set, otherwise
      replace existing set.
    */
    while (isspace(*value)) value++;
    if (*value == '+')
      value++;
    else
      gs->clear();
    // Add specified set of groups to Gtid_set.
    global_sid_lock->rdlock();
    enum_return_status ret = gs->add_gtid_text(value);
    global_sid_lock->unlock();
    if (ret != RETURN_STATUS_OK) {
      gsn->set_null();
      return true;
    }
  }
  return false;
}
#endif  // HAVE_GTID_NEXT_LIST

/**
  This function shall issue a deprecation warning
  if the new gtid mode is set to GTID_MODE_ON and
  there is at least one replication channel with
  IGNORE_SERVER_IDS configured (i.e., not empty).

  The caller must have acquired a lock on the
  channel_map object before calling this function.

  The warning emitted is: ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT .

  @param thd The current session thread context.
  @param oldmode The old value of @@global.gtid_mode.
  @param newmode The new value for @@global.gtid_mode.

*/
static void issue_deprecation_warnings_gtid_mode(
    THD *thd, Gtid_mode::value_type oldmode [[maybe_unused]],
    Gtid_mode::value_type newmode) {
  channel_map.assert_some_lock();

  /*
    Check that if changing to gtid_mode=on no channel is configured
    to ignore server ids. If it is, issue a deprecation warning.
  */
  if (newmode == Gtid_mode::ON) {
    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      Master_info *mi = it->second;
      if (mi != nullptr && mi->is_ignore_server_ids_configured()) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
            ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
            "CHANGE MASTER TO ... IGNORE_SERVER_IDS='...' "
            "(when @@GLOBAL.GTID_MODE = ON)");

        break;  // Only push one warning
      }
    }
  }
}

/**
  This function shall be called whenever the global scope
  of gtid_mode var is updated.

  It checks some preconditions and also emits deprecation
  warnings conditionally when changing the value.

  Deprecation warnings are emitted after error conditions
  have been checked and only if there is no error raised.
*/
bool Sys_var_gtid_mode::global_update(THD *thd, set_var *var) {
  DBUG_TRACE;
  bool ret = true;

  /*
    SET binlog_checksome command should ignore 'read-only' and
    'super_read_only' options so that it can update 'mysql.gtid_executed'
    replication repository table.
  */
  thd->set_skip_readonly_check();
  /*
    Hold lock_log so that:
    - other transactions are not flushed while gtid_mode is changed;
    - gtid_mode is not changed while some other thread is rotating
    the binlog.

    Hold channel_map lock so that:
    - gtid_mode is not changed during the execution of some
    replication command; particularly CHANGE MASTER. CHANGE MASTER
    checks if GTID_MODE is compatible with AUTO_POSITION, and
    later it actually updates the in-memory structure for
    AUTO_POSITION.  If gtid_mode was changed between these calls,
    auto_position could be set incompatible with gtid_mode.

    Hold global_sid_lock.wrlock so that:
    - other transactions cannot acquire ownership of any gtid.

    Hold Gtid_mode::lock so that all places that don't want to hold
    any of the other locks, but want to read gtid_mode, don't need
    to take the other locks.
  */

  auto new_gtid_mode =
      static_cast<Gtid_mode::value_type>(var->save_result.ulonglong_value);

  if (Gtid_mode::lock.trywrlock()) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), Gtid_mode::to_string(new_gtid_mode),
             "there is a concurrent operation that disallows changes to "
             "@@GLOBAL.GTID_MODE");
    return ret;
  }

  channel_map.wrlock();
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  global_sid_lock->wrlock();
  int lock_count = 4;

  auto old_gtid_mode = global_gtid_mode.get();
  assert(new_gtid_mode <= Gtid_mode::ON);

  DBUG_PRINT("info", ("old_gtid_mode=%d new_gtid_mode=%d", old_gtid_mode,
                      new_gtid_mode));

  if (new_gtid_mode == old_gtid_mode) goto end;

  // Can only change one step at a time.
  /*
   Change gtid_mode value without checking for one step change during
   server startup.
  */
  if (mysqld_server_started &&
      abs((int)new_gtid_mode - (int)old_gtid_mode) > 1) {
    my_error(ER_GTID_MODE_CAN_ONLY_CHANGE_ONE_STEP_AT_A_TIME, MYF(0));
    goto err;
  }

  DBUG_PRINT("info", ("sql_replica_skip_counter=%d", sql_replica_skip_counter));
  if (new_gtid_mode == Gtid_mode::ON && sql_replica_skip_counter > 0) {
    push_warning(
        thd, Sql_condition::SL_WARNING,
        ER_SQL_REPLICA_SKIP_COUNTER_USED_WITH_GTID_MODE_ON,
        ER_THD(thd, ER_SQL_REPLICA_SKIP_COUNTER_USED_WITH_GTID_MODE_ON));
  }

  if (new_gtid_mode != Gtid_mode::ON && replicate_same_server_id &&
      opt_log_replica_updates && opt_bin_log) {
    std::stringstream ss;

    ss << "replicate_same_server_id is set together with log_replica_updates"
       << " and log_bin. Thus, any anonymous transactions"
       << " would circulate infinitely in case this server is part of a"
       << " circular replication topology";

    my_error(ER_CANT_SET_GTID_MODE, MYF(0), Gtid_mode::to_string(new_gtid_mode),
             ss.str().c_str());
    goto err;
  }

  // Cannot set OFF when some channel uses AUTO_POSITION.
  if (new_gtid_mode == Gtid_mode::OFF) {
    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      Master_info *mi = it->second;
      if (mi != nullptr && mi->is_auto_position()) {
        DBUG_PRINT("info", ("auto_position for channel '%s' is %d",
                            mi->get_channel(), mi->is_auto_position()));
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "replication channel '%.192s' is configured "
                 "in AUTO_POSITION mode. Execute "
                 "CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 "
                 "FOR CHANNEL '%.192s' before you set "
                 "@@GLOBAL.GTID_MODE = OFF.",
                 mi->get_channel(), mi->get_channel());
        my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF", buf);
        goto err;
      }
    }
  }

  // Cannot set to GTID_MODE <> ON when some channel uses
  // ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = LOCAL|UUID.
  if (old_gtid_mode == Gtid_mode::ON && new_gtid_mode != Gtid_mode::ON) {
    for (auto it : channel_map) {
      Master_info *mi = it.second;
      if (mi != nullptr &&
          mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() >
              Assign_gtids_to_anonymous_transactions_info::enum_type::
                  AGAT_OFF) {
        DBUG_PRINT(
            "info",
            ("assign_gtids_to_anonymous_transactions for channel '%s' is %d",
             mi->get_channel(),
             static_cast<int>(
                 mi->rli->m_assign_gtids_to_anonymous_transactions_info
                     .get_type())));
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "replication channel '%.192s' is configured "
                 "with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS set to LOCAL or "
                 "to a UUID. "
                 "Execute CHANGE REPLICATION SOURCE TO "
                 "ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = OFF "
                 "FOR CHANNEL '%.192s' before you set "
                 "@@GLOBAL.GTID_MODE = '%s'",
                 mi->get_channel(), mi->get_channel(),
                 Gtid_mode::to_string(new_gtid_mode));
        my_error(ER_CANT_SET_GTID_MODE, MYF(0),
                 Gtid_mode::to_string(new_gtid_mode), buf);
        goto err;
      }
    }
  }
  /*
    Cannot set OFF when source_connection_auto_failover is enabled for any
    channel.
  */
  if (new_gtid_mode != Gtid_mode::ON) {
    for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
         it++) {
      Master_info *mi = it->second;
      if (mi != nullptr && mi->is_source_connection_auto_failover()) {
        my_error(ER_DISABLE_GTID_MODE_REQUIRES_ASYNC_RECONNECT_OFF, MYF(0),
                 Gtid_mode::to_string(new_gtid_mode));
        goto err;
      }
    }
  }
  /*
    Cannot set to <> ON when gtid_only is enabled for any channel.
  */
  if (old_gtid_mode == Gtid_mode::ON && new_gtid_mode != Gtid_mode::ON) {
    for (auto it : channel_map) {
      Master_info *mi = it.second;
      if (mi != nullptr && mi->is_gtid_only_mode()) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "replication channel '%.192s' is configured "
                 "with GTID_ONLY = 1. "
                 "Execute CHANGE REPLICATION SOURCE TO "
                 "GTID_ONLY = 0 "
                 "FOR CHANNEL '%.192s' before you set "
                 "@@GLOBAL.GTID_MODE = '%s'",
                 mi->get_channel(), mi->get_channel(),
                 Gtid_mode::to_string(new_gtid_mode));
        my_error(ER_CANT_SET_GTID_MODE, MYF(0),
                 Gtid_mode::to_string(new_gtid_mode), buf);
        goto err;
      }
    }
  }

  // Can't set GTID_MODE != ON when group replication is enabled.
  if (is_group_replication_running()) {
    assert(old_gtid_mode == Gtid_mode::ON);
    assert(new_gtid_mode == Gtid_mode::ON_PERMISSIVE);
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), Gtid_mode::to_string(new_gtid_mode),
             "group replication requires @@GLOBAL.GTID_MODE=ON");
    goto err;
  }

  // Compatible with ongoing transactions.
  DBUG_PRINT("info", ("anonymous_ownership_count=%d owned_gtids->is_empty=%d",
                      gtid_state->get_anonymous_ownership_count(),
                      gtid_state->get_owned_gtids()->is_empty()));
  gtid_state->get_owned_gtids()->dbug_print("global owned_gtids");
  if (new_gtid_mode == Gtid_mode::ON &&
      gtid_state->get_anonymous_ownership_count() > 0) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON",
             "there are ongoing, anonymous transactions. Before "
             "setting @@GLOBAL.GTID_MODE = ON, wait until "
             "SHOW STATUS LIKE 'ANONYMOUS_TRANSACTION_COUNT' "
             "shows zero on all servers. Then wait for all "
             "existing, anonymous transactions to replicate to "
             "all replicas, and then execute "
             "SET @@GLOBAL.GTID_MODE = ON on all servers. "
             "See the Manual for details");
    goto err;
  }

  if (new_gtid_mode == Gtid_mode::OFF &&
      !gtid_state->get_owned_gtids()->is_empty()) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF",
             "there are ongoing transactions that have a GTID. "
             "Before you set @@GLOBAL.GTID_MODE = OFF, wait "
             "until SELECT @@GLOBAL.GTID_OWNED is empty on all "
             "servers. Then wait for all GTID-transactions to "
             "replicate to all servers, and then execute "
             "SET @@GLOBAL.GTID_MODE = OFF on all servers. "
             "See the Manual for details");
    goto err;
  }

  // Compatible with ongoing GTID-violating transactions
  DBUG_PRINT("info",
             ("automatic_gtid_violating_transaction_count=%d",
              gtid_state->get_automatic_gtid_violating_transaction_count()));
  if (new_gtid_mode >= Gtid_mode::ON_PERMISSIVE &&
      gtid_state->get_automatic_gtid_violating_transaction_count() > 0) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON_PERMISSIVE",
             "there are ongoing transactions that use "
             "GTID_NEXT = 'AUTOMATIC', which violate GTID "
             "consistency. Adjust your workload to be "
             "GTID-consistent before setting "
             "@@GLOBAL.GTID_MODE = ON_PERMISSIVE. "
             "See the Manual for "
             "@@GLOBAL.ENFORCE_GTID_CONSISTENCY for details");
    goto err;
  }

  // Compatible with ENFORCE_GTID_CONSISTENCY.
  if (new_gtid_mode == Gtid_mode::ON &&
      get_gtid_consistency_mode() != GTID_CONSISTENCY_MODE_ON) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON",
             "ENFORCE_GTID_CONSISTENCY is not ON");
    goto err;
  }

  // Can't set GTID_MODE=OFF with ongoing calls to
  // WAIT_FOR_EXECUTED_GTID_SET or
  // WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS.
  DBUG_PRINT("info",
             ("gtid_wait_count=%d", gtid_state->get_gtid_wait_count() > 0));
  if (new_gtid_mode == Gtid_mode::OFF &&
      gtid_state->get_gtid_wait_count() > 0) {
    my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF",
             "there are ongoing calls to "
             "WAIT_FOR_EXECUTED_GTID_SET or "
             "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS. Before you set "
             "@@GLOBAL.GTID_MODE = OFF, ensure that no other "
             "client is waiting for GTID-transactions to be "
             "committed");
    goto err;
  }

  // Update the mode
  global_var(ulong) = new_gtid_mode;
  global_gtid_mode.set(new_gtid_mode);
  global_sid_lock->unlock();
  lock_count = 3;

  // Generate note in log
  LogErr(SYSTEM_LEVEL, ER_CHANGED_GTID_MODE,
         Gtid_mode::to_string(old_gtid_mode),
         Gtid_mode::to_string(new_gtid_mode));

  // Rotate
  {
    bool dont_care = false;
    if (mysql_bin_log.rotate(true, &dont_care)) goto err;
  }

end:
  /* handle deprecations warning */
  issue_deprecation_warnings_gtid_mode(thd, old_gtid_mode, new_gtid_mode);

  ret = false;
err:
  assert(lock_count >= 0);
  assert(lock_count <= 4);
  if (lock_count == 4) global_sid_lock->unlock();
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());
  channel_map.unlock();
  Gtid_mode::lock.unlock();
  return ret;
}

bool Sys_var_enforce_gtid_consistency::global_update(THD *thd, set_var *var) {
  DBUG_TRACE;
  bool ret = true;

  /*
    Hold global_sid_lock.wrlock so that other transactions cannot
    acquire ownership of any gtid.
  */
  global_sid_lock->wrlock();

  DBUG_PRINT("info", ("var->save_result.ulonglong_value=%llu",
                      var->save_result.ulonglong_value));
  enum_gtid_consistency_mode new_mode =
      (enum_gtid_consistency_mode)var->save_result.ulonglong_value;
  enum_gtid_consistency_mode old_mode = get_gtid_consistency_mode();
  auto gtid_mode = global_gtid_mode.get();

  assert(new_mode <= GTID_CONSISTENCY_MODE_WARN);

  DBUG_PRINT("info", ("old enforce_gtid_consistency=%d "
                      "new enforce_gtid_consistency=%d "
                      "gtid_mode=%d ",
                      old_mode, new_mode, gtid_mode));

  if (new_mode == old_mode) goto end;

  // Can't turn off GTID-consistency when GTID_MODE=ON.
  if (new_mode != GTID_CONSISTENCY_MODE_ON && gtid_mode == Gtid_mode::ON) {
    my_error(ER_GTID_MODE_ON_REQUIRES_ENFORCE_GTID_CONSISTENCY_ON, MYF(0));
    goto err;
  }
  // If there are ongoing GTID-violating transactions, and we are
  // moving from OFF->ON, WARN->ON, or OFF->WARN, generate warning
  // or error accordingly.
  if (new_mode == GTID_CONSISTENCY_MODE_ON ||
      (old_mode == GTID_CONSISTENCY_MODE_OFF &&
       new_mode == GTID_CONSISTENCY_MODE_WARN)) {
    DBUG_PRINT("info",
               ("automatic_gtid_violating_transaction_count=%d "
                "anonymous_gtid_violating_transaction_count=%d",
                gtid_state->get_automatic_gtid_violating_transaction_count(),
                gtid_state->get_anonymous_gtid_violating_transaction_count()));
    if (gtid_state->get_automatic_gtid_violating_transaction_count() > 0 ||
        gtid_state->get_anonymous_gtid_violating_transaction_count() > 0) {
      if (new_mode == GTID_CONSISTENCY_MODE_ON) {
        my_error(
            ER_CANT_ENFORCE_GTID_CONSISTENCY_WITH_ONGOING_GTID_VIOLATING_TX,
            MYF(0));
        goto err;
      } else {
        push_warning(
            thd, Sql_condition::SL_WARNING,
            ER_ENFORCE_GTID_CONSISTENCY_WARN_WITH_ONGOING_GTID_VIOLATING_TX,
            ER_THD(
                thd,
                ER_ENFORCE_GTID_CONSISTENCY_WARN_WITH_ONGOING_GTID_VIOLATING_TX));
      }
    }
  }

  // Update the mode
  global_var(ulong) = new_mode;

  // Generate note in log
  LogErr(INFORMATION_LEVEL, ER_CHANGED_ENFORCE_GTID_CONSISTENCY,
         get_gtid_consistency_mode_string(old_mode),
         get_gtid_consistency_mode_string(new_mode));

end:
  ret = false;
err:
  global_sid_lock->unlock();
  return ret;
}

static Sys_var_enum_binlog_checksum Binlog_checksum_enum(
    "binlog_checksum",
    "Type of BINLOG_CHECKSUM_ALG. Include checksum for "
    "log events in the binary log. Possible values are NONE and CRC32; "
    "default is CRC32.",
    GLOBAL_VAR(binlog_checksum_options), CMD_LINE(REQUIRED_ARG),
    binlog_checksum_type_names, DEFAULT(binary_log::BINLOG_CHECKSUM_ALG_CRC32),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_outside_trx));

static Sys_var_bool Sys_source_verify_checksum(
    "source_verify_checksum",
    "Force checksum verification of events in binary log before "
    "sending them to replicas or printing them in output of SHOW BINLOG "
    "EVENTS. "
    "Disabled by default.",
    GLOBAL_VAR(opt_source_verify_checksum), CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_deprecated_alias Sys_master_verify_checksum(
    "master_verify_checksum", Sys_source_verify_checksum);

static Sys_var_ulong Sys_slow_launch_time(
    "slow_launch_time",
    "If creating the thread takes longer than this value (in seconds), "
    "the Slow_launch_threads counter will be incremented",
    GLOBAL_VAR(slow_launch_time), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, LONG_TIMEOUT), DEFAULT(2), BLOCK_SIZE(1));

static Sys_var_ulong Sys_sort_buffer(
    "sort_buffer_size",
    "Each thread that needs to do a sort allocates a buffer of this size",
    HINT_UPDATEABLE SESSION_VAR(sortbuff_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(MIN_SORT_MEMORY, ULONG_MAX), DEFAULT(DEFAULT_SORT_MEMORY),
    BLOCK_SIZE(1));

/**
  Check sql modes strict_mode, 'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and
  'ERROR_FOR_DIVISION_BY_ZERO' are used together. If only subset of it
  is set then warning is reported.

  @param sql_mode sql mode.
  @param thd      Current thread
*/
static void check_sub_modes_of_strict_mode(sql_mode_t &sql_mode, THD *thd) {
  const sql_mode_t strict_modes =
      (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES);

  const sql_mode_t new_strict_submodes =
      (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE |
       MODE_ERROR_FOR_DIVISION_BY_ZERO);

  const sql_mode_t strict_modes_set = (sql_mode & strict_modes);
  const sql_mode_t new_strict_submodes_set = (sql_mode & new_strict_submodes);

  if (((strict_modes_set | new_strict_submodes_set) != 0) &&
      ((new_strict_submodes_set != new_strict_submodes) ||
       (strict_modes_set == 0))) {
    if (thd)
      push_warning(thd, Sql_condition::SL_WARNING, ER_SQL_MODE_MERGED,
                   ER_THD(thd, ER_SQL_MODE_MERGED));
    else
      LogErr(WARNING_LEVEL, ER_SQL_MODE_MERGED_WITH_STRICT_MODE);
  }
}

export sql_mode_t expand_sql_mode(sql_mode_t sql_mode, THD *thd) {
  if (sql_mode & MODE_ANSI) {
    /*
      Note that we dont set
      MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS | MODE_NO_FIELD_OPTIONS
      to allow one to get full use of MySQL in this mode.
    */
    sql_mode |= (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
                 MODE_IGNORE_SPACE | MODE_ONLY_FULL_GROUP_BY);
  }
  if (sql_mode & MODE_TRADITIONAL)
    sql_mode |= (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES |
                 MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE |
                 MODE_ERROR_FOR_DIVISION_BY_ZERO | MODE_NO_ENGINE_SUBSTITUTION);

  check_sub_modes_of_strict_mode(sql_mode, thd);
  return sql_mode;
}
static bool check_sql_mode(sys_var *, THD *thd, set_var *var) {
  sql_mode_t candidate_mode =
      expand_sql_mode(var->save_result.ulonglong_value, thd);

  if (candidate_mode & ~(MODE_ALLOWED_MASK | MODE_IGNORED_MASK)) {
    my_error(ER_UNSUPPORTED_SQL_MODE, MYF(0),
             candidate_mode & ~(MODE_ALLOWED_MASK | MODE_IGNORED_MASK));
    return true;  // mode seems never supported before
  }

  if (candidate_mode & ~MODE_ALLOWED_MASK) {
    if (thd->variables.pseudo_replica_mode &&  // (1)
        thd->lex->sphead == nullptr) {         // (2)
      /*
        (1): catch the auto-generated SET SQL_MODE calls in the output of
             mysqlbinlog,
        (2): but ignore the other ones (e.g. nested SET SQL_MODE calls in
             SBR-invoked trigger calls).
      */
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_WARN_REMOVED_SQL_MODE,
          ER_THD(thd, ER_WARN_REMOVED_SQL_MODE),
          static_cast<uint>(candidate_mode & ~MODE_ALLOWED_MASK));
      // ignore obsolete mode flags in case this is an old mysqlbinlog:
      candidate_mode &= MODE_ALLOWED_MASK;
    } else {
      my_error(ER_UNSUPPORTED_SQL_MODE, MYF(0),
               candidate_mode & ~MODE_ALLOWED_MASK);
      return true;  // error on obsolete mode flags
    }
  }

  if (candidate_mode & MODE_PAD_CHAR_TO_FULL_LENGTH) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SQLMODE,
        ER_THD(thd, ER_WARN_DEPRECATED_SQLMODE), "PAD_CHAR_TO_FULL_LENGTH");
  }

  var->save_result.ulonglong_value = candidate_mode;
  return false;
}
static bool fix_sql_mode(sys_var *self, THD *thd, enum_var_type type) {
  if (!self->is_global_persist(type)) {
    /* Update thd->server_status */
    if (thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
      thd->server_status |= SERVER_STATUS_NO_BACKSLASH_ESCAPES;
    else
      thd->server_status &= ~SERVER_STATUS_NO_BACKSLASH_ESCAPES;
  }
  return false;
}
/*
  WARNING: When adding new SQL modes don't forget to update the
  tables definitions that stores it's value (ie: mysql.event, mysql.routines,
  mysql.triggers)
*/
static const char *sql_mode_names[] = {"REAL_AS_FLOAT",
                                       "PIPES_AS_CONCAT",
                                       "ANSI_QUOTES",
                                       "IGNORE_SPACE",
                                       "NOT_USED",
                                       "ONLY_FULL_GROUP_BY",
                                       "NO_UNSIGNED_SUBTRACTION",
                                       "NO_DIR_IN_CREATE",
                                       "NOT_USED_9",
                                       "NOT_USED_10",
                                       "NOT_USED_11",
                                       "NOT_USED_12",
                                       "NOT_USED_13",
                                       "NOT_USED_14",
                                       "NOT_USED_15",
                                       "NOT_USED_16",
                                       "NOT_USED_17",
                                       "NOT_USED_18",
                                       "ANSI",
                                       "NO_AUTO_VALUE_ON_ZERO",
                                       "NO_BACKSLASH_ESCAPES",
                                       "STRICT_TRANS_TABLES",
                                       "STRICT_ALL_TABLES",
                                       "NO_ZERO_IN_DATE",
                                       "NO_ZERO_DATE",
                                       "ALLOW_INVALID_DATES",
                                       "ERROR_FOR_DIVISION_BY_ZERO",
                                       "TRADITIONAL",
                                       "NOT_USED_29",
                                       "HIGH_NOT_PRECEDENCE",
                                       "NO_ENGINE_SUBSTITUTION",
                                       "PAD_CHAR_TO_FULL_LENGTH",
                                       "TIME_TRUNCATE_FRACTIONAL",
                                       nullptr};
export bool sql_mode_string_representation(THD *thd, sql_mode_t sql_mode,
                                           LEX_STRING *ls) {
  set_to_string(thd, ls, sql_mode, sql_mode_names);
  return ls->str == nullptr;
}
export bool sql_mode_quoted_string_representation(THD *thd, sql_mode_t sql_mode,
                                                  LEX_STRING *ls) {
  set_to_string(thd, ls, sql_mode, sql_mode_names, true);
  return ls->str == nullptr;
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
    HINT_UPDATEABLE SESSION_VAR(sql_mode), CMD_LINE(REQUIRED_ARG),
    sql_mode_names,
    DEFAULT(MODE_NO_ENGINE_SUBSTITUTION | MODE_ONLY_FULL_GROUP_BY |
            MODE_STRICT_TRANS_TABLES | MODE_NO_ZERO_IN_DATE |
            MODE_NO_ZERO_DATE | MODE_ERROR_FOR_DIVISION_BY_ZERO),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_sql_mode),
    ON_UPDATE(fix_sql_mode));

static Sys_var_ulong Sys_max_execution_time(
    "max_execution_time",
    "Kill SELECT statement that takes over the specified number of "
    "milliseconds",
    HINT_UPDATEABLE SESSION_VAR(max_execution_time), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULONG_MAX), DEFAULT(0), BLOCK_SIZE(1));

static const char *ssl_fips_mode_names[] = {"OFF", "ON", "STRICT", nullptr};
static Sys_var_enum Sys_ssl_fips_mode(
    "ssl_fips_mode",
    "SSL FIPS mode (applies only for OpenSSL); "
    "permitted values are: OFF, ON, STRICT",
    READ_ONLY GLOBAL_VAR(opt_ssl_fips_mode),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_FIPS_MODE), ssl_fips_mode_names, DEFAULT(0),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""), sys_var::PARSE_EARLY);

static Sys_var_bool Sys_auto_generate_certs(
    "auto_generate_certs",
    "Auto generate SSL certificates at server startup if --ssl is set to "
    "ON and none of the other SSL system variables are specified and "
    "certificate/key files are not present in data directory.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_auto_generate_certs),
    CMD_LINE(OPT_ARG), DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr);

// why ENUM and not BOOL ?
static const char *updatable_views_with_limit_names[] = {"NO", "YES", nullptr};
static Sys_var_enum Sys_updatable_views_with_limit(
    "updatable_views_with_limit",
    "YES = Don't issue an error message (warning only) if a VIEW without "
    "presence of a key of the underlying table is used in queries with a "
    "LIMIT clause for updating. NO = Prohibit update of a VIEW, which "
    "does not contain a key of the underlying table and the query uses "
    "a LIMIT clause (usually get from GUI tools)",
    HINT_UPDATEABLE SESSION_VAR(updatable_views_with_limit),
    CMD_LINE(REQUIRED_ARG), updatable_views_with_limit_names, DEFAULT(true));

static Sys_var_system_time_zone Sys_system_time_zone(
    "system_time_zone", "The server system time zone");

static Sys_var_ulong Sys_table_def_size(
    "table_definition_cache", "The number of cached table definitions",
    GLOBAL_VAR(table_def_size),
    CMD_LINE(REQUIRED_ARG, OPT_TABLE_DEFINITION_CACHE),
    VALID_RANGE(TABLE_DEF_CACHE_MIN, 512 * 1024),
    DEFAULT(TABLE_DEF_CACHE_DEFAULT), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr,
    /* table_definition_cache is used as a sizing hint by the performance
       schema. */
    sys_var::PARSE_EARLY);

static Sys_var_ulong Sys_schema_def_size(
    "schema_definition_cache", "The number of cached schema definitions",
    GLOBAL_VAR(schema_def_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(SCHEMA_DEF_CACHE_MIN, 512 * 1024),
    DEFAULT(SCHEMA_DEF_CACHE_DEFAULT), BLOCK_SIZE(1));

static Sys_var_ulong Sys_tablespace_def_size(
    "tablespace_definition_cache",
    "The number of cached tablespace definitions",
    GLOBAL_VAR(tablespace_def_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(TABLESPACE_DEF_CACHE_MIN, 512 * 1024),
    DEFAULT(TABLESPACE_DEF_CACHE_DEFAULT), BLOCK_SIZE(1));

static Sys_var_ulong Sys_stored_program_def_size(
    "stored_program_definition_cache",
    "The number of cached stored program definitions",
    GLOBAL_VAR(stored_program_def_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(STORED_PROGRAM_DEF_CACHE_MIN, 512 * 1024),
    DEFAULT(STORED_PROGRAM_DEF_CACHE_DEFAULT), BLOCK_SIZE(1));

static bool fix_table_cache_size(sys_var *, THD *, enum_var_type) {
  /*
    table_open_cache parameter is a soft limit for total number of objects
    in all table cache instances. Once this value is updated we need to
    update value of a per-instance soft limit on table cache size.
  */
  table_cache_size_per_instance = table_cache_size / table_cache_instances;
  return false;
}

static Sys_var_ulong Sys_table_cache_size(
    "table_open_cache",
    "The number of cached open tables "
    "(total for all table cache instances)",
    GLOBAL_VAR(table_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, 512 * 1024), DEFAULT(TABLE_OPEN_CACHE_DEFAULT),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_table_cache_size), nullptr,
    /* table_open_cache is used as a sizing hint by the performance schema. */
    sys_var::PARSE_EARLY);

static Sys_var_ulong Sys_table_cache_instances(
    "table_open_cache_instances", "The number of table cache instances",
    READ_ONLY GLOBAL_VAR(table_cache_instances), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, Table_cache_manager::MAX_TABLE_CACHES),
    DEFAULT(Table_cache_manager::DEFAULT_MAX_TABLE_CACHES), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    nullptr,
    /*
      table_open_cache is used as a sizing hint by the performance schema,
      and 'table_open_cache' is a prefix of 'table_open_cache_instances'.
      Is is better to keep these options together, to avoid confusing
      handle_options() with partial name matches.
    */
    sys_var::PARSE_EARLY);

/**
  Modify the thread size cache size.
*/

static inline bool modify_thread_cache_size(sys_var *, THD *, enum_var_type) {
  if (Connection_handler_manager::thread_handling ==
      Connection_handler_manager::SCHEDULER_ONE_THREAD_PER_CONNECTION) {
    Per_thread_connection_handler::modify_thread_cache_size(
        Per_thread_connection_handler::max_blocked_pthreads);
  }
  return false;
}

static Sys_var_ulong Sys_thread_cache_size(
    "thread_cache_size", "How many threads we should keep in a cache for reuse",
    GLOBAL_VAR(Per_thread_connection_handler::max_blocked_pthreads),
    CMD_LINE(REQUIRED_ARG, OPT_THREAD_CACHE_SIZE), VALID_RANGE(0, 16384),
    DEFAULT(0), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, nullptr,
    ON_UPDATE(modify_thread_cache_size));

/**
  Function to check if the 'next' transaction isolation level
  can be changed.

  @param[in] thd    Thread handler.
  @param[in] var    A pointer to set_var holding the specified list of
                    system variable names.

  @retval   false   Success.
  @retval   true    Error.
*/
static bool check_transaction_isolation(sys_var *, THD *thd, set_var *var) {
  if (var->type == OPT_DEFAULT &&
      (thd->in_active_multi_stmt_transaction() || thd->in_sub_stmt)) {
    assert(thd->in_multi_stmt_transaction_mode() || thd->in_sub_stmt);
    my_error(ER_CANT_CHANGE_TX_CHARACTERISTICS, MYF(0));
    return true;
  }
  return false;
}

/**
  This function sets the session variable thd->variables.transaction_isolation
  to reflect changes to @@session.transaction_isolation.

  @param[in] thd    Thread handler.
  @param[in] var    A pointer to the set_var.

  @retval   false   Success.
  @retval   true    Error.
*/
bool Sys_var_transaction_isolation::session_update(THD *thd, set_var *var) {
  if (var->type == OPT_SESSION && Sys_var_enum::session_update(thd, var))
    return true;
  if (var->type == OPT_DEFAULT ||
      !(thd->in_active_multi_stmt_transaction() || thd->in_sub_stmt)) {
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
    tx_isol = (enum_tx_isolation)var->save_result.ulonglong_value;
    bool one_shot = (var->type == OPT_DEFAULT);
    return set_tx_isolation(thd, tx_isol, one_shot);
  }
  return false;
}

// NO_CMD_LINE
static Sys_var_transaction_isolation Sys_transaction_isolation(
    "transaction_isolation", "Default transaction isolation level",
    UNTRACKED_DEFAULT SESSION_VAR(transaction_isolation), NO_CMD_LINE,
    tx_isolation_names, DEFAULT(ISO_REPEATABLE_READ), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_transaction_isolation));

/**
  Function to check if the state of 'transaction_read_only' can be changed.
  The state cannot be changed if there is already a transaction in progress.

  @param[in] thd    Thread handler
  @param[in] var    A pointer to set_var holding the specified list of
                    system variable names.

  @retval   false   Success.
  @retval   true    Error.
*/
static bool check_transaction_read_only(sys_var *, THD *thd, set_var *var) {
  if (var->type == OPT_DEFAULT &&
      (thd->in_active_multi_stmt_transaction() || thd->in_sub_stmt)) {
    assert(thd->in_multi_stmt_transaction_mode() || thd->in_sub_stmt);
    my_error(ER_CANT_CHANGE_TX_CHARACTERISTICS, MYF(0));
    return true;
  }
  return false;
}

/**
  This function sets the session variable thd->variables.transaction_read_only
  to reflect changes to @@session.transaction_read_only.

  @param[in] thd    Thread handler.
  @param[in] var    A pointer to the set_var.

  @retval   false   Success.
*/
bool Sys_var_transaction_read_only::session_update(THD *thd, set_var *var) {
  if (var->type == OPT_SESSION && Sys_var_bool::session_update(thd, var))
    return true;
  if (var->type == OPT_DEFAULT ||
      !(thd->in_active_multi_stmt_transaction() || thd->in_sub_stmt)) {
    // @see Sys_var_transaction_isolation::session_update() above for the
    // rules.
    thd->tx_read_only = var->save_result.ulonglong_value;

    if (thd->variables.session_track_transaction_info > TX_TRACK_NONE) {
      TX_TRACKER_GET(tst);

      if (var->type == OPT_DEFAULT)
        tst->set_read_flags(thd,
                            thd->tx_read_only ? TX_READ_ONLY : TX_READ_WRITE);
      else
        tst->set_read_flags(thd, TX_READ_INHERIT);
    }
  }
  return false;
}

static Sys_var_transaction_read_only Sys_transaction_read_only(
    "transaction_read_only",
    "Set default transaction access mode to read only.",
    UNTRACKED_DEFAULT SESSION_VAR(transaction_read_only), NO_CMD_LINE,
    DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_transaction_read_only));

static Sys_var_ulonglong Sys_tmp_table_size(
    "tmp_table_size",
    "If an internal in-memory temporary table in the MEMORY or TempTable "
    "storage engine exceeds this size, MySQL will automatically convert it "
    "to an on-disk table ",
    HINT_UPDATEABLE SESSION_VAR(tmp_table_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, std::numeric_limits<ulonglong>::max()),
    DEFAULT(16 * 1024 * 1024), BLOCK_SIZE(1));

static char *server_version_ptr;
static Sys_var_version Sys_version(
    "version", "Server version",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_version_ptr), NO_CMD_LINE,
    IN_SYSTEM_CHARSET, DEFAULT(server_version));

static char *server_version_comment_ptr;
static Sys_var_charptr Sys_version_comment(
    "version_comment", "version_comment",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_version_comment_ptr), NO_CMD_LINE,
    IN_SYSTEM_CHARSET, DEFAULT(MYSQL_COMPILATION_COMMENT_SERVER));

static char *server_version_compile_machine_ptr;
static Sys_var_charptr Sys_version_compile_machine(
    "version_compile_machine", "version_compile_machine",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_version_compile_machine_ptr),
    NO_CMD_LINE, IN_SYSTEM_CHARSET, DEFAULT(MACHINE_TYPE));

static char *server_version_compile_os_ptr;
static Sys_var_charptr Sys_version_compile_os(
    "version_compile_os", "version_compile_os",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_version_compile_os_ptr),
    NO_CMD_LINE, IN_SYSTEM_CHARSET, DEFAULT(SYSTEM_TYPE));

static const char *server_version_compile_zlib_ptr = ZLIB_VERSION;
static Sys_var_charptr Sys_version_compile_zlib(
    "version_compile_zlib", "version_compile_zlib",
    READ_ONLY NON_PERSIST GLOBAL_VAR(server_version_compile_zlib_ptr),
    NO_CMD_LINE, IN_SYSTEM_CHARSET, DEFAULT(ZLIB_VERSION));

static Sys_var_ulong Sys_net_wait_timeout(
    "wait_timeout",
    "The number of seconds the server waits for activity on a "
    "connection before closing it",
    SESSION_VAR(net_wait_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, IF_WIN(INT_MAX32 / 1000, LONG_TIMEOUT)),
    DEFAULT(NET_WAIT_TIMEOUT), BLOCK_SIZE(1));

static Sys_var_plugin Sys_default_storage_engine(
    "default_storage_engine", "The default storage engine for new tables",
    SESSION_VAR(table_plugin), NO_CMD_LINE, MYSQL_STORAGE_ENGINE_PLUGIN,
    DEFAULT(&default_storage_engine), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_storage_engine));

const char *internal_tmp_mem_storage_engine_names[] = {"MEMORY", "TempTable",
                                                       nullptr};
static Sys_var_enum Sys_internal_tmp_mem_storage_engine(
    "internal_tmp_mem_storage_engine",
    "The default storage engine for in-memory internal temporary tables.",
    HINT_UPDATEABLE SESSION_VAR(internal_tmp_mem_storage_engine),
    CMD_LINE(REQUIRED_ARG), internal_tmp_mem_storage_engine_names,
    DEFAULT(TMP_TABLE_TEMPTABLE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin_no_super));

static Sys_var_ulonglong Sys_temptable_max_ram(
    "temptable_max_ram",
    "Maximum amount of memory (in bytes) the TempTable storage engine is "
    "allowed to allocate from the main memory (RAM) before starting to "
    "store data on disk.",
    GLOBAL_VAR(temptable_max_ram), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(2 << 20 /* 2 MiB */, ULLONG_MAX), DEFAULT(1 << 30 /* 1 GiB */),
    BLOCK_SIZE(1));

static Sys_var_ulonglong Sys_temptable_max_mmap(
    "temptable_max_mmap",
    "Maximum amount of memory (in bytes) the TempTable storage engine is "
    "allowed to allocate from MMAP-backed files before starting to "
    "store data on disk.",
    GLOBAL_VAR(temptable_max_mmap), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULLONG_MAX), DEFAULT(1 << 30 /* 1 GiB */), BLOCK_SIZE(1));

static Sys_var_bool Sys_temptable_use_mmap(
    "temptable_use_mmap",
    "Use mmap files for temptables. "
    "This variable is deprecated and will be removed in a future release.",
    GLOBAL_VAR(temptable_use_mmap), CMD_LINE(OPT_ARG), DEFAULT(true),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_deprecated_with_removal_message), nullptr,
    sys_var::PARSE_NORMAL);

static Sys_var_plugin Sys_default_tmp_storage_engine(
    "default_tmp_storage_engine",
    "The default storage engine for new explicit temporary tables",
    HINT_UPDATEABLE SESSION_VAR(temp_table_plugin), NO_CMD_LINE,
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
static Sys_var_debug_sync Sys_debug_sync("debug_sync", "Debug Sync Facility",
                                         sys_var::ONLY_SESSION, NO_CMD_LINE,
                                         DEFAULT(nullptr), NO_MUTEX_GUARD,
                                         NOT_IN_BINLOG,
                                         ON_CHECK(check_session_admin));
#endif /* defined(ENABLED_DEBUG_SYNC) */

/**
  Pre-update function to commit connection's active transactions when autocommit
  is enabled.

  @note This hook relies on the fact that it is called while not holding any
        locks. Breaking this assumption might result in deadlocks as commit
        acquires many different locks in its process (e.g. to open GTID-related
        tables).

  @param[in] self   A pointer to the sys_var, i.e. Sys_autocommit.
  @param[in] thd    A reference to THD object.
  @param[in] var    A pointer to the set_var created by the parser.

  @retval true   Error during commit
  @retval false  Otherwise
*/
static bool pre_autocommit(sys_var *self, THD *thd, set_var *var) {
  if (!(self->is_global_persist(var->type)) &&
      (thd->variables.option_bits & OPTION_NOT_AUTOCOMMIT) &&
      var->save_result.ulonglong_value) {
    // Autocommit mode is about to be activated.
    if (trans_commit_stmt(thd) || trans_commit(thd)) return true;
  }
  return false;
}

static bool fix_autocommit(sys_var *self, THD *thd, enum_var_type type) {
  if (self->is_global_persist(type)) {
    if (global_system_variables.option_bits & OPTION_AUTOCOMMIT)
      global_system_variables.option_bits &= ~OPTION_NOT_AUTOCOMMIT;
    else
      global_system_variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
    return false;
  }

  if (thd->variables.option_bits & OPTION_AUTOCOMMIT &&
      thd->variables.option_bits &
          OPTION_NOT_AUTOCOMMIT) {  // activating autocommit
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
    thd->variables.option_bits &= ~(OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
    thd->get_transaction()->reset_unsafe_rollback_flags(
        Transaction_ctx::SESSION);
    thd->server_status |= SERVER_STATUS_AUTOCOMMIT;
    return false;
  }

  if (!(thd->variables.option_bits & OPTION_AUTOCOMMIT) &&
      !(thd->variables.option_bits &
        OPTION_NOT_AUTOCOMMIT)) {  // disabling autocommit

    thd->get_transaction()->reset_unsafe_rollback_flags(
        Transaction_ctx::SESSION);
    thd->server_status &= ~SERVER_STATUS_AUTOCOMMIT;
    thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
    return false;
  }

  return false;  // autocommit value wasn't changed
}
static Sys_var_bit Sys_autocommit("autocommit", "autocommit",
                                  SESSION_VAR(option_bits), NO_CMD_LINE,
                                  OPTION_AUTOCOMMIT, DEFAULT(true),
                                  NO_MUTEX_GUARD, NOT_IN_BINLOG,
                                  ON_CHECK(nullptr), PRE_UPDATE(pre_autocommit),
                                  ON_UPDATE(fix_autocommit));
export sys_var *Sys_autocommit_ptr = &Sys_autocommit;  // for sql_yacc.yy

static Sys_var_bool Sys_big_tables(
    "big_tables",
    "Allow big result sets by saving all "
    "temporary sets on file (Solves most 'table full' errors)",
    HINT_UPDATEABLE SESSION_VAR(big_tables), CMD_LINE(OPT_ARG), DEFAULT(false));

static Sys_var_bit Sys_big_selects("sql_big_selects", "sql_big_selects",
                                   HINT_UPDATEABLE SESSION_VAR(option_bits),
                                   NO_CMD_LINE, OPTION_BIG_SELECTS,
                                   DEFAULT(false));

static Sys_var_bit Sys_log_off("sql_log_off", "sql_log_off",
                               SESSION_VAR(option_bits), NO_CMD_LINE,
                               OPTION_LOG_OFF, DEFAULT(false), NO_MUTEX_GUARD,
                               NOT_IN_BINLOG, ON_CHECK(check_session_admin));

/**
  This function sets the session variable thd->variables.sql_log_bin
  to reflect changes to @@session.sql_log_bin.

  @param     thd    Current thread
  @param[in] type   The type either session or global.

  @return @c false.
*/
static bool fix_sql_log_bin_after_update(sys_var *, THD *thd,
                                         enum_var_type type [[maybe_unused]]) {
  assert(type == OPT_SESSION);

  if (thd->variables.sql_log_bin)
    thd->variables.option_bits |= OPTION_BIN_LOG;
  else
    thd->variables.option_bits &= ~OPTION_BIN_LOG;

  return false;
}

/**
  This function checks if the sql_log_bin can be changed,
  what is possible if:
    - the user is a super user;
    - the set is not called from within a function/trigger;
    - there is no on-going transaction.

  @param     thd    Current thread
  @param[in] self   A pointer to the sys_var, i.e. Sys_log_binlog.
  @param[in] var    A pointer to the set_var created by the parser.

  @return @c false if the change is allowed, otherwise @c true.
*/
static bool check_sql_log_bin(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin(self, thd, var)) return true;

  if (var->is_global_persist()) return true;

  /* If in a stored function/trigger, it's too late to change sql_log_bin. */
  if (thd->in_sub_stmt) {
    my_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_SQL_LOG_BIN, MYF(0));
    return true;
  }
  /* Make the session variable 'sql_log_bin' read-only inside a transaction.
   */
  if (thd->in_active_multi_stmt_transaction()) {
    my_error(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_SQL_LOG_BIN, MYF(0));
    return true;
  }

  return false;
}

static Sys_var_bool Sys_log_binlog(
    "sql_log_bin", "Controls whether logging to the binary log is done",
    SESSION_ONLY(sql_log_bin), NO_CMD_LINE, DEFAULT(true), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_sql_log_bin),
    ON_UPDATE(fix_sql_log_bin_after_update));

static Sys_var_bit Sys_transaction_allow_batching(
    "transaction_allow_batching", "transaction_allow_batching",
    SESSION_ONLY(option_bits), NO_CMD_LINE, OPTION_ALLOW_BATCH, DEFAULT(false));

static Sys_var_bit Sys_sql_warnings("sql_warnings", "sql_warnings",
                                    SESSION_VAR(option_bits), NO_CMD_LINE,
                                    OPTION_WARNINGS, DEFAULT(false));

static Sys_var_bit Sys_sql_notes("sql_notes", "sql_notes",
                                 SESSION_VAR(option_bits), NO_CMD_LINE,
                                 OPTION_SQL_NOTES, DEFAULT(true));

static Sys_var_bit Sys_auto_is_null("sql_auto_is_null", "sql_auto_is_null",
                                    HINT_UPDATEABLE SESSION_VAR(option_bits),
                                    NO_CMD_LINE, OPTION_AUTO_IS_NULL,
                                    DEFAULT(false), NO_MUTEX_GUARD, IN_BINLOG);

static Sys_var_bit Sys_safe_updates("sql_safe_updates", "sql_safe_updates",
                                    HINT_UPDATEABLE SESSION_VAR(option_bits),
                                    NO_CMD_LINE, OPTION_SAFE_UPDATES,
                                    DEFAULT(false));

static Sys_var_bit Sys_buffer_results("sql_buffer_result", "sql_buffer_result",
                                      HINT_UPDATEABLE SESSION_VAR(option_bits),
                                      NO_CMD_LINE, OPTION_BUFFER_RESULT,
                                      DEFAULT(false));

static Sys_var_bit Sys_quote_show_create("sql_quote_show_create",
                                         "sql_quote_show_create",
                                         SESSION_VAR(option_bits), NO_CMD_LINE,
                                         OPTION_QUOTE_SHOW_CREATE,
                                         DEFAULT(true));

static Sys_var_bit Sys_foreign_key_checks(
    "foreign_key_checks", "foreign_key_checks",
    HINT_UPDATEABLE SESSION_VAR(option_bits), NO_CMD_LINE,
    REVERSE(OPTION_NO_FOREIGN_KEY_CHECKS), DEFAULT(true), NO_MUTEX_GUARD,
    IN_BINLOG);

static Sys_var_bit Sys_unique_checks("unique_checks", "unique_checks",
                                     HINT_UPDATEABLE SESSION_VAR(option_bits),
                                     NO_CMD_LINE,
                                     REVERSE(OPTION_RELAXED_UNIQUE_CHECKS),
                                     DEFAULT(true), NO_MUTEX_GUARD, IN_BINLOG);

#ifdef ENABLED_PROFILING
static Sys_var_bit Sys_profiling("profiling", "profiling",
                                 SESSION_VAR(option_bits), NO_CMD_LINE,
                                 OPTION_PROFILING, DEFAULT(false),
                                 NO_MUTEX_GUARD, NOT_IN_BINLOG,
                                 ON_CHECK(nullptr), PRE_UPDATE(nullptr),
                                 ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_ulong Sys_profiling_history_size(
    "profiling_history_size", "Limit of query profiling memory",
    SESSION_VAR(profiling_history_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 100), DEFAULT(15), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), DEPRECATED_VAR(""));
#endif

static Sys_var_harows Sys_select_limit(
    "sql_select_limit",
    "The maximum number of rows to return from SELECT statements",
    HINT_UPDATEABLE SESSION_VAR(select_limit), NO_CMD_LINE,
    VALID_RANGE(0, HA_POS_ERROR), DEFAULT(HA_POS_ERROR), BLOCK_SIZE(1));

static bool update_timestamp(THD *thd, set_var *var) {
  if (var->value) {
    double intpart;
    double fractpart = modf(var->save_result.double_value, &intpart);
    double micros = fractpart * 1000000.0;
    // Double multiplication, and conversion to integral may yield
    // 1000000 rather than 999999.
    struct timeval tmp;
    tmp.tv_sec = llrint(intpart);
    tmp.tv_usec = std::min(llrint(micros), 999999LL);
    thd->set_time(&tmp);
  } else  // SET timestamp=DEFAULT
  {
    thd->user_time.tv_sec = 0;
    thd->user_time.tv_usec = 0;
  }
  return false;
}
static double read_timestamp(THD *thd) {
  return (double)thd->start_time.tv_sec +
         (double)thd->start_time.tv_usec / 1000000;
}

static bool check_timestamp(sys_var *, THD *, set_var *var) {
  double val;

  if (!var->value) return false;

  val = var->save_result.double_value;
  if (val != 0 &&  // this is how you set the default value
      (val < TYPE_TIMESTAMP_MIN_VALUE || val > TYPE_TIMESTAMP_MAX_VALUE)) {
    ErrConvString prm(val);
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "timestamp", prm.ptr());
    return true;
  }
  return false;
}

static Sys_var_session_special_double Sys_timestamp(
    "timestamp", "Set the time for this client",
    HINT_UPDATEABLE sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, 0),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_timestamp),
    ON_UPDATE(update_timestamp), ON_READ(read_timestamp));

static bool update_last_insert_id(THD *thd, set_var *var) {
  if (!var->value) {
    my_error(ER_NO_DEFAULT, MYF(0), var->m_var_tracker.get_var_name());
    return true;
  }
  thd->first_successful_insert_id_in_prev_stmt =
      var->save_result.ulonglong_value;
  return false;
}
static ulonglong read_last_insert_id(THD *thd) {
  return thd->read_first_successful_insert_id_in_prev_stmt();
}
static Sys_var_session_special Sys_last_insert_id(
    "last_insert_id", "The value to be returned from LAST_INSERT_ID()",
    sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULLONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_last_insert_id), ON_READ(read_last_insert_id));

// alias for last_insert_id(), Sybase-style
static Sys_var_session_special Sys_identity(
    "identity", "Synonym for the last_insert_id variable",
    sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULLONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(nullptr),
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
static bool update_insert_id(THD *thd, set_var *var) {
  if (!var->value) {
    my_error(ER_NO_DEFAULT, MYF(0), var->m_var_tracker.get_var_name());
    return true;
  }
  thd->force_one_auto_inc_interval(var->save_result.ulonglong_value);
  return false;
}

static ulonglong read_insert_id(THD *thd) {
  return thd->auto_inc_intervals_forced.minimum();
}
static Sys_var_session_special Sys_insert_id(
    "insert_id",
    "The value to be used by the following INSERT "
    "or ALTER TABLE statement when inserting an AUTO_INCREMENT value",
    HINT_UPDATEABLE sys_var::ONLY_SESSION, NO_CMD_LINE,
    VALID_RANGE(0, ULLONG_MAX), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(update_insert_id), ON_READ(read_insert_id));

static bool update_rand_seed1(THD *thd, set_var *var) {
  if (!var->value) {
    my_error(ER_NO_DEFAULT, MYF(0), var->m_var_tracker.get_var_name());
    return true;
  }
  thd->rand.seed1 = (ulong)var->save_result.ulonglong_value;
  return false;
}
static ulonglong read_rand_seed(THD *) { return 0; }
static Sys_var_session_special Sys_rand_seed1(
    "rand_seed1",
    "Sets the internal state of the RAND() "
    "generator for replication purposes",
    sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_rand_seed1), ON_READ(read_rand_seed));

static bool update_rand_seed2(THD *thd, set_var *var) {
  if (!var->value) {
    my_error(ER_NO_DEFAULT, MYF(0), var->m_var_tracker.get_var_name());
    return true;
  }
  thd->rand.seed2 = (ulong)var->save_result.ulonglong_value;
  return false;
}
static Sys_var_session_special Sys_rand_seed2(
    "rand_seed2",
    "Sets the internal state of the RAND() "
    "generator for replication purposes",
    sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_rand_seed2), ON_READ(read_rand_seed));

static ulonglong read_error_count(THD *thd) {
  return thd->get_stmt_da()->error_count(thd);
}
// this really belongs to the SHOW STATUS
static Sys_var_session_special Sys_error_count(
    "error_count",
    "The number of errors that resulted from the "
    "last statement that generated messages",
    READ_ONLY sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULLONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), ON_READ(read_error_count));

static ulonglong read_warning_count(THD *thd) {
  return thd->get_stmt_da()->warn_count(thd);
}

static ulonglong read_statement_id(THD *thd) {
  return (ulonglong)thd->query_id;
}

// this really belongs to the SHOW STATUS
static Sys_var_session_special Sys_warning_count(
    "warning_count",
    "The number of errors, warnings, and notes "
    "that resulted from the last statement that generated messages",
    READ_ONLY sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, ULLONG_MAX),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), ON_READ(read_warning_count));

static Sys_var_ulong Sys_default_week_format(
    "default_week_format", "The default week format used by WEEK() functions",
    SESSION_VAR(default_week_format), CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 7),
    DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_ulong Sys_group_concat_max_len(
    "group_concat_max_len",
    "The maximum length of the result of function  GROUP_CONCAT()",
    HINT_UPDATEABLE SESSION_VAR(group_concat_max_len), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(4, ULONG_MAX), DEFAULT(1024), BLOCK_SIZE(1));

static char *glob_hostname_ptr;
static Sys_var_charptr Sys_hostname(
    "hostname", "Server host name",
    READ_ONLY NON_PERSIST GLOBAL_VAR(glob_hostname_ptr), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(glob_hostname));

static Sys_var_charptr Sys_repl_report_host(
    "report_host",
    "Hostname or IP that this replica will report to the source while "
    "initiating the replication connection. Will appear in the output of "
    "SHOW REPLICAS. Leave this unset if you do not want the replica to "
    "register itself with the source. Note that it is not sufficient for "
    "the source to simply read the IP of the replica off the socket once the "
    "replica connects: in the presence of NAT other routing features, that IP "
    "may not be valid for connecting to the replica from the source or other "
    "hosts.",
    READ_ONLY GLOBAL_VAR(report_host), CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT(nullptr));

static Sys_var_charptr Sys_repl_report_user(
    "report_user",
    "The account user name that this replica will report to the source "
    "while initiating the replication connection.",
    READ_ONLY GLOBAL_VAR(report_user), CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT(nullptr));

static Sys_var_charptr Sys_repl_report_password(
    "report_password",
    "The account password that this replica will report to the source "
    "while initiating the replication connection.",
    READ_ONLY GLOBAL_VAR(report_password), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_uint Sys_repl_report_port(
    "report_port",
    "The port for connecting to the replica, which this replica will report "
    "to the source while initiating the replication connection. "
    "Set it only if the replica is listening on a non-default "
    "port or if you have a special tunnel from the source or other clients "
    "to this replica. If not sure, leave this option unset.",
    READ_ONLY GLOBAL_VAR(report_port), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, 65535), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_bool Sys_keep_files_on_create(
    "keep_files_on_create",
    "Don't overwrite stale .MYD and .MYI even if no directory is specified",
    SESSION_VAR(keep_files_on_create), CMD_LINE(OPT_ARG), DEFAULT(false));

static char *license;
static Sys_var_charptr Sys_license("license",
                                   "The type of license the server has",
                                   READ_ONLY NON_PERSIST GLOBAL_VAR(license),
                                   NO_CMD_LINE, IN_SYSTEM_CHARSET,
                                   DEFAULT(STRINGIFY_ARG(LICENSE)));

static bool check_log_path(sys_var *self, THD *, set_var *var) {
  if (!var->value) return false;  // DEFAULT is ok

  if (!var->save_result.string_value.str) return true;

  if (!is_valid_log_name(var->save_result.string_value.str,
                         var->save_result.string_value.length)) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str,
             var->save_result.string_value.str);
    return true;
  }

  if (var->save_result.string_value.length > FN_REFLEN) {  // path is too long
    my_error(ER_PATH_LENGTH, MYF(0), self->name.str);
    return true;
  }

  char path[FN_REFLEN];
  size_t path_length = unpack_filename(path, var->save_result.string_value.str);

  if (!path_length) return true;

  if (!is_filename_allowed(var->save_result.string_value.str,
                           var->save_result.string_value.length, true)) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str,
             var->save_result.string_value.str);
    return true;
  }

  MY_STAT f_stat;

  if (my_stat(path, &f_stat, MYF(0))) {
    if (!MY_S_ISREG(f_stat.st_mode) || !(f_stat.st_mode & MY_S_IWRITE))
      return true;  // not a regular writable file
    return false;
  }

  (void)dirname_part(path, var->save_result.string_value.str, &path_length);

  if (var->save_result.string_value.length - path_length >=
      FN_LEN) {  // filename is too long
    my_error(ER_PATH_LENGTH, MYF(0), self->name.str);
    return true;
  }

  if (!path_length)  // no path is good path (remember, relative to datadir)
    return false;

  if (my_access(path, (F_OK | W_OK))) return true;  // directory is not writable

  return false;
}

static bool fix_general_log_file(sys_var *, THD *, enum_var_type) {
  bool res;

  if (!opt_general_logname)  // SET ... = DEFAULT
  {
    char buff[FN_REFLEN];
    opt_general_logname = my_strdup(
        key_memory_LOG_name, make_query_log_name(buff, QUERY_LOG_GENERAL),
        MYF(MY_FAE + MY_WME));
    if (!opt_general_logname) return true;
  }

  res = query_logger.set_log_file(QUERY_LOG_GENERAL);

  if (opt_general_log) {
    mysql_mutex_unlock(&LOCK_global_system_variables);

    if (!res)
      res = query_logger.reopen_log_file(QUERY_LOG_GENERAL);
    else
      query_logger.deactivate_log_handler(QUERY_LOG_GENERAL);

    mysql_mutex_lock(&LOCK_global_system_variables);
  }

  if (res) opt_general_log = false;

  return res;
}

static Sys_var_charptr Sys_general_log_path(
    "general_log_file", "Log connections and queries to given file",
    GLOBAL_VAR(opt_general_logname), CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT(nullptr), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_log_path),
    ON_UPDATE(fix_general_log_file));

static bool fix_slow_log_file(sys_var *, THD *thd [[maybe_unused]],
                              enum_var_type) {
  bool res;

  DEBUG_SYNC(thd, "log_fix_slow_log_holds_sysvar_lock");

  if (!opt_slow_logname)  // SET ... = DEFAULT
  {
    char buff[FN_REFLEN];
    opt_slow_logname = my_strdup(key_memory_LOG_name,
                                 make_query_log_name(buff, QUERY_LOG_SLOW),
                                 MYF(MY_FAE + MY_WME));
    if (!opt_slow_logname) return true;
  }

  res = query_logger.set_log_file(QUERY_LOG_SLOW);

  DEBUG_SYNC(thd, "log_fix_slow_log_released_logger_lock");

  if (opt_slow_log) {
    mysql_mutex_unlock(&LOCK_global_system_variables);

    DEBUG_SYNC(thd, "log_fix_slow_log_released_sysvar_lock");

    if (!res)
      res = query_logger.reopen_log_file(QUERY_LOG_SLOW);
    else
      query_logger.deactivate_log_handler(QUERY_LOG_SLOW);

    mysql_mutex_lock(&LOCK_global_system_variables);
  }

  if (res) opt_slow_log = false;

  return res;
}
static Sys_var_charptr Sys_slow_log_path(
    "slow_query_log_file",
    "Log slow queries to given log file. "
    "Defaults logging to hostname-slow.log. Must be enabled to activate "
    "other slow log options",
    GLOBAL_VAR(opt_slow_logname), CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT(nullptr), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_log_path),
    ON_UPDATE(fix_slow_log_file));

static Sys_var_have Sys_have_compress(
    "have_compress", "have_compress",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_compress), NO_CMD_LINE);

static Sys_var_have Sys_have_dlopen(
    "have_dynamic_loading", "have_dynamic_loading",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_dlopen), NO_CMD_LINE);

static Sys_var_have Sys_have_geometry(
    "have_geometry", "have_geometry",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_geometry), NO_CMD_LINE);

static SHOW_COMP_OPTION have_ssl_func(THD *thd [[maybe_unused]]) {
  return have_ssl() ? SHOW_OPTION_YES : SHOW_OPTION_DISABLED;
}

enum SHOW_COMP_OPTION Sys_var_have_func::dummy_;

static Sys_var_have_func Sys_have_openssl("have_openssl", "have_openssl",
                                          have_ssl_func, DEPRECATED_VAR(""));

static Sys_var_have Sys_have_profiling(
    "have_profiling", "have_profiling",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_profiling), NO_CMD_LINE,
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_have Sys_have_query_cache(
    "have_query_cache",
    "have_query_cache. "
    "This variable is deprecated and will be removed in a future release.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_query_cache), NO_CMD_LINE,
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_have Sys_have_rtree_keys(
    "have_rtree_keys", "have_rtree_keys",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_rtree_keys), NO_CMD_LINE);

static Sys_var_have_func Sys_have_ssl(
    "have_ssl", "have_ssl", have_ssl_func,
    DEPRECATED_VAR("performance_schema.tls_channel_status table"));

static Sys_var_have Sys_have_symlink(
    "have_symlink", "have_symlink",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_symlink), NO_CMD_LINE);

static Sys_var_have Sys_have_statement_timeout(
    "have_statement_timeout", "have_statement_timeout",
    READ_ONLY NON_PERSIST GLOBAL_VAR(have_statement_timeout), NO_CMD_LINE);

static bool fix_general_log_state(sys_var *, THD *thd, enum_var_type) {
  bool new_state = opt_general_log, res = false;

  if (query_logger.is_log_file_enabled(QUERY_LOG_GENERAL) == new_state)
    return false;

  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (!new_state) {
    query_logger.deactivate_log_handler(QUERY_LOG_GENERAL);
  } else {
    res = query_logger.activate_log_handler(thd, QUERY_LOG_GENERAL);
  }

  mysql_mutex_lock(&LOCK_global_system_variables);

  if (res) opt_general_log = false;

  return res;
}
static Sys_var_bool Sys_general_log(
    "general_log",
    "Log connections and queries to a table or log file. "
    "Defaults to logging to a file hostname.log, "
    "or if --log-output=TABLE is used, to a table mysql.general_log.",
    GLOBAL_VAR(opt_general_log), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_general_log_state));

static Sys_var_bool Sys_log_raw(
    "log_raw",
    "Log to general log before any rewriting of the query. For use in "
    "debugging, not production as sensitive information may be logged.",
    GLOBAL_VAR(opt_general_log_raw), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG);

static bool fix_slow_log_state(sys_var *, THD *thd, enum_var_type) {
  bool new_state = opt_slow_log, res = false;

  if (query_logger.is_log_file_enabled(QUERY_LOG_SLOW) == new_state)
    return false;

  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (!new_state) {
    query_logger.deactivate_log_handler(QUERY_LOG_SLOW);
  } else {
    res = query_logger.activate_log_handler(thd, QUERY_LOG_SLOW);
  }

  mysql_mutex_lock(&LOCK_global_system_variables);

  if (res) opt_slow_log = false;

  return res;
}
static Sys_var_bool Sys_slow_query_log(
    "slow_query_log",
    "Log slow queries to a table or log file. Defaults logging to a file "
    "hostname-slow.log or a table mysql.slow_log if --log-output=TABLE is "
    "used. Must be enabled to activate other slow log options",
    GLOBAL_VAR(opt_slow_log), CMD_LINE(OPT_ARG), DEFAULT(false), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(fix_slow_log_state));

static bool check_slow_log_extra(sys_var *, THD *thd, set_var *) {
  // If FILE is not one of the log-targets, succeed but warn!
  if (!(log_output_options & LOG_FILE))
    push_warning(
        thd, Sql_condition::SL_WARNING,
        ER_SLOW_LOG_MODE_IGNORED_WHEN_NOT_LOGGING_TO_FILE,
        ER_THD(thd, ER_SLOW_LOG_MODE_IGNORED_WHEN_NOT_LOGGING_TO_FILE));

  return false;
}

static Sys_var_bool Sys_slow_log_extra(
    "log_slow_extra",
    "Print more attributes to the slow query log file. Has no effect on "
    "logging to table.",
    GLOBAL_VAR(opt_log_slow_extra), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_slow_log_extra),
    ON_UPDATE(nullptr));

static bool check_not_empty_set(sys_var *, THD *, set_var *var) {
  return var->save_result.ulonglong_value == 0;
}
static bool fix_log_output(sys_var *, THD *, enum_var_type) {
  query_logger.set_handlers(static_cast<uint>(log_output_options));
  return false;
}

static const char *log_output_names[] = {"NONE", "FILE", "TABLE", nullptr};

static Sys_var_set Sys_log_output(
    "log_output",
    "Syntax: log-output=value[,value...], "
    "where \"value\" could be TABLE, FILE or NONE",
    GLOBAL_VAR(log_output_options), CMD_LINE(REQUIRED_ARG), log_output_names,
    DEFAULT(LOG_FILE), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_not_empty_set), ON_UPDATE(fix_log_output));

static Sys_var_bool Sys_log_replica_updates(
    "log_replica_updates",
    "If enabled, the replication applier threads will write to this server's "
    "binary log.",
    READ_ONLY GLOBAL_VAR(opt_log_replica_updates),
    CMD_LINE(OPT_ARG, OPT_LOG_REPLICA_UPDATES), DEFAULT(1));

static Sys_var_deprecated_alias Sys_log_slave_updates("log_slave_updates",
                                                      Sys_log_replica_updates);

static Sys_var_charptr Sys_relay_log(
    "relay_log", "The location and name to use for relay logs",
    READ_ONLY NON_PERSIST GLOBAL_VAR(opt_relay_logname), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr));

/*
  Uses NO_CMD_LINE since the --relay-log-index option set
  opt_relaylog_index_name variable and computes a value for the
  relay_log_index variable.
*/
static Sys_var_charptr Sys_relay_log_index(
    "relay_log_index",
    "The location and name to use for the file "
    "that keeps a list of the last relay logs",
    READ_ONLY NON_PERSIST GLOBAL_VAR(relay_log_index), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(nullptr));

/*
  Uses NO_CMD_LINE since the --log-bin-index option set
  opt_binlog_index_name variable and computes a value for the
  log_bin_index variable.
*/
static Sys_var_charptr Sys_binlog_index(
    "log_bin_index", "File that holds the names for last binary log files.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(log_bin_index), NO_CMD_LINE, IN_FS_CHARSET,
    DEFAULT(nullptr));

static Sys_var_charptr Sys_relay_log_basename(
    "relay_log_basename",
    "The full path of the relay log file names, excluding the extension.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(relay_log_basename), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_charptr Sys_log_bin_basename(
    "log_bin_basename",
    "The full path of the binary log file names, excluding the extension.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(log_bin_basename), NO_CMD_LINE,
    IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_charptr Sys_relay_log_info_file(
    "relay_log_info_file",
    "The location and name of the file that "
    "remembers where the SQL replication thread is in the relay logs",
    READ_ONLY NON_PERSIST GLOBAL_VAR(relay_log_info_file),
    CMD_LINE(REQUIRED_ARG, OPT_RELAY_LOG_INFO_FILE), IN_FS_CHARSET,
    DEFAULT(nullptr), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_bool Sys_relay_log_purge(
    "relay_log_purge",
    "if disabled - do not purge relay logs. "
    "if enabled - purge them as soon as they are no more needed",
    GLOBAL_VAR(relay_log_purge), CMD_LINE(OPT_ARG), DEFAULT(true));

static Sys_var_bool Sys_relay_log_recovery(
    "relay_log_recovery",
    "If enabled, existing relay logs will be skipped by the "
    "replication threads. The receiver will start a new relay "
    "log and the applier will start reading from the beginning of that file. "
    "The receiver's position relative to the source will be reset to the "
    "applier's "
    "position relative to the source; the receiver uses this in case "
    "SOURCE_AUTO_POSITION=0.",
    READ_ONLY GLOBAL_VAR(relay_log_recovery), CMD_LINE(OPT_ARG),
    DEFAULT(false));

static Sys_var_ulong Sys_rpl_read_size(
    "rpl_read_size",
    "The size for reads done from the binlog and relay log. "
    "It must be a multiple of 4kb. Making it larger might help with IO "
    "stalls while reading these files when they are not in the OS buffer "
    "cache",
    GLOBAL_VAR(rpl_read_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(IO_SIZE * 2, ULONG_MAX), DEFAULT(IO_SIZE * 2),
    BLOCK_SIZE(IO_SIZE));

static Sys_var_bool Sys_replica_allow_batching(
    "replica_allow_batching",
    "Allow this replica to batch requests when "
    "using the NDB storage engine.",
    GLOBAL_VAR(opt_replica_allow_batching), CMD_LINE(OPT_ARG), DEFAULT(true));

static Sys_var_deprecated_alias Sys_slave_allow_batching(
    "slave_allow_batching", Sys_replica_allow_batching);

static Sys_var_charptr Sys_replica_load_tmpdir(
    "replica_load_tmpdir",
    "The location where this replica will store temporary files when "
    "replicating a LOAD DATA INFILE command from a source having "
    "binlog_format=STATEMENT.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(replica_load_tmpdir),
    CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET, DEFAULT(nullptr));

static Sys_var_deprecated_alias Sys_slave_load_tmpdir("slave_load_tmpdir",
                                                      Sys_replica_load_tmpdir);

static bool fix_replica_net_timeout(sys_var *, THD *thd, enum_var_type) {
  DEBUG_SYNC(thd, "fix_replica_net_timeout");
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
  mysql_mutex_unlock(&LOCK_replica_net_timeout);
  mysql_mutex_unlock(&LOCK_global_system_variables);
  channel_map.wrlock();

  for (mi_map::iterator it = channel_map.begin(); it != channel_map.end();
       it++) {
    mi = it->second;

    DBUG_PRINT("info",
               ("replica_net_timeout=%u mi->heartbeat_period=%.3f",
                replica_net_timeout, (mi ? mi->heartbeat_period : 0.0)));
    if (mi != nullptr && replica_net_timeout < mi->heartbeat_period)
      push_warning(thd, Sql_condition::SL_WARNING,
                   ER_REPLICA_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX,
                   ER_THD(thd, ER_REPLICA_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX));
  }

  channel_map.unlock();
  mysql_mutex_lock(&LOCK_global_system_variables);
  mysql_mutex_lock(&LOCK_replica_net_timeout);
  return false;
}
static PolyLock_mutex PLock_replica_net_timeout(&LOCK_replica_net_timeout);
static Sys_var_uint Sys_replica_net_timeout(
    "replica_net_timeout",
    "Number of seconds to wait for more data "
    "from a replication connection before aborting the read.",
    GLOBAL_VAR(replica_net_timeout), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1, LONG_TIMEOUT), DEFAULT(REPLICA_NET_TIMEOUT), BLOCK_SIZE(1),
    &PLock_replica_net_timeout, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_replica_net_timeout));

static Sys_var_deprecated_alias Sys_slave_net_timeout("slave_net_timeout",
                                                      Sys_replica_net_timeout);

static bool check_slave_skip_counter(sys_var *, THD *thd, set_var *var) {
  /*
    @todo: move this check into the set function and hold the lock on
    Gtid_mode::lock until the operation has completed, so that we are
    sure a concurrent connection does not change gtid_mode between
    check and fix.
  */
  if (global_gtid_mode.get() == Gtid_mode::ON &&
      var->save_result.ulonglong_value > 0)
    push_warning(
        thd, Sql_condition::SL_WARNING,
        ER_SQL_REPLICA_SKIP_COUNTER_USED_WITH_GTID_MODE_ON,
        ER_THD(thd, ER_SQL_REPLICA_SKIP_COUNTER_USED_WITH_GTID_MODE_ON));
  return false;
}

static PolyLock_mutex PLock_sql_replica_skip_counter(
    &LOCK_sql_replica_skip_counter);
static Sys_var_uint Sys_sql_replica_skip_counter(
    "sql_replica_skip_counter", "sql_replica_skip_counter",
    GLOBAL_VAR(sql_replica_skip_counter), NO_CMD_LINE, VALID_RANGE(0, UINT_MAX),
    DEFAULT(0), BLOCK_SIZE(1), &PLock_sql_replica_skip_counter, NOT_IN_BINLOG,
    ON_CHECK(check_slave_skip_counter));

static Sys_var_deprecated_alias Sys_sql_slave_skip_counter(
    "sql_slave_skip_counter", Sys_sql_replica_skip_counter);

static Sys_var_charptr Sys_replica_skip_errors(
    "replica_skip_errors",
    "Comma-separated list of error numbers. If an applier thread on this "
    "replica encounters one of these errors while applying a Query_log_event, "
    "it will ignore the error, rather than stop.",
    READ_ONLY GLOBAL_VAR(opt_replica_skip_errors), CMD_LINE(REQUIRED_ARG),
    IN_SYSTEM_CHARSET, DEFAULT(nullptr));

static Sys_var_deprecated_alias Sys_slave_skip_errors("slave_skip_errors",
                                                      Sys_replica_skip_errors);

static Sys_var_ulonglong Sys_relay_log_space_limit(
    "relay_log_space_limit", "Maximum space to use for all relay logs",
    READ_ONLY GLOBAL_VAR(relay_log_space_limit), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULLONG_MAX), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_relaylog_period(
    "sync_relay_log",
    "Synchronously flush relay log to disk after "
    "every #th event. Use 0 to disable synchronous flushing",
    GLOBAL_VAR(sync_relaylog_period), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX), DEFAULT(10000), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_relayloginfo_period(
    "sync_relay_log_info",
    "Synchronously flush relay log info "
    "to disk after every #th transaction. Use 0 to disable "
    "synchronous flushing. This variable is deprecated and will be removed in "
    "a future version.",
    GLOBAL_VAR(sync_relayloginfo_period),
    CMD_LINE(REQUIRED_ARG, OPT_SYNC_RELAY_LOG_INFO), VALID_RANGE(0, UINT_MAX),
    DEFAULT(10000), BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr), DEPRECATED_VAR(""));

static Sys_var_uint Sys_replica_checkpoint_period(
    "replica_checkpoint_period",
    "When using a multi-threaded applier (replica_parallel_workers>0), it "
    "will update the worker progress status periodically. This option "
    "specifies the maximum number of milliseconds between updates.",
    GLOBAL_VAR(opt_mta_checkpoint_period), CMD_LINE(REQUIRED_ARG),
#ifndef NDEBUG
    VALID_RANGE(0, UINT_MAX), DEFAULT(300), BLOCK_SIZE(1));
#else
    VALID_RANGE(1, UINT_MAX), DEFAULT(300), BLOCK_SIZE(1));
#endif /* NDEBUG */

static Sys_var_deprecated_alias Sys_slave_checkpoint_period(
    "slave_checkpoint_period", Sys_replica_checkpoint_period);

static Sys_var_uint Sys_replica_checkpoint_group(
    "replica_checkpoint_group",
    "When using multi-threaded applier (replica_parallel_workers>0), it will "
    "update the worker progress status periodically. This option specifies "
    "the maximum number of committed transactions between updates.",
    GLOBAL_VAR(opt_mta_checkpoint_group), CMD_LINE(REQUIRED_ARG),
#ifndef NDEBUG
    VALID_RANGE(1, MTS_MAX_BITS_IN_GROUP), DEFAULT(512), BLOCK_SIZE(1));
#else
    VALID_RANGE(32, MTS_MAX_BITS_IN_GROUP), DEFAULT(512), BLOCK_SIZE(8));
#endif /* NDEBUG */

static Sys_var_deprecated_alias Sys_slave_checkpoint_group(
    "slave_checkpoint_group", Sys_replica_checkpoint_group);

static Sys_var_uint Sys_sync_binlog_period(
    "sync_binlog",
    "Synchronously flush binary log to disk after"
    " every #th write to the file. Use 0 to disable synchronous"
    " flushing",
    GLOBAL_VAR(sync_binlog_period), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX), DEFAULT(1), BLOCK_SIZE(1));

static Sys_var_uint Sys_sync_source_info(
    "sync_source_info",
    "Synchronize replication receiver positions to disk periodically, after "
    "the specified number of events. Use 0 to disable periodic "
    "synchronization.",
    GLOBAL_VAR(sync_masterinfo_period), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX), DEFAULT(10000), BLOCK_SIZE(1));

static Sys_var_deprecated_alias Sys_sync_master_info("sync_master_info",
                                                     Sys_sync_source_info);

static Sys_var_ulonglong Sys_var_original_commit_timestamp(
    "original_commit_timestamp",
    "The time when the current transaction was committed on the originating "
    "source, measured in microseconds since 1970 (the \"epoch\").",
    SESSION_ONLY(original_commit_timestamp), NO_CMD_LINE,
    VALID_RANGE(0, MAX_COMMIT_TIMESTAMP_VALUE),
    DEFAULT(MAX_COMMIT_TIMESTAMP_VALUE), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    IN_BINLOG, ON_CHECK(check_session_admin_or_replication_applier));

static Sys_var_ulong Sys_replica_transaction_retries(
    "replica_transaction_retries",
    "Number of times the replication applier will retry a transaction in "
    "case it failed with a deadlock or other transient error, before it gives "
    "up and stops.",
    GLOBAL_VAR(slave_trans_retries), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, ULONG_MAX), DEFAULT(10), BLOCK_SIZE(1));

static Sys_var_deprecated_alias Sys_slave_transaction_retries(
    "slave_transaction_retries", Sys_replica_transaction_retries);

static Sys_var_ulong Sys_replica_parallel_workers(
    "replica_parallel_workers",
    "Number of worker threads for executing events in parallel ",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_mts_replica_parallel_workers),
    CMD_LINE(REQUIRED_ARG, OPT_REPLICA_PARALLEL_WORKERS),
    VALID_RANGE(0, MTS_MAX_WORKERS), DEFAULT(4), BLOCK_SIZE(1), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(replica_parallel_workers_update));

static Sys_var_deprecated_alias Sys_slave_parallel_workers(
    "slave_parallel_workers", Sys_replica_parallel_workers);

static Sys_var_ulonglong Sys_replica_pending_jobs_size_max(
    "replica_pending_jobs_size_max",
    "Soft limit on the size, in bytes, of per-worker queues of events that "
    "have not yet been applied. The queue size may exceed this limit in case "
    "a single event is bigger than the limit.",
    GLOBAL_VAR(opt_mts_pending_jobs_size_max), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(1024, (ulonglong) ~(intptr)0), DEFAULT(128 * 1024 * 1024),
    BLOCK_SIZE(1024), ON_CHECK(nullptr));

static Sys_var_deprecated_alias Sys_slave_pending_jobs_size_max(
    "slave_pending_jobs_size_max", Sys_replica_pending_jobs_size_max);

static bool check_locale(sys_var *self, THD *thd, set_var *var) {
  if (!var->value) return false;

  MY_LOCALE *locale;
  char buff[STRING_BUFFER_USUAL_SIZE];
  if (var->value->result_type() == INT_RESULT) {
    int lcno = (int)var->value->val_int();
    if (!(locale = my_locale_by_number(lcno))) {
      my_error(ER_UNKNOWN_LOCALE, MYF(0), llstr(lcno, buff));
      return true;
    }
    if (check_not_null(self, thd, var)) return true;
  } else  // STRING_RESULT
  {
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (!(res = var->value->val_str(&str)))
      return true;
    else if (!(locale = my_locale_by_name(thd, res->ptr(), res->length()))) {
      ErrConvString err(res);
      my_error(ER_UNKNOWN_LOCALE, MYF(0), err.ptr());
      return true;
    }
  }

  var->save_result.ptr = locale;

  if (!locale->errmsgs->is_loaded()) {
    mysql_mutex_lock(&LOCK_error_messages);
    if (!locale->errmsgs->is_loaded() && locale->errmsgs->read_texts()) {
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

namespace {
struct Get_locale_name {
  explicit Get_locale_name(const MY_LOCALE *ml) : m_ml(ml) {}
  const uchar *get_name() const {
    return pointer_cast<const uchar *>(m_ml->name);
  }
  const MY_LOCALE *m_ml;
};
}  // namespace

static Sys_var_struct<MY_LOCALE, Get_locale_name> Sys_lc_messages(
    "lc_messages", "Set the language used for the error messages",
    SESSION_VAR(lc_messages), NO_CMD_LINE, DEFAULT(&my_default_lc_messages),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_locale));

static Sys_var_struct<MY_LOCALE, Get_locale_name> Sys_lc_time_names(
    "lc_time_names",
    "Set the language used for the month "
    "names and the days of the week",
    SESSION_VAR(lc_time_names), NO_CMD_LINE, DEFAULT(&my_default_lc_time_names),
    NO_MUTEX_GUARD, IN_BINLOG, ON_CHECK(check_locale));

static Sys_var_tz Sys_time_zone("time_zone", "time_zone",
                                HINT_UPDATEABLE SESSION_VAR(time_zone),
                                NO_CMD_LINE, DEFAULT(&default_tz),
                                NO_MUTEX_GUARD, IN_BINLOG);

static bool fix_host_cache_size(sys_var *, THD *, enum_var_type) {
  hostname_cache_resize(host_cache_size);
  return false;
}

static Sys_var_uint Sys_host_cache_size(
    "host_cache_size",
    "How many host names should be cached to avoid resolving.",
    GLOBAL_VAR(host_cache_size), CMD_LINE(REQUIRED_ARG, OPT_HOST_CACHE_SIZE),
    VALID_RANGE(0, 65536), DEFAULT(HOST_CACHE_SIZE), BLOCK_SIZE(1),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(fix_host_cache_size));

const Sys_var_multi_enum::ALIAS enforce_gtid_consistency_aliases[] = {
    {"OFF", 0},   {"ON", 1},   {"WARN", 2},
    {"FALSE", 0}, {"TRUE", 1}, {nullptr, 0}};
static Sys_var_enforce_gtid_consistency Sys_enforce_gtid_consistency(
    "enforce_gtid_consistency",
    "Prevents execution of statements that would be impossible to log "
    "in a transactionally safe manner. Currently, the disallowed "
    "statements include CREATE TEMPORARY TABLE inside transactions, "
    "all updates to non-transactional tables, and CREATE TABLE ... SELECT.",
    PERSIST_AS_READONLY GLOBAL_VAR(_gtid_consistency_mode),
    CMD_LINE(OPT_ARG, OPT_ENFORCE_GTID_CONSISTENCY),
    enforce_gtid_consistency_aliases, 3,
    DEFAULT(3 /*position of "FALSE" in enforce_gtid_consistency_aliases*/),
    DEFAULT(GTID_CONSISTENCY_MODE_ON), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin_outside_trx_outside_sf_outside_sp));
const char *fixup_enforce_gtid_consistency_command_line(char *value_arg) {
  return Sys_enforce_gtid_consistency.fixup_command_line(value_arg);
}

static Sys_var_bool Sys_binlog_gtid_simple_recovery(
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
    READ_ONLY GLOBAL_VAR(binlog_gtid_simple_recovery), CMD_LINE(OPT_ARG),
    DEFAULT(true));

static Sys_var_ulong Sys_sp_cache_size(
    "stored_program_cache",
    "The soft upper limit for number of cached stored routines for "
    "one connection.",
    GLOBAL_VAR(stored_program_cache_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(16, 512 * 1024), DEFAULT(256), BLOCK_SIZE(1));

static bool check_pseudo_replica_mode(sys_var *self, THD *thd, set_var *var) {
  if (check_session_admin_or_replication_applier(self, thd, var)) return true;
  if (check_outside_trx(self, thd, var)) return true;
  longlong previous_val = thd->variables.pseudo_replica_mode;
  longlong val = (longlong)var->save_result.ulonglong_value;
  bool rli_fake = false;

  rli_fake = thd->rli_fake ? true : false;

  if (rli_fake) {
    if (!val) {
      thd->rli_fake->end_info();
      delete thd->rli_fake;
      thd->rli_fake = nullptr;
    } else if (previous_val && val)
      goto ineffective;
    else if (!previous_val && val)
      push_warning(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE_FOR_VAR,
                   "'pseudo_replica_mode' is already ON.");
  } else {
    if (!previous_val && !val)
      goto ineffective;
    else if (previous_val && !val)
      push_warning(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE_FOR_VAR,
                   "Replica applier execution mode not active, "
                   "statement ineffective.");
  }
  goto end;

ineffective:
  push_warning(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE_FOR_VAR,
               "'pseudo_replica_mode' change was ineffective.");

end:
  return false;
}
static Sys_var_bool Sys_pseudo_replica_mode(
    "pseudo_replica_mode",
    "Internal variable that will be enabled while applying a "
    "Format_description_log_event encoded in a BINLOG statement printed "
    "by mysqlbinlog.",
    SESSION_ONLY(pseudo_replica_mode), NO_CMD_LINE, DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_pseudo_replica_mode));

static Sys_var_deprecated_alias Sys_pseudo_slave_mode("pseudo_slave_mode",
                                                      Sys_pseudo_replica_mode);

#ifdef HAVE_GTID_NEXT_LIST
static bool check_gtid_next_list(sys_var *self, THD *thd, set_var *var) {
  DBUG_TRACE;
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "GTID_NEXT_LIST");
  if (check_session_admin_outside_trx_outside_sf_outside_sp(self, thd, var))
    return true;
  /*
    @todo: move this check into the set function and hold the lock on
    Gtid_mode::lock until the operation has completed, so that we are
    sure a concurrent connection does not change gtid_mode between
    check and fix - if we ever implement this variable.
  */
  if (global_gtid_mode.get() == Gtid_mode::OFF &&
      var->save_result.string_value.str != NULL)
    my_error(ER_CANT_SET_GTID_NEXT_LIST_TO_NON_NULL_WHEN_GTID_MODE_IS_OFF,
             MYF(0));
  return false;
}

static bool update_gtid_next_list(sys_var *self, THD *thd, enum_var_type type) {
  assert(type == OPT_SESSION);
  if (thd->get_gtid_next_list() != NULL)
    return gtid_acquire_ownership_multiple(thd) != 0 ? true : false;
  return false;
}

static Sys_var_gtid_set Sys_gtid_next_list(
    "gtid_next_list",
    "Before re-executing a transaction that contains multiple "
    "Global Transaction Identifiers, this variable must be set "
    "to the set of all re-executed transactions.",
    SESSION_ONLY(gtid_next_list), NO_CMD_LINE, DEFAULT(NULL), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_gtid_next_list),
    ON_UPDATE(update_gtid_next_list));
export sys_var *Sys_gtid_next_list_ptr = &Sys_gtid_next_list;
#endif  // HAVE_GTID_NEXT_LIST

static Sys_var_gtid_next Sys_gtid_next(
    "gtid_next",
    "Specifies the Global Transaction Identifier for the following "
    "transaction.",
    SESSION_ONLY(gtid_next), NO_CMD_LINE, DEFAULT("AUTOMATIC"), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_gtid_next));
export sys_var *Sys_gtid_next_ptr = &Sys_gtid_next;

static Sys_var_gtid_executed Sys_gtid_executed(
    "gtid_executed",
    "The global variable contains the set of GTIDs in the "
    "binary log. The session variable contains the set of GTIDs "
    "in the current, ongoing transaction.");

static bool check_gtid_purged(sys_var *self, THD *thd, set_var *var) {
  DBUG_TRACE;

  /*
    GTID_PURGED must not be set / updated when GR is running (it goes against
    the whole purpose of update everywhere replication).
  */
  if (is_group_replication_running()) {
    my_error(ER_UPDATE_GTID_PURGED_WITH_GR, MYF(0));
    return true;
  }

  if (!var->value || check_session_admin_outside_trx_outside_sf(self, thd, var))
    return true;

  if (var->value->result_type() != STRING_RESULT ||
      !var->save_result.string_value.str)
    return true;

  return false;
}

bool Sys_var_gtid_purged::global_update(THD *thd, set_var *var) {
  DBUG_TRACE;
  bool error = false;
  bool gtid_threshold_breach = false;

  global_sid_lock->wrlock();

  /*
    ensures the commit of the transaction started when saving the
    purged gtid set in the table
  */
  thd->lex->autocommit = true;

  /*
    SET GITD_PURGED command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.gtid_executed' replication repository
    table.
  */
  thd->set_skip_readonly_check();
  char *previous_gtid_executed = nullptr, *previous_gtid_purged = nullptr,
       *current_gtid_executed = nullptr, *current_gtid_purged = nullptr;
  gtid_state->get_executed_gtids()->to_string(&previous_gtid_executed);
  gtid_state->get_lost_gtids()->to_string(&previous_gtid_purged);
  Gtid_set gtid_set(global_sid_map, global_sid_lock);
  bool starts_with_plus = false;
  enum_return_status ret = gtid_set.add_gtid_text(
      var->save_result.string_value.str, nullptr, &starts_with_plus);

  if (ret != RETURN_STATUS_OK) {
    error = true;
    goto end;
  }
  ret = gtid_state->add_lost_gtids(&gtid_set, starts_with_plus);
  if (ret != RETURN_STATUS_OK) {
    error = true;
    goto end;
  }
  gtid_state->get_executed_gtids()->to_string(&current_gtid_executed);
  gtid_state->get_lost_gtids()->to_string(&current_gtid_purged);

  gtid_threshold_breach =
      (gtid_state->get_executed_gtids()->get_gtid_count(
           gtid_state->get_server_sidno()) > GNO_WARNING_THRESHOLD);

  // Log messages saying that GTID_PURGED and GTID_EXECUTED were changed.
  LogErr(SYSTEM_LEVEL, ER_GTID_PURGED_WAS_UPDATED, previous_gtid_purged,
         current_gtid_purged);
  LogErr(SYSTEM_LEVEL, ER_GTID_EXECUTED_WAS_UPDATED, previous_gtid_executed,
         current_gtid_executed);

end:
  global_sid_lock->unlock();
  my_free(previous_gtid_executed);
  my_free(previous_gtid_purged);
  my_free(current_gtid_executed);
  my_free(current_gtid_purged);

  if (gtid_threshold_breach)
    LogErr(WARNING_LEVEL, ER_WARN_GTID_THRESHOLD_BREACH);

  return error;
}

Gtid_set *gtid_purged;
static Sys_var_gtid_purged Sys_gtid_purged(
    "gtid_purged",
    "The set of GTIDs that existed in previous, purged binary logs.",
    NON_PERSIST GLOBAL_VAR(gtid_purged), NO_CMD_LINE, DEFAULT(nullptr),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_gtid_purged));
export sys_var *Sys_gtid_purged_ptr = &Sys_gtid_purged;

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
    "ON is required on a source before any replica can use "
    "SOURCE_AUTO_POSITION=1. To safely switch from OFF to ON, first "
    "set all servers to OFF_PERMISSIVE, then set all servers to "
    "ON_PERMISSIVE, then wait for all transactions without a GTID to "
    "be replicated and executed on all servers, and finally set all "
    "servers to GTID_MODE = ON.",
    PERSIST_AS_READONLY GLOBAL_VAR(Gtid_mode::sysvar_mode),
    CMD_LINE(REQUIRED_ARG), Gtid_mode::names, DEFAULT(Gtid_mode::DEFAULT),
    NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin_outside_trx_outside_sf_outside_sp));

static Sys_var_uint Sys_gtid_executed_compression_period(
    "gtid_executed_compression_period",
    "Compress the mysql.gtid_executed table whenever this number of "
    "transactions have been added, by waking up a foreground thread "
    "(compress_gtid_table). This compression method only operates when "
    "binary logging is disabled on the replica; if binary logging is "
    "enabled, the table is compressed every time the binary log is "
    "rotated, and this value is ignored. Before MySQL 8.0.23, the "
    "default is 1000, and from MySQL 8.0.23, the default is zero, which "
    "disables this compression method. This is because in releases from "
    "MySQL 8.0.17, InnoDB transactions are written to the "
    "mysql.gtid_executed table by a separate process to non-InnoDB "
    "transactions. If the server has a mix of InnoDB and non-InnoDB "
    "transactions, attempting to compress the table with the "
    "compress_gtid_table thread can slow this process, so from "
    "MySQL 8.0.17 it is recommended that you set "
    "gtid_executed_compression_period to 0.",
    GLOBAL_VAR(gtid_executed_compression_period), CMD_LINE(OPT_ARG),
    VALID_RANGE(0, UINT_MAX32), DEFAULT(0), BLOCK_SIZE(1));

static Sys_var_bool Sys_disconnect_on_expired_password(
    "disconnect_on_expired_password",
    "Give clients that don't signal password expiration support execution "
    "time "
    "error(s) instead of connection error",
    READ_ONLY GLOBAL_VAR(disconnect_on_expired_password), CMD_LINE(OPT_ARG),
    DEFAULT(true));

static Sys_var_bool Sys_validate_user_plugins(
    "validate_user_plugins",
    "Turns on additional validation of authentication plugins assigned "
    "to user accounts. ",
    READ_ONLY NOT_VISIBLE GLOBAL_VAR(validate_user_plugins), CMD_LINE(OPT_ARG),
    DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG);

static Sys_var_enum Sys_block_encryption_mode(
    "block_encryption_mode", "mode for AES_ENCRYPT/AES_DECRYPT",
    SESSION_VAR(my_aes_mode), CMD_LINE(REQUIRED_ARG), my_aes_opmode_names,
    DEFAULT(my_aes_128_ecb));

static bool check_track_session_sys_vars(sys_var *, THD *thd, set_var *var) {
  DBUG_TRACE;
  return thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
      ->check(thd, var);
  return false;
}

static bool update_track_session_sys_vars(sys_var *, THD *thd,
                                          enum_var_type type) {
  DBUG_TRACE;
  /* Populate map only for session variable. */
  if (type == OPT_SESSION)
    return thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
        ->update(thd);
  return false;
}

static Sys_var_charptr Sys_track_session_sys_vars(
    "session_track_system_variables",
    "Track changes in registered system variables.",
    SESSION_VAR(track_sysvars_ptr), CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
    DEFAULT("time_zone,autocommit,character_set_client,character_set_results,"
            "character_set_connection"),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_track_session_sys_vars),
    ON_UPDATE(update_track_session_sys_vars));
export sys_var *Sys_track_session_sys_vars_ptr = &Sys_track_session_sys_vars;

static bool update_session_track_schema(sys_var *, THD *thd, enum_var_type) {
  DBUG_TRACE;
  return thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->update(thd);
}

static Sys_var_bool Sys_session_track_schema(
    "session_track_schema", "Track changes to the 'default schema'.",
    SESSION_VAR(session_track_schema), CMD_LINE(OPT_ARG), DEFAULT(true),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_session_track_schema));

static bool update_session_track_tx_info(sys_var *, THD *thd, enum_var_type) {
  DBUG_TRACE;
  TX_TRACKER_GET(tst);
  return tst->update(thd);
}

static const char *session_track_transaction_info_names[] = {
    "OFF", "STATE", "CHARACTERISTICS", NullS};

static Sys_var_enum Sys_session_track_transaction_info(
    "session_track_transaction_info",
    "Track changes to the transaction attributes. OFF to disable; "
    "STATE to track just transaction state (Is there an active transaction? "
    "Does it have any data? etc.); CHARACTERISTICS to track transaction "
    "state "
    "and report all statements needed to start a transaction with the same "
    "characteristics (isolation level, read only/read write, snapshot - "
    "but not any work done / data modified within the transaction).",
    SESSION_VAR(session_track_transaction_info), CMD_LINE(REQUIRED_ARG),
    session_track_transaction_info_names, DEFAULT(TX_TRACK_NONE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_session_track_tx_info));

static bool update_session_track_state_change(sys_var *, THD *thd,
                                              enum_var_type) {
  DBUG_TRACE;
  return thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
      ->update(thd);
}

static Sys_var_bool Sys_session_track_state_change(
    "session_track_state_change", "Track changes to the 'session state'.",
    SESSION_VAR(session_track_state_change), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(update_session_track_state_change));

static bool handle_offline_mode(sys_var *, THD *thd, enum_var_type) {
  DBUG_TRACE;
  DEBUG_SYNC(thd, "after_lock_offline_mode_acquire");

  if (mysqld_offline_mode()) {
    // Unlock the global system variable lock as kill holds LOCK_thd_data.
    mysql_mutex_unlock(&LOCK_global_system_variables);
    killall_non_super_threads(thd);
    mysql_mutex_lock(&LOCK_global_system_variables);
  }

  return false;
}

/**
  Checks if user has an additional CONNECTION_ADMIN privilege, needed
  to modify OFFLINE_MODE system variable (unless having SUPER).

  @retval true failure
  @retval false success

  @param thd the session context
*/
static bool check_offline_mode(sys_var * /*self*/, THD *thd,
                               set_var * /*setv*/) {
  Security_context *sctx = thd->security_context();
  if (!sctx->has_global_grant(STRING_WITH_LEN("CONNECTION_ADMIN")).first &&
      !sctx->check_access(SUPER_ACL)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SYSTEM_VARIABLES_ADMIN plus CONNECTION_ADMIN or SUPER");
    return true;
  }
  return false;
}

static Sys_var_bool Sys_offline_mode("offline_mode",
                                     "Make the server into offline mode",
                                     GLOBAL_VAR(offline_mode),
                                     CMD_LINE(OPT_ARG), DEFAULT(false),
                                     NO_MUTEX_GUARD, NOT_IN_BINLOG,
                                     ON_CHECK(check_offline_mode),
                                     ON_UPDATE(handle_offline_mode));

static Sys_var_bool Sys_avoid_temporal_upgrade(
    "avoid_temporal_upgrade",
    "When this option is enabled, the pre-5.6.4 temporal types are "
    "not upgraded to the new format for ALTER TABLE requests "
    "ADD/CHANGE/MODIFY"
    " COLUMN, ADD INDEX or FORCE operation. "
    "This variable is deprecated and will be removed in a future release.",
    GLOBAL_VAR(avoid_temporal_upgrade),
    CMD_LINE(OPT_ARG, OPT_AVOID_TEMPORAL_UPGRADE), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_bool Sys_show_old_temporals(
    "show_old_temporals",
    "When this option is enabled, the pre-5.6.4 temporal types will "
    "be marked in the 'SHOW CREATE TABLE' and 'INFORMATION_SCHEMA.COLUMNS' "
    "table as a comment in COLUMN_TYPE field. "
    "This variable is deprecated and will be removed in a future release.",
    SESSION_VAR(show_old_temporals), CMD_LINE(OPT_ARG, OPT_SHOW_OLD_TEMPORALS),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin_no_super), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_charptr Sys_disabled_storage_engines(
    "disabled_storage_engines",
    "Limit CREATE TABLE for the storage engines listed",
    READ_ONLY GLOBAL_VAR(opt_disabled_storage_engines), CMD_LINE(REQUIRED_ARG),
    IN_SYSTEM_CHARSET, DEFAULT(""));

static Sys_var_bool Sys_persisted_globals_load(
    PERSISTED_GLOBALS_LOAD,
    "When this option is enabled, config file mysqld-auto.cnf is read "
    "and applied to server, else this file is ignored even if present.",
    READ_ONLY NON_PERSIST GLOBAL_VAR(persisted_globals_load), CMD_LINE(OPT_ARG),
    DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr));

static bool sysvar_check_authid_string(sys_var *, THD *thd, set_var *var) {
  /*
    Since mandatory_roles is similar to a GRANT role statement without a
    GRANT ADMIN privilege, setting this variable requires both the
    ROLE_ADMIN and the SYSTEM_VARIABLES_ADMIN.
  */
  Security_context *sctx = thd->security_context();
  assert(sctx != nullptr);
  if (sctx && !sctx->has_global_grant(STRING_WITH_LEN("ROLE_ADMIN")).first) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SYSTEM_VARIABLES_ADMIN or SUPER privileges, as well as the "
             "ROLE_ADMIN");
    /* No privilege access error */
    return true;
  }
  if (var->save_result.string_value.str == nullptr) {
    var->save_result.string_value.str = const_cast<char *>("");
    var->save_result.string_value.length = 0;
  }
  return check_authorization_id_string(thd, var->save_result.string_value);
}

static bool sysvar_update_mandatory_roles(sys_var *, THD *, enum_var_type) {
  update_mandatory_roles();
  return false;
}

static PolyLock_mutex PLock_sys_mandatory_roles(&LOCK_mandatory_roles);
static Sys_var_lexstring Sys_mandatory_roles(
    "mandatory_roles",
    "All the specified roles are always considered granted to every user and "
    "they"
    " can't be revoked. Mandatory roles still require activation unless they "
    "are made into "
    "default roles. The granted roles will not be visible in the "
    "mysql.role_edges"
    " table.",
    GLOBAL_VAR(opt_mandatory_roles), CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(""), &PLock_sys_mandatory_roles, NOT_IN_BINLOG,
    ON_CHECK(sysvar_check_authid_string),
    ON_UPDATE(sysvar_update_mandatory_roles));

static Sys_var_bool Sys_always_activate_granted_roles(
    "activate_all_roles_on_login",
    "Automatically set all granted roles as active after the user has "
    "authenticated successfully.",
    GLOBAL_VAR(opt_always_activate_granted_roles), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr));

static PolyLock_mutex plock_sys_password_history(&LOCK_password_history);
static Sys_var_uint Sys_password_history(
    "password_history",
    "The number of old passwords to check in the history."
    " Set to 0 (the default) to turn the checks off",
    GLOBAL_VAR(global_password_history), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX32), DEFAULT(0), BLOCK_SIZE(1),
    &plock_sys_password_history);

static PolyLock_mutex plock_sys_password_reuse_interval(
    &LOCK_password_reuse_interval);
static Sys_var_uint Sys_password_reuse_interval(
    "password_reuse_interval",
    "The minimum number of days that need to pass before a password can "
    "be reused. Set to 0 (the default) to turn the checks off",
    GLOBAL_VAR(global_password_reuse_interval), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(0, UINT_MAX32), DEFAULT(0), BLOCK_SIZE(1),
    &plock_sys_password_reuse_interval);

static bool check_resultset_metadata(sys_var *, THD *thd, set_var *var) {
  /*
    Set @@resultset_metadata to the value other than FULL only if
    the client supports it.
  */
  if (var->save_result.ulonglong_value != RESULTSET_METADATA_FULL &&
      !thd->get_protocol()->has_client_capability(
          CLIENT_OPTIONAL_RESULTSET_METADATA)) {
    my_error(ER_CLIENT_DOES_NOT_SUPPORT, MYF(0), "optional metadata transfer");
    return true;
  }
  return false;
}

static const char *resultset_metadata_names[] = {"NONE", "FULL", NullS};

static Sys_var_enum Sys_resultset_metadata(
    "resultset_metadata",
    "Controls what meatadata the server will send to the client: "
    "either FULL (default) for all metadata, NONE for no metadata.",
    SESSION_ONLY(resultset_metadata), NO_CMD_LINE, resultset_metadata_names,
    DEFAULT(static_cast<ulong>(RESULTSET_METADATA_FULL)), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_resultset_metadata), ON_UPDATE(nullptr));

static bool check_binlog_row_value_options(sys_var *self, THD *thd,
                                           set_var *var) {
  DBUG_TRACE;
  if (check_session_admin_outside_trx_outside_sf_outside_sp(self, thd, var))
    return true;
  if (var->save_result.ulonglong_value != 0) {
    const char *msg = nullptr;
    int code = ER_WARN_BINLOG_PARTIAL_UPDATES_DISABLED;
    if (!mysql_bin_log.is_open())
      msg = "the binary log is closed";
    else if (!var->is_global_persist()) {
      if (!thd->variables.sql_log_bin)
        msg = "the binary log is disabled";
      else if (thd->variables.binlog_format == BINLOG_FORMAT_STMT)
        msg = "binlog_format=STATEMENT";
      else if (log_bin_use_v1_row_events) {
        msg = "binlog_row_value_options=PARTIAL_JSON";
        code = ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED;
      } else if (thd->variables.binlog_row_image == BINLOG_ROW_IMAGE_FULL) {
        msg = "binlog_row_image=FULL";
        code = ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES;
      }
    } else {
      if (global_system_variables.binlog_format == BINLOG_FORMAT_STMT)
        msg = "binlog_format=STATEMENT";
      else if (log_bin_use_v1_row_events) {
        msg = "binlog_row_value_options=PARTIAL_JSON";
        code = ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED;
      } else if (global_system_variables.binlog_row_image ==
                 BINLOG_ROW_IMAGE_FULL) {
        msg = "binlog_row_image=FULL";
        code = ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES;
      }
    }
    if (msg) {
      switch (code) {
        case ER_WARN_BINLOG_PARTIAL_UPDATES_DISABLED:
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, code,
              ER_THD(thd, ER_WARN_BINLOG_PARTIAL_UPDATES_DISABLED), msg,
              "PARTIAL_JSON");
          break;
        case ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES:
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, code,
              ER_THD(thd,
                     ER_WARN_BINLOG_PARTIAL_UPDATES_SUGGESTS_PARTIAL_IMAGES),
              msg, "PARTIAL_JSON");
          break;
        case ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED:
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, code,
              ER_THD(thd, ER_WARN_BINLOG_V1_ROW_EVENTS_DISABLED), msg);
          break;
        default:
          assert(0); /* purecov: deadcode */
      }
    }
  }

  return false;
}

const char *binlog_row_value_options_names[] = {"PARTIAL_JSON", nullptr};
static Sys_var_set Sys_binlog_row_value_options(
    "binlog_row_value_options",
    "When set to PARTIAL_JSON, this option enables a space-efficient "
    "row-based binary log format for UPDATE statements that modify a "
    "JSON value using only the functions JSON_SET, JSON_REPLACE, and "
    "JSON_REMOVE. For such updates, only the modified parts of the "
    "JSON document are included in the binary log, so small changes of "
    "big documents may need significantly less space.",
    SESSION_VAR(binlog_row_value_options), CMD_LINE(REQUIRED_ARG),
    binlog_row_value_options_names, DEFAULT(0), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_binlog_row_value_options));

static bool check_keyring_access(sys_var *, THD *thd, set_var *) {
  if (!thd->security_context()->check_access(SUPER_ACL) &&
      !(thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("ENCRYPTION_KEY_ADMIN"))
            .first)) {
    my_error(ER_KEYRING_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or ENCRYPTION_KEY_ADMIN");
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
static Sys_var_bool Sys_keyring_operations(
    "keyring_operations",
    "This variable provides access to keyring service APIs. When this "
    "option is disabled calls to keyring_key_generate(), keyring_key_store() "
    "and keyring_key_remove() will report error until this variable is "
    "enabled.",
    NON_PERSIST GLOBAL_VAR(opt_keyring_operations), NO_CMD_LINE, DEFAULT(true),
    &PLock_keyring_operations, NOT_IN_BINLOG, ON_CHECK(check_keyring_access),
    ON_UPDATE(nullptr));

static bool check_default_collation_for_utf8mb4(sys_var *self, THD *thd,
                                                set_var *var) {
  if (check_collation_not_null(self, thd, var)) {
    return true;
  }

  if (!var->value)
    var->save_result.ptr = reinterpret_cast<void *>(self->get_default());

  auto cs = static_cast<const CHARSET_INFO *>(var->save_result.ptr);
  if (cs == &my_charset_utf8mb4_0900_ai_ci ||
      cs == &my_charset_utf8mb4_general_ci)
    return false;

  my_error(ER_INVALID_DEFAULT_UTF8MB4_COLLATION, MYF(0), cs->m_coll_name);
  return true;
}

static Sys_var_struct<CHARSET_INFO, Get_name> Sys_default_collation_for_utf8mb4(
    "default_collation_for_utf8mb4",
    "Controls default collation for utf8mb4 while replicating implicit "
    "utf8mb4 collations.",
    SESSION_VAR(default_collation_for_utf8mb4), NO_CMD_LINE,
    DEFAULT(&my_charset_utf8mb4_0900_ai_ci), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_default_collation_for_utf8mb4),
    ON_UPDATE(update_deprecated));

static Sys_var_bool Sys_show_create_table_verbosity(
    "show_create_table_verbosity",
    "When this option is enabled, it increases the verbosity of "
    "'SHOW CREATE TABLE'.",
    SESSION_VAR(show_create_table_verbosity), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr));

static const char *use_secondary_engine_values[] = {"OFF", "ON", "FORCED",
                                                    nullptr};
static Sys_var_enum Sys_use_secondary_engine(
    "use_secondary_engine",
    "Controls preparation of SELECT statements against secondary storage "
    "engine. Valid values: OFF/ON/FORCED. OFF = Prepare only against primary "
    "storage engine. ON = First prepare against secondary storage engine, "
    "reprepare against primary storage engine if error. FORCED = Prepare all "
    "SELECT statements referencing one or more base tables only against "
    "secondary storage engine.",
    HINT_UPDATEABLE SESSION_ONLY(use_secondary_engine), NO_CMD_LINE,
    use_secondary_engine_values, DEFAULT(SECONDARY_ENGINE_ON), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr));

static Sys_var_session_special Sys_statement_id(
    "statement_id",
    "statement_id: represents the id of the query "
    "When this option is enabled it returns the statement id to the client, "
    "the client can find more data about this query from the performance schema"
    "(such as: events_statements_history table, rpd_query_stats table etc)  by "
    "searching for a specific statement_id value.",
    READ_ONLY sys_var::ONLY_SESSION, NO_CMD_LINE, VALID_RANGE(0, INT_MAX64),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr), ON_READ(read_statement_id));

/**
  Cost threshold for executing queries in a secondary storage engine. Only
  queries that have an estimated cost above this value will be attempted
  executed in a secondary storage engine.

  Secondary storage engines are meant to accelerate queries that would otherwise
  take a relatively long time to execute. If a secondary storage engine accepts
  a query, it is assumed that it will be able to accelerate it. However, if the
  estimated cost of the query is low, the query will execute fast in the primary
  engine too, so there is little to gain by offloading the query to the
  secondary engine.

  The default value aims to avoid use of secondary storage engines for queries
  that could be executed by the primary engine in a few tenths of seconds or
  less, and attempt to use secondary storage engines for queries would take
  seconds or more.
*/
static Sys_var_double Sys_secondary_engine_cost_threshold(
    "secondary_engine_cost_threshold",
    "Controls which statements to consider for execution in a secondary "
    "storage engine. Only statements that have a cost estimate higher than "
    "this value will be attempted executed in a secondary storage engine.",
    HINT_UPDATEABLE SESSION_VAR(secondary_engine_cost_threshold),
    CMD_LINE(OPT_ARG), VALID_RANGE(0, DBL_MAX), DEFAULT(100000), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr));

static Sys_var_bool Sys_sql_require_primary_key{
    "sql_require_primary_key",
    "When set, tables must be created with a primary key, and an existing "
    "primary key cannot be removed with 'ALTER TABLE'. Attempts to do so "
    "will result in an error.",
    HINT_UPDATEABLE SESSION_VAR(sql_require_primary_key),
    CMD_LINE(OPT_ARG),
    DEFAULT(false),
    NO_MUTEX_GUARD,
    IN_BINLOG,
    ON_CHECK(check_session_admin)};

static Sys_var_bool Sys_sql_generate_invisible_primary_key(
    "sql_generate_invisible_primary_key",
    "When set, if a table is created without a primary key then server "
    "generates invisible auto-increment column as a primary key for the table.",
    SESSION_VAR(sql_generate_invisible_primary_key), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_session_admin), ON_UPDATE(nullptr));

static Sys_var_bool Sys_show_gipk_in_create_table_and_information_schema(
    "show_gipk_in_create_table_and_information_schema",
    "When set, if a primary key is generated for a table then SHOW commands "
    "and INFORMATION_SCHEMA tables shows generated invisible primary key "
    "definition.",
    SESSION_VAR(show_gipk_in_create_table_and_information_schema),
    CMD_LINE(OPT_ARG), DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr));

static Sys_var_charptr Sys_sys_variables_admin_subject(
    PERSIST_ONLY_ADMIN_X509_SUBJECT,
    "The client peer certificate name required to enable setting all "
    "system variables via SET PERSIST[_ONLY]",
    READ_ONLY NON_PERSIST GLOBAL_VAR(sys_var_persist_only_admin_x509_subject),
    CMD_LINE(OPT_ARG), IN_SYSTEM_CHARSET, DEFAULT(""));

static Sys_var_ulong Sys_binlog_row_event_max_size(
    "binlog_row_event_max_size",
    "The maximum size of a row-based binary log event in bytes. Rows will be "
    "grouped into events smaller than this size if possible. "
    "The value has to be a multiple of 256.",
    READ_ONLY GLOBAL_VAR(binlog_row_event_max_size), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(256, ULONG_MAX), DEFAULT(8192), BLOCK_SIZE(256));

static bool check_group_replication_consistency(sys_var *self, THD *thd,
                                                set_var *var) {
  if (var->type == OPT_GLOBAL || var->type == OPT_PERSIST) {
    Security_context *sctx = thd->security_context();
    if (!sctx->check_access(SUPER_ACL) &&
        !sctx->has_global_grant(STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"))
             .first) {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
               "SUPER or GROUP_REPLICATION_ADMIN");
      return true;
    }
  }

  return check_outside_trx(self, thd, var);
}

static const char *group_replication_consistency_names[] = {
    "EVENTUAL", "BEFORE_ON_PRIMARY_FAILOVER", "BEFORE",
    "AFTER",    "BEFORE_AND_AFTER",           NullS};

static Sys_var_enum Sys_group_replication_consistency(
    "group_replication_consistency",
    "Transaction consistency guarantee, possible values: EVENTUAL, "
    "BEFORE_ON_PRIMARY_FAILOVER, BEFORE, AFTER, BEFORE_AND_AFTER",
    SESSION_VAR(group_replication_consistency), CMD_LINE(OPT_ARG),
    group_replication_consistency_names,
    DEFAULT(GROUP_REPLICATION_CONSISTENCY_EVENTUAL), NO_MUTEX_GUARD,
    NOT_IN_BINLOG, ON_CHECK(check_group_replication_consistency),
    ON_UPDATE(nullptr));

static bool check_binlog_encryption_admin(sys_var *, THD *thd, set_var *) {
  DBUG_TRACE;
  if (!thd->security_context()->check_access(SUPER_ACL) &&
      !(thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("BINLOG_ENCRYPTION_ADMIN"))
            .first)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or BINLOG_ENCRYPTION_ADMIN");
    return true;
  }
  return false;
}

bool Sys_var_binlog_encryption::global_update(THD *thd, set_var *var) {
  DBUG_TRACE;

  /* No-op if trying to set to current value */
  bool new_value = var->save_result.ulonglong_value;
  if (new_value == rpl_encryption.is_enabled()) return false;

  DEBUG_SYNC(thd, "after_locking_global_sys_var_set_binlog_enc");
  /* We unlock in following statement to avoid deadlock involving following
   * conditions.
   * ------------------------------------------------------------------------
   * Thread 1 (START SLAVE)  has locked channel_map and waiting for cond_wait
   * that is supposed to be done by Thread 2.
   *
   * Thread 2 (handle_slave_io) is supposed to signal Thread 1 but waiting to
   * lock LOCK_global_system_variables.
   *
   * Thread 3 (SET GLOBAL binlog_encryption=ON|OFF) has locked
   * LOCK_global_system_variables and waiting for channel_map.
   */
  mysql_mutex_unlock(&LOCK_global_system_variables);
  /* Set the option new value */
  bool res = false;
  if (new_value)
    res = rpl_encryption.enable(thd);
  else
    rpl_encryption.disable(thd);
  mysql_mutex_lock(&LOCK_global_system_variables);
  return res;
}

static Sys_var_binlog_encryption Sys_binlog_encryption(
    "binlog_encryption", "Enable/disable binary and relay logs encryption.",
    GLOBAL_VAR(rpl_encryption.get_enabled_var()), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_binlog_encryption_admin));

static Sys_var_bool Sys_binlog_rotate_encryption_master_key_at_startup(
    "binlog_rotate_encryption_master_key_at_startup",
    "Force binlog encryption master key rotation at startup",
    READ_ONLY GLOBAL_VAR(
        rpl_encryption.get_master_key_rotation_at_startup_var()),
    CMD_LINE(OPT_ARG), DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG);

static Sys_var_uint Sys_original_server_version(
    "original_server_version",
    "The version of the server where the transaction was originally executed",
    SESSION_ONLY(original_server_version), NO_CMD_LINE,
    VALID_RANGE(0, UNDEFINED_SERVER_VERSION), DEFAULT(UNDEFINED_SERVER_VERSION),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_session_admin_or_replication_applier));

static Sys_var_uint Sys_immediate_server_version(
    "immediate_server_version",
    "The server version of the immediate server in the replication topology",
    SESSION_ONLY(immediate_server_version), NO_CMD_LINE,
    VALID_RANGE(0, UNDEFINED_SERVER_VERSION), DEFAULT(UNDEFINED_SERVER_VERSION),
    BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_session_admin_or_replication_applier));

static bool check_set_default_table_encryption_access(sys_var *self
                                                      [[maybe_unused]],
                                                      THD *thd, set_var *var) {
  DBUG_EXECUTE_IF("skip_table_encryption_admin_check_for_set",
                  { return false; });
  if ((var->type == OPT_GLOBAL || var->type == OPT_PERSIST) &&
      is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The default_table_encryption option cannot be changed when "
               "Group replication is running.",
               MYF(0));
    return true;
  }

  // Should own one of SUPER or both (SYSTEM_VARIABLES_ADMIN and
  // TABLE_ENCRYPTION_ADMIN), unless this is the session option and
  // the value is unchanged.
  longlong previous_val = thd->variables.default_table_encryption;
  longlong val = (longlong)var->save_result.ulonglong_value;
  if ((!var->is_global_persist() && val == previous_val) ||
      thd->security_context()->check_access(SUPER_ACL) ||
      (thd->security_context()
           ->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
           .first &&
       thd->security_context()
           ->has_global_grant(STRING_WITH_LEN("TABLE_ENCRYPTION_ADMIN"))
           .first)) {
    return false;
  }

  my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
           "SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN");
  return true;
}

static Sys_var_bool Sys_default_table_encryption(
    "default_table_encryption",
    "Database and tablespace are created with this default encryption property "
    "unless the user specifies an explicit encryption property.",
    HINT_UPDATEABLE SESSION_VAR(default_table_encryption), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_set_default_table_encryption_access), ON_UPDATE(nullptr));

static bool check_set_table_encryption_privilege_access(sys_var *, THD *thd,
                                                        set_var *) {
  DBUG_EXECUTE_IF("skip_table_encryption_admin_check_for_set",
                  { return false; });
  if (!thd->security_context()->check_access(SUPER_ACL)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return true;
  }
  return false;
}

static Sys_var_bool Sys_table_encryption_privilege_check(
    "table_encryption_privilege_check",
    "Indicates if server enables privilege check when user tries to use "
    "non-default value for CREATE DATABASE or CREATE TABLESPACE or when "
    "user tries to do CREATE TABLE with ENCRYPTION option which deviates "
    "from per-database default.",
    GLOBAL_VAR(opt_table_encryption_privilege_check), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_set_table_encryption_privilege_access), ON_UPDATE(nullptr));

static Sys_var_bool Sys_var_print_identified_with_as_hex(
    "print_identified_with_as_hex",
    "SHOW CREATE USER will print the AS clause as HEX if it contains "
    "non-prinable characters",
    SESSION_VAR(print_identified_with_as_hex), CMD_LINE(OPT_ARG),
    DEFAULT(false));

/**
   Session only flag to skip printing secondary engine in SHOW CREATE
   TABLE.

   @sa store_create_info
*/
static Sys_var_bool Sys_var_show_create_table_skip_secondary_engine(
    "show_create_table_skip_secondary_engine",
    "SHOW CREATE TABLE will skip SECONDARY_ENGINE when printing the table "
    "definition",
    SESSION_ONLY(show_create_table_skip_secondary_engine), NO_CMD_LINE,
    DEFAULT(false));

static Sys_var_uint Sys_generated_random_password_length(
    "generated_random_password_length",
    "Determines the length randomly generated passwords in CREATE USER-,"
    "SET PASSWORD- or ALTER USER statements",
    SESSION_VAR(generated_random_password_length), CMD_LINE(REQUIRED_ARG),
    VALID_RANGE(5, 255), DEFAULT(20), BLOCK_SIZE(1), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(nullptr));

static bool check_set_protocol_compression_algorithms(sys_var *, THD *,
                                                      set_var *var) {
  if (!(var->save_result.string_value.str)) return true;
  return validate_compression_attributes(var->save_result.string_value.str,
                                         std::string(), true);
}

static Sys_var_charptr Sys_protocol_compression_algorithms(
    "protocol_compression_algorithms",
    "List of compression algorithms supported by server. Supported values "
    "are any combination of zlib, zstd, uncompressed. Command line clients "
    "may use the --compression-algorithms flag to specify a set of algorithms, "
    "and the connection will use an algorithm supported by both client and "
    "server. It picks zlib if both client and server support it; otherwise it "
    "picks zstd if both support it; otherwise it picks uncompressed if both "
    "support it; otherwise it fails.",
    GLOBAL_VAR(opt_protocol_compression_algorithms), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET,
    DEFAULT(const_cast<char *>(PROTOCOL_COMPRESSION_DEFAULT_VALUE)),
    NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(check_set_protocol_compression_algorithms), ON_UPDATE(nullptr));

static bool check_set_require_row_format(sys_var *, THD *thd, set_var *var) {
  /*
   Should own SUPER or SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN
   when the value is changing to NO, no privileges are needed to set to YES
  */
  longlong previous_val = thd->variables.require_row_format;
  longlong val = (longlong)var->save_result.ulonglong_value;
  assert(!var->is_global_persist());

  // if it was true and we are changing it
  if (previous_val && val != previous_val) {
    if (thd->security_context()->check_access(SUPER_ACL) ||
        thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
            .first ||
        thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("SESSION_VARIABLES_ADMIN"))
            .first)
      return false;

    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN");
    return true;
  }
  return false;
}

/**
   Session only flag to limit the application of queries to row based events
   and DDLs with the exception of temporary table creation/deletion
*/
static Sys_var_bool Sys_var_require_row_format(
    "require_row_format",
    "Limit the application of queries to row based events "
    "and DDLs with the exception of temporary table creation/deletion.",
    SESSION_ONLY(require_row_format), NO_CMD_LINE, DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_set_require_row_format));

/**
  Changes the `Delegate` internal state in regards to which type of lock to
  use and in regards to whether or not to take plugin locks in each hook
  invocation.
*/
static bool handle_plugin_lock_type_change(sys_var *, THD *, enum_var_type) {
  DBUG_TRACE;
  delegates_acquire_locks();
  delegates_update_lock_type();
  delegates_release_locks();
  return false;
}

static Sys_var_bool Sys_replication_optimize_for_static_plugin_config(
    "replication_optimize_for_static_plugin_config",
    "Optional flag that blocks plugin install/uninstall and allows skipping "
    "the acquisition of the lock to read from the plugin list and the usage "
    "of read-optimized spin-locks. Use only when plugin hook callback needs "
    "optimization (a lot of semi-sync replicas, for instance).",
    GLOBAL_VAR(opt_replication_optimize_for_static_plugin_config),
    CMD_LINE(OPT_ARG), DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(handle_plugin_lock_type_change));

static Sys_var_bool Sys_replication_sender_observe_commit_only(
    "replication_sender_observe_commit_only",
    "Optional flag that allows for only calling back observer hooks at "
    "commit.",
    GLOBAL_VAR(opt_replication_sender_observe_commit_only), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr));

static Sys_var_bool Sys_skip_replica_start(
    "skip_replica_start",
    "Do not start replication threads automatically "
    "when the server starts.",
    READ_ONLY GLOBAL_VAR(opt_skip_replica_start), CMD_LINE(OPT_ARG),
    DEFAULT(false), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(nullptr));

static bool check_authentication_policy(sys_var *, THD *, set_var *var) {
  if (!(var->save_result.string_value.str)) return true;
  return validate_authentication_policy(var->save_result.string_value.str);
}

static bool fix_authentication_policy(sys_var *, THD *, enum_var_type) {
  DBUG_TRACE;
  update_authentication_policy();
  return false;
}
/**
  This is a mutex used to protect @@global.authentication_policy variable.
*/
static PolyLock_mutex PLock_authentication_policy(&LOCK_authentication_policy);
/*
  when authentication_policy = 'mysql_native_password,,' and
  --default-authentication-plugin = 'caching_sha2_password'
  set default as mysql_native_password.
  --authentication_policy has precedence over --default-authentication-plugin
  with 1 exception as below: when authentication_policy = '*,,' and
  --default-authentication-plugin = 'mysql_native_password'
  set default as mysql_native_password
  in case no concrete plugin can be extracted from --authentication_policy
  for first factor, server picks plugin name from
  --default-authentication-plugin
*/
static Sys_var_charptr Sys_authentication_policy(
    "authentication_policy",
    "Defines policies around how user account can be configured with Multi "
    "Factor authentication methods during CREATE/ALTER USER statement. "
    "This variable accepts at-most 3 comma separated list of authentication "
    "plugin names where each value refers to what authentication plugin should "
    "be used in place of 1st Factor Authentication (FA), 2FA and 3FA method. "
    "Value * indicates any plugin is allowed for 1FA, 2FA and 3FA method. "
    "An empty value means nth FA method is optional.",
    GLOBAL_VAR(opt_authentication_policy), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT("*,,"), &PLock_authentication_policy, NOT_IN_BINLOG,
    ON_CHECK(check_authentication_policy),
    ON_UPDATE(fix_authentication_policy));

static Sys_var_deprecated_alias Sys_skip_slave_start("skip_slave_start",
                                                     Sys_skip_replica_start);

static const char *terminology_use_previous_names[] = {"NONE", "BEFORE_8_0_26",
                                                       nullptr};

static Sys_var_enum Sys_terminology_use_previous(
    "terminology_use_previous",
    "Make monitoring tables and statements use the identifiers that were "
    "in use before they were changed in a given release. That includes names "
    "for mutexes, read/write locks, condition variables, memory allocations, "
    "thread names, thread stages, and thread commands. When the session "
    "option is set to BEFORE_8_0_26, the session uses the names that were in "
    "use until 8.0.25, when it selects from performance_schema tables, or "
    "selects from INFORMATION_SCHEMA.PROCESSLIST, or issues SHOW PROCESSLIST "
    "or SHOW REPLICA STATUS. When the global option is set to BEFORE_8_0_26, "
    "new sessions use BEFORE_8_0_26 as default for the session option, and in "
    "addition the thread commands that were in use until 8.0.25 are written "
    "to the slow query log.",
    SESSION_VAR(terminology_use_previous), CMD_LINE(REQUIRED_ARG),
    terminology_use_previous_names, DEFAULT(terminology_use_previous::NONE),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr),
    DEPRECATED_VAR(""));

static Sys_var_bool Sys_xa_detatch_on_prepare(
    "xa_detach_on_prepare",
    "When set, XA transactions will be detached (AKA dissociated or "
    "disconnected) from connection as part of XA PREPARE. This means that "
    "the XA transaction can be committed/rolled back by any connection, "
    "even if the starting connection has not terminated, and the starting "
    "connection can start new transactions. As a side effect, temporary "
    "tables cannot be used inside XA transactions. "
    "When disabled, XA transactions are associated with the same connection "
    "until the session disconnects. ON is the only safe choice for "
    "replication.",
    HINT_UPDATEABLE SESSION_VAR(xa_detach_on_prepare), CMD_LINE(OPT_ARG),
    DEFAULT(true), NO_MUTEX_GUARD, IN_BINLOG,
    ON_CHECK(check_session_admin_outside_trx_outside_sf));

#ifndef NDEBUG
static Sys_var_charptr Sys_debug_sensitive_session_string(
    "debug_sensitive_session_string",
    "Debug variable to test sensitive session string variable.",
    SENSITIVE SESSION_VAR(debug_sensitive_session_str), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(""));
#endif /* NDEBUG */

static Sys_var_bool Sys_persist_sensitive_variables_in_plaintext(
    "persist_sensitive_variables_in_plaintext",
    "If set to FALSE, server will refuse to persist SENSITIVE variables in "
    "plaintext and refuse to start if encrypted part of persited file cannot "
    "be processed.",
    READ_ONLY NON_PERSIST
        GLOBAL_VAR(opt_persist_sensitive_variables_in_plaintext),
    CMD_LINE(OPT_ARG), DEFAULT(true), NO_MUTEX_GUARD, NOT_IN_BINLOG,
    ON_CHECK(nullptr), ON_UPDATE(nullptr), nullptr, sys_var::PARSE_EARLY);

static const char *explain_format_names[] = {
    "TRADITIONAL", "TRADITIONAL_STRICT", "TREE", "JSON", NullS};
static Sys_var_enum Sys_explain_format(
    "explain_format",
    "The default format in which the EXPLAIN statement displays information. "
    "Valid values are TRADITIONAL (default), TREE, JSON and TRADITIONAL_STRICT."
    " TRADITIONAL_STRICT is only used internally by the mtr test suite, and is "
    "not meant to be used anywhere else.",
    SESSION_VAR(explain_format), CMD_LINE(OPT_ARG), explain_format_names,
    DEFAULT(static_cast<ulong>(Explain_format_type::TRADITIONAL)),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr));

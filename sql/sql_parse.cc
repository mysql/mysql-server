/* Copyright (c) 1999, 2023, Oracle and/or its affiliates.

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

#include "sql/sql_parse.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "my_config.h"
#ifdef HAVE_LSAN_DO_RECOVERABLE_LEAK_CHECK
#include <sanitizer/lsan_interface.h>
#endif

#include "dur_prop.h"
#include "field_types.h"  // enum_field_types
#include "m_ctype.h"
#include "m_string.h"
#include "mem_root_deque.h"
#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_alloc.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_hostname.h"
#include "my_inttypes.h"  // TODO: replace with cstdint
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "my_thread_local.h"
#include "my_time.h"
#include "mysql/com_data.h"
#include "mysql/components/services/bits/plugin_audit_connection_types.h"  // MYSQL_AUDIT_CONNECTION_CHANGE_USER
#include "mysql/components/services/bits/psi_statement_bits.h"  // PSI_statement_info
#include "mysql/components/services/log_builtins.h"             // LogErr
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/udf_registration_types.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "mysys_err.h"  // EE_CAPACITY_EXCEEDED
#include "pfs_thread_provider.h"
#include "prealloced_array.h"
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h"  // purge_source_logs
#include "sql/clone_handler.h"
#include "sql/comp_creator.h"
#include "sql/create_field.h"
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client::Auto_releaser
#include "sql/dd/dd.h"                       // dd::get_dictionary
#include "sql/dd/dd_schema.h"                // Schema_MDL_locker
#include "sql/dd/dictionary.h"  // dd::Dictionary::is_system_view_name
#include "sql/dd/info_schema/table_stats.h"
#include "sql/dd/types/column.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/derror.h"      // ER_THD
#include "sql/discrete_interval.h"
#include "sql/error_handler.h"  // Strict_error_handler
#include "sql/events.h"         // Events
#include "sql/field.h"
#include "sql/gis/srid.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_subselect.h"
#include "sql/item_timefunc.h"  // Item_func_unix_timestamp
#include "sql/key_spec.h"       // Key_spec
#include "sql/locked_tables_list.h"
#include "sql/log.h"        // query_logger
#include "sql/log_event.h"  // slave_execute_deferred_events
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"              // stage_execution_of_init_command
#include "sql/mysqld_thd_manager.h"  // Find_thd_with_id
#include "sql/nested_join.h"
#include "sql/opt_hints.h"
#include "sql/opt_trace.h"  // Opt_trace_start
#include "sql/parse_location.h"
#include "sql/parse_tree_node_base.h"
#include "sql/parse_tree_nodes.h"
#include "sql/parser_yystype.h"
#include "sql/persisted_variable.h"
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/resourcegroups/resource_group_basic_types.h"
#include "sql/resourcegroups/resource_group_mgr.h"  // Resource_group_mgr::instance
#include "sql/rpl_context.h"
#include "sql/rpl_filter.h"             // rpl_filter
#include "sql/rpl_group_replication.h"  // group_replication_start
#include "sql/rpl_gtid.h"
#include "sql/rpl_handler.h"  // launch_hook_trans_begin
#include "sql/rpl_replica.h"  // change_master_cmd
#include "sql/rpl_source.h"   // register_slave
#include "sql/rpl_utility.h"
#include "sql/session_tracker.h"
#include "sql/set_var.h"
#include "sql/sp.h"        // sp_create_routine
#include "sql/sp_cache.h"  // sp_cache_enforce_limit
#include "sql/sp_head.h"   // sp_head
#include "sql/sql_admin.h"
#include "sql/sql_alter.h"
#include "sql/sql_audit.h"   // MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER
#include "sql/sql_base.h"    // find_temporary_table
#include "sql/sql_binlog.h"  // mysql_client_binlog_statement
#include "sql/sql_check_constraint.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_connect.h"  // decrease_user_connections
#include "sql/sql_const.h"
#include "sql/sql_db.h"  // mysql_change_db
#include "sql/sql_digest.h"
#include "sql/sql_digest_stream.h"
#include "sql/sql_error.h"
#include "sql/sql_handler.h"  // mysql_ha_rm_tables
#include "sql/sql_help.h"     // mysqld_help
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_prepare.h"  // mysql_stmt_execute
#include "sql/sql_profile.h"
#include "sql/sql_query_rewrite.h"  // invoke_pre_parse_rewrite_plugins
#include "sql/sql_reload.h"         // handle_reload_request
#include "sql/sql_rename.h"         // mysql_rename_tables
#include "sql/sql_rewrite.h"        // mysql_rewrite_query
#include "sql/sql_show.h"           // find_schema_table
#include "sql/sql_table.h"          // mysql_create_table
#include "sql/sql_trigger.h"        // add_table_for_trigger
#include "sql/sql_udf.h"
#include "sql/sql_view.h"  // mysql_create_view
#include "sql/strfunc.h"
#include "sql/system_variables.h"  // System_status_var
#include "sql/table.h"
#include "sql/table_cache.h"  // table_cache_manager
#include "sql/thd_raii.h"
#include "sql/transaction.h"  // trans_rollback_implicit
#include "sql/transaction_info.h"
#include "sql_string.h"
#include "template_utils.h"
#include "thr_lock.h"
#include "violite.h"

#ifdef WITH_LOCK_ORDER
#include "sql/debug_lock_order.h"
#endif /* WITH_LOCK_ORDER */

namespace resourcegroups {
class Resource_group;
}  // namespace resourcegroups
struct mysql_rwlock_t;

namespace dd {
class Schema;
}  // namespace dd

namespace dd {
class Abstract_table;
}  // namespace dd

using std::max;

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

/* Used in error handling only */
#define SP_COM_STRING(LP)                                  \
  ((LP)->sql_command == SQLCOM_CREATE_SPFUNCTION ||        \
           (LP)->sql_command == SQLCOM_ALTER_FUNCTION ||   \
           (LP)->sql_command == SQLCOM_SHOW_CREATE_FUNC || \
           (LP)->sql_command == SQLCOM_DROP_FUNCTION       \
       ? "FUNCTION"                                        \
       : "PROCEDURE")

static void sql_kill(THD *thd, my_thread_id id, bool only_kill_query);

const std::string Command_names::m_names[] = {
    "Sleep",
    "Quit",
    "Init DB",
    "Query",
    "Field List",
    "Create DB",
    "Drop DB",
    "Refresh",
    "Shutdown",
    "Statistics",
    "Processlist",
    "Connect",
    "Kill",
    "Debug",
    "Ping",
    "Time",
    "Delayed insert",
    "Change user",
    "Binlog Dump",
    "Table Dump",
    "Connect Out",
    "Register Replica",
    "Prepare",
    "Execute",
    "Long Data",
    "Close stmt",
    "Reset stmt",
    "Set option",
    "Fetch",
    "Daemon",
    "Binlog Dump GTID",
    "Reset Connection",
    "clone",
    "Group Replication Data Stream subscription",
    "Error"  // Last command number
};

const std::string &Command_names::translate(const System_variables &sysvars) {
  terminology_use_previous::enum_compatibility_version version =
      static_cast<terminology_use_previous::enum_compatibility_version>(
          sysvars.terminology_use_previous);
  if (version != terminology_use_previous::NONE && version <= m_replace_version)
    return m_replace_str;
  return m_names[m_replace_com];
}

const std::string &Command_names::str_session(enum_server_command cmd) {
  assert(current_thd);
  if (cmd != m_replace_com || current_thd == nullptr) return m_names[cmd];
  return translate(current_thd->variables);
}

const std::string &Command_names::str_global(enum_server_command cmd) {
  if (cmd != m_replace_com) return m_names[cmd];
  return translate(global_system_variables);
}

const std::string Command_names::m_replace_str{"Register Slave"};

bool command_satisfy_acl_cache_requirement(unsigned command) {
  return !((sql_command_flags[command] & CF_REQUIRE_ACL_CACHE) > 0 &&
           skip_grant_tables());
}

/**
  Returns true if all tables should be ignored.
*/
bool all_tables_not_ok(THD *thd, Table_ref *tables) {
  Rpl_filter *rpl_filter = thd->rli_slave->rpl_filter;

  return rpl_filter->is_on() && tables && !thd->sp_runtime_ctx &&
         !rpl_filter->tables_ok(thd->db().str, tables);
}

bool is_normal_transaction_boundary_stmt(enum_sql_command sql_cmd) {
  switch (sql_cmd) {
    case SQLCOM_BEGIN:
    case SQLCOM_COMMIT:
    case SQLCOM_SAVEPOINT:
    case SQLCOM_ROLLBACK:
    case SQLCOM_ROLLBACK_TO_SAVEPOINT:
      return true;
    default:
      return false;
  }

  return false;
}

bool is_xa_transaction_boundary_stmt(enum_sql_command sql_cmd) {
  switch (sql_cmd) {
    case SQLCOM_XA_START:
    case SQLCOM_XA_END:
    case SQLCOM_XA_COMMIT:
    case SQLCOM_XA_ROLLBACK:
      return true;
    default:
      return false;
  }

  return false;
}

/**
  Checks whether the event for the given database, db, should
  be ignored or not. This is done by checking whether there are
  active rules in ignore_db or in do_db containers. If there
  are, then check if there is a match, if not then check the
  wild_do rules.

  NOTE: This means that when using this function replicate-do-db
        and replicate-ignore-db take precedence over wild do
        rules.

  @param thd  Thread handle.
  @param db   Database name used while evaluating the filtering
              rules.
  @param sql_cmd Represents the current query that needs to be
                 verified against the database filter rules.
  @return true Query should not be filtered out from the execution.
          false Query should be filtered out from the execution.

*/
inline bool check_database_filters(THD *thd, const char *db,
                                   enum_sql_command sql_cmd) {
  DBUG_TRACE;
  assert(thd->slave_thread);
  if (!db || is_normal_transaction_boundary_stmt(sql_cmd) ||
      is_xa_transaction_boundary_stmt(sql_cmd))
    return true;

  Rpl_filter *rpl_filter = thd->rli_slave->rpl_filter;
  auto db_ok{rpl_filter->db_ok(db)};
  /*
    No filters exist in ignore/do_db ? Then, just check
    wild_do_table filtering for 'DATABASE' related
    statements (CREATE/DROP/ATLER DATABASE)
  */
  if (db_ok && (rpl_filter->get_do_db()->is_empty() &&
                rpl_filter->get_ignore_db()->is_empty())) {
    switch (sql_cmd) {
      case SQLCOM_CREATE_DB:
      case SQLCOM_ALTER_DB:
      case SQLCOM_DROP_DB:
        db_ok = rpl_filter->db_ok_with_wild_table(db);
      default:
        break;
    }
  }
  return db_ok;
}

bool some_non_temp_table_to_be_updated(THD *thd, Table_ref *tables) {
  for (Table_ref *table = tables; table; table = table->next_global) {
    assert(table->db && table->table_name);
    /*
      Update on performance_schema and temp tables are allowed
      in readonly mode.
    */
    if (table->updating && !find_temporary_table(thd, table) &&
        !is_perfschema_db(table->db, table->db_length))
      return true;
  }
  return false;
}

/**
  Returns whether the command in thd->lex->sql_command should cause an
  implicit commit. An active transaction should be implicitly committed if the
  statement requires so.

  @param thd    Thread handle.
  @param mask   Bitmask used for the SQL command match.

  @retval true This statement shall cause an implicit commit.
  @retval false This statement shall not cause an implicit commit.
*/
bool stmt_causes_implicit_commit(const THD *thd, uint mask) {
  DBUG_TRACE;
  const LEX *lex = thd->lex;

  if ((sql_command_flags[lex->sql_command] & mask) == 0 ||
      thd->is_plugin_fake_ddl())
    return false;

  switch (lex->sql_command) {
    case SQLCOM_DROP_TABLE:
      return !lex->drop_temporary;
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_CREATE_TABLE:
      /* If CREATE TABLE of non-temporary table or without
        START TRANSACTION, do implicit commit */
      return (lex->create_info->options & HA_LEX_CREATE_TMP_TABLE ||
              lex->create_info->m_transactional_ddl) == 0;
    case SQLCOM_SET_OPTION:
      /* Implicitly commit a transaction started by a SET statement */
      return lex->autocommit;
    case SQLCOM_RESET:
      return lex->option_type != OPT_PERSIST;
    case SQLCOM_STOP_GROUP_REPLICATION:
      return lex->was_replication_command_executed();
    default:
      return true;
  }
}

/**
  @brief Iterates over all post replication filter actions registered and
  executes them.

  All actions registered will be executed at most once. They are executed in
  the order that they were registered. Shall there be an error while iterating
  through the list of actions and executing them, then the process stops and an
  error is returned immediately. This means that in that case, some actions may
  have executed successfully and some not. In other words, this procedure is
  not atomic.

  This function will consume all actions from the list if there is no error.
  This means that actions run only once per statement. Should there be any
  sub-statements then actions only run on the top level statement execution.

  @param thd The thread context.
  @return true If there was an error while executing the registered actions.
  @return false If all actions executed successfully.
*/
static bool run_post_replication_filters_actions(THD *thd) {
  DBUG_TRACE;
  auto &actions{thd->rpl_thd_ctx.post_filters_actions()};
  for (auto &action : actions) {
    if (action()) return true;
  }
  actions.clear();
  return false;
}

/**
  @brief This function determines if the current statement parsed violates the
  require_row_format check.

  Given a parsed context within the THD object, this function will infer
  whether the require row format check is violated or not. If it is, this
  function returns true, false otherwise. Note that this function can be called
  from both, normal sessions and replication applier, execution paths.

  @param thd The session context holding the parsed statement.
  @return true if there was a require row format validation failure.
  @return false if the check was successful, meaning no require row format
  validation failure.
*/
static bool check_and_report_require_row_format_violation(THD *thd) {
  DBUG_TRACE;
  assert(thd != nullptr);
  auto perform_check{thd->slave_thread
                         ? thd->rli_slave->is_row_format_required()
                         : thd->variables.require_row_format};
  if (!perform_check) return false;

  if (is_require_row_format_violation(thd)) {
    if (thd->slave_thread) thd->is_slave_error = true;
    my_error(ER_CLIENT_QUERY_FAILURE_INVALID_NON_ROW_FORMAT, MYF(0));
    return true;
  }
  return false;
}

/**
  Mark all commands that somehow changes a table.

  This is used to check number of updates / hour.

  sql_command is actually set to SQLCOM_END sometimes
  so we need the +1 to include it in the array.

  See COMMAND_FLAG_xxx for different type of commands
     2  - query that returns meaningful ROW_COUNT() -
          a number of modified rows
*/

uint sql_command_flags[SQLCOM_END + 1];
uint server_command_flags[COM_END + 1];

void init_sql_command_flags() {
  /* Initialize the server command flags array. */
  memset(server_command_flags, 0, sizeof(server_command_flags));

  server_command_flags[COM_SLEEP] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_INIT_DB] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_QUERY] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_FIELD_LIST] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_REFRESH] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STATISTICS] = CF_SKIP_QUESTIONS;
  server_command_flags[COM_PROCESS_KILL] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_PING] = CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_PREPARE] =
      CF_SKIP_QUESTIONS | CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STMT_EXECUTE] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STMT_SEND_LONG_DATA] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STMT_CLOSE] =
      CF_SKIP_QUESTIONS | CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STMT_RESET] =
      CF_SKIP_QUESTIONS | CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_STMT_FETCH] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_RESET_CONNECTION] = CF_ALLOW_PROTOCOL_PLUGIN;
  server_command_flags[COM_END] = CF_ALLOW_PROTOCOL_PLUGIN;

  /* Initialize the sql command flags array. */
  memset(sql_command_flags, 0, sizeof(sql_command_flags));

  /*
    In general, DDL statements do not generate row events and do not go
    through a cache before being written to the binary log. However, the
    CREATE TABLE...SELECT is an exception because it may generate row
    events. For that reason,  the SQLCOM_CREATE_TABLE  which represents
    a CREATE TABLE, including the CREATE TABLE...SELECT, has the
    CF_CAN_GENERATE_ROW_EVENTS flag. The distinction between a regular
    CREATE TABLE and the CREATE TABLE...SELECT is made in other parts of
    the code, in particular in the Query_log_event's constructor.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_AUTO_COMMIT_TRANS |
      CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_INDEX] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE] =
      CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE] =
      CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_LOAD] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_DB] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_DB] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_IMPORT] = CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_UPDATE] = CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                     CF_CAN_GENERATE_ROW_EVENTS |
                                     CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_UPDATE_MULTI] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS |
      CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  // This is INSERT VALUES(...), can be VALUES(stored_func()) so we trace it
  sql_command_flags[SQLCOM_INSERT] = CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                     CF_CAN_GENERATE_ROW_EVENTS |
                                     CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_INSERT_SELECT] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS |
      CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE] = CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                     CF_CAN_GENERATE_ROW_EVENTS |
                                     CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE_MULTI] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS |
      CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE] = CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                      CF_CAN_GENERATE_ROW_EVENTS |
                                      CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE_SELECT] =
      CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS |
      CF_OPTIMIZER_TRACE | CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_SELECT] =
      CF_REEXECUTION_FRAGILE | CF_CAN_GENERATE_ROW_EVENTS | CF_OPTIMIZER_TRACE |
      CF_HAS_RESULT_SET | CF_CAN_BE_EXPLAINED;
  // (1) so that subquery is traced when doing "SET @var = (subquery)"
  /*
    @todo SQLCOM_SET_OPTION should have CF_CAN_GENERATE_ROW_EVENTS
    set, because it may invoke a stored function that generates row
    events. /Sven
  */
  sql_command_flags[SQLCOM_SET_OPTION] =
      CF_REEXECUTION_FRAGILE | CF_AUTO_COMMIT_TRANS |
      CF_CAN_GENERATE_ROW_EVENTS | CF_OPTIMIZER_TRACE;  // (1)
  // (1) so that subquery is traced when doing "DO @var := (subquery)"
  sql_command_flags[SQLCOM_DO] = CF_REEXECUTION_FRAGILE |
                                 CF_CAN_GENERATE_ROW_EVENTS |
                                 CF_OPTIMIZER_TRACE;  // (1)

  sql_command_flags[SQLCOM_SET_PASSWORD] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS | CF_NEEDS_AUTOCOMMIT_OFF |
      CF_POTENTIAL_ATOMIC_DDL | CF_DISALLOW_IN_RO_TRANS;

  sql_command_flags[SQLCOM_SHOW_STATUS_PROC] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_STATUS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_DATABASES] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_TRIGGERS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_EVENTS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_OPEN_TABLES] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_PLUGINS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FIELDS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_KEYS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_VARIABLES] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_CHARSETS] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS] =
      CF_STATUS_COMMAND | CF_HAS_RESULT_SET | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_BINLOGS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STORAGE_ENGINES] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PRIVILEGES] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_WARNS] = CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ERRORS] =
      CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_MUTEX] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_LOGS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_GRANTS] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_PROC] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_FUNC] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_TRIGGER] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC] =
      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE | CF_HAS_RESULT_SET;
  sql_command_flags[SQLCOM_SHOW_PROC_CODE] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FUNC_CODE] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_EVENT] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILES] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILE] = CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_BINLOG_BASE64_EVENT] =
      CF_STATUS_COMMAND | CF_CAN_GENERATE_ROW_EVENTS;

  sql_command_flags[SQLCOM_SHOW_TABLES] =
      (CF_STATUS_COMMAND | CF_SHOW_TABLE_COMMAND | CF_HAS_RESULT_SET |
       CF_REEXECUTION_FRAGILE);
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS] =
      (CF_STATUS_COMMAND | CF_SHOW_TABLE_COMMAND | CF_HAS_RESULT_SET |
       CF_REEXECUTION_FRAGILE);
  /**
    ACL DDLs do not access data-dictionary tables. However, they still
    need to be marked to avoid autocommit. This is necessary because
    code which saves GTID state or slave state in the system tables
    at commit time does statement commit on low-level (see
    System_table_access::close_table()) and thus can pre-maturely commit
    DDL otherwise.
  */
  sql_command_flags[SQLCOM_CREATE_USER] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_RENAME_USER] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_USER] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_USER] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_GRANT] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_REVOKE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_REVOKE_ALL] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_USER_DEFAULT_ROLE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_GRANT_ROLE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_REVOKE_ROLE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_ROLE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_ROLE] =
      CF_CHANGES_DATA | CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;

  sql_command_flags[SQLCOM_OPTIMIZE] = CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_INSTANCE] = CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_FUNCTION] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_INSTALL_COMPONENT] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_COMPONENT] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_RESOURCE_GROUP] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS | CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_RESOURCE_GROUP] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS | CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_RESOURCE_GROUP] =
      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS | CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SET_RESOURCE_GROUP] =
      CF_CHANGES_DATA | CF_ALLOW_PROTOCOL_PLUGIN;

  sql_command_flags[SQLCOM_CLONE] =
      CF_AUTO_COMMIT_TRANS | CF_ALLOW_PROTOCOL_PLUGIN;

  /* Does not change the contents of the Diagnostics Area. */
  sql_command_flags[SQLCOM_GET_DIAGNOSTICS] = CF_DIAGNOSTIC_STMT;

  /*
    (1): without it, in "CALL some_proc((subq))", subquery would not be
    traced.
  */
  sql_command_flags[SQLCOM_CALL] = CF_REEXECUTION_FRAGILE |
                                   CF_CAN_GENERATE_ROW_EVENTS |
                                   CF_OPTIMIZER_TRACE;  // (1)
  sql_command_flags[SQLCOM_EXECUTE] = CF_CAN_GENERATE_ROW_EVENTS;

  /*
    The following admin table operations are allowed
    on log tables.
  */
  sql_command_flags[SQLCOM_REPAIR] =
      CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE] |=
      CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ANALYZE] =
      CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHECK] =
      CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_CREATE_USER] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_ROLE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_USER] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_ROLE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RESTART_SERVER] =
      CF_AUTO_COMMIT_TRANS | CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REVOKE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ROLE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_GRANT] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_GRANT_ROLE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER_DEFAULT_ROLE] |= CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_PRELOAD_KEYS] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_INSTANCE] |= CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_FLUSH] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RESET] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHANGE_MASTER] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHANGE_REPLICATION_FILTER] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_START] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_STOP] = CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_STOP_GROUP_REPLICATION] = CF_IMPLICIT_COMMIT_END;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SRS] |= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_SRS] |= CF_AUTO_COMMIT_TRANS;

  /*
    The following statements can deal with temporary tables,
    so temporary tables should be pre-opened for those statements to
    simplify privilege checking.

    There are other statements that deal with temporary tables and open
    them, but which are not listed here. The thing is that the order of
    pre-opening temporary tables for those statements is somewhat custom.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_TABLE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CREATE_INDEX] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ALTER_TABLE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_TRUNCATE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_LOAD] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_INDEX] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE_MULTI] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_INSERT] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_INSERT_SELECT] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE_MULTI] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPLACE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPLACE_SELECT] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SELECT] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SET_OPTION] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DO] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CALL] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECKSUM] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ANALYZE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECK] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_OPTIMIZE] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPAIR] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_PRELOAD_KEYS] |= CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE] |= CF_PREOPEN_TMP_TABLES;

  /*
    DDL statements that should start with closing opened handlers.

    We use this flag only for statements for which open HANDLERs
    have to be closed before emporary tables are pre-opened.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_TABLE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ALTER_TABLE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_TRUNCATE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_REPAIR] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_OPTIMIZE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ANALYZE] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CHECK] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CREATE_INDEX] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_INDEX] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_PRELOAD_KEYS] |= CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE] |= CF_HA_CLOSE;

  /*
    Mark statements that always are disallowed in read-only
    transactions. Note that according to the SQL standard,
    even temporary table DDL should be disallowed.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_INDEX] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_DB] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_DB] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_USER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_ROLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_USER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_ROLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_FUNCTION] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REPAIR] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_GRANT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_GRANT_ROLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ROLE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_INSTALL_COMPONENT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_COMPONENT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_INSTANCE] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_IMPORT] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SRS] |= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_SRS] |= CF_DISALLOW_IN_RO_TRANS;

  /*
    Mark statements that are allowed to be executed by the plugins.
  */
  sql_command_flags[SQLCOM_SELECT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_TABLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_INDEX] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_TABLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_UPDATE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_INSERT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_INSERT_SELECT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DELETE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_TRUNCATE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_TABLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_INDEX] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_DATABASES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_TABLES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_FIELDS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_KEYS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_VARIABLES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_STATUS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_ENGINE_LOGS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_ENGINE_MUTEX] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_GRANTS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CHARSETS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_TRIGGERS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_LOAD] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SET_OPTION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_LOCK_TABLES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_UNLOCK_TABLES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_GRANT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CHANGE_DB] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_DB] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_DB] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_DB] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REPAIR] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REPLACE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REPLACE_SELECT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_FUNCTION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_FUNCTION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REVOKE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_OPTIMIZE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CHECK] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_PRELOAD_KEYS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_FLUSH] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_KILL] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ANALYZE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ROLLBACK] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ROLLBACK_TO_SAVEPOINT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_COMMIT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SAVEPOINT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_RELEASE_SAVEPOINT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SLAVE_START] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SLAVE_STOP] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_START_GROUP_REPLICATION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_STOP_GROUP_REPLICATION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_BEGIN] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CHANGE_MASTER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CHANGE_REPLICATION_FILTER] |=
      CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_RENAME_TABLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_RESET] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_PURGE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_PURGE_BEFORE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_BINLOGS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_OPEN_TABLES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_HA_OPEN] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_HA_CLOSE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_HA_READ] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DELETE_MULTI] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_UPDATE_MULTI] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DO] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_WARNS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_EMPTY_QUERY] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_ERRORS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_STORAGE_ENGINES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PRIVILEGES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_HELP] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_USER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_USER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_RENAME_USER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REVOKE_ALL] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CHECKSUM] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CALL] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_PROCEDURE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_FUNCTION] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_PROC] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_FUNC] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_STATUS_PROC] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_PREPARE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_EXECUTE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DEALLOCATE_PREPARE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_VIEW] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_VIEW] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_TRIGGER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_TRIGGER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_START] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_END] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_PREPARE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_COMMIT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_ROLLBACK] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_XA_RECOVER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PROC_CODE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_FUNC_CODE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_BINLOG_BASE64_EVENT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PLUGINS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_SERVER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_SERVER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_SERVER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_EVENT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_EVENT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_EVENT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_EVENT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_EVENTS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_TRIGGER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PROFILE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_PROFILES] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SIGNAL] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_RESIGNAL] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_RELAYLOG_EVENTS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_GET_DIAGNOSTICS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_USER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_EXPLAIN_OTHER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SHOW_CREATE_USER] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SET_PASSWORD] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_SET_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_GRANT_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_REVOKE_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_ALTER_USER_DEFAULT_ROLE] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_IMPORT] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_END] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_CREATE_SRS] |= CF_ALLOW_PROTOCOL_PLUGIN;
  sql_command_flags[SQLCOM_DROP_SRS] |= CF_ALLOW_PROTOCOL_PLUGIN;

  /*
    Mark DDL statements which require that auto-commit mode to be temporarily
    turned off. See sqlcom_needs_autocommit_off() for more details.

    CREATE TABLE and DROP TABLE are not marked as such as they have special
    variants dealing with temporary tables which don't update data-dictionary
    at all and which should be allowed in the middle of transaction.
  */
  sql_command_flags[SQLCOM_CREATE_INDEX] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_TABLE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_TRUNCATE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_INDEX] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_DB] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_DB] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_DB] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_REPAIR] |= CF_NEEDS_AUTOCOMMIT_OFF;
  sql_command_flags[SQLCOM_OPTIMIZE] |= CF_NEEDS_AUTOCOMMIT_OFF;
  sql_command_flags[SQLCOM_RENAME_TABLE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_VIEW] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_VIEW] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_FUNCTION] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_FUNCTION] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_FUNCTION] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_PROCEDURE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_TRIGGER] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_TRIGGER] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_IMPORT] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN] |= CF_NEEDS_AUTOCOMMIT_OFF;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN] |= CF_NEEDS_AUTOCOMMIT_OFF;
  sql_command_flags[SQLCOM_CREATE_EVENT] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_ALTER_EVENT] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_EVENT] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_CREATE_SRS] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;
  sql_command_flags[SQLCOM_DROP_SRS] |=
      CF_NEEDS_AUTOCOMMIT_OFF | CF_POTENTIAL_ATOMIC_DDL;

  /*
    Mark these statements as SHOW commands using INFORMATION_SCHEMA system
    views.
  */
  sql_command_flags[SQLCOM_SHOW_CHARSETS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_DATABASES] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_TABLES] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_FIELDS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_KEYS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_EVENTS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_TRIGGERS] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_STATUS_PROC] |= CF_SHOW_USES_SYSTEM_VIEW;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC] |= CF_SHOW_USES_SYSTEM_VIEW;

  /**
    Some statements doesn't if the ACL CACHE is disabled using the
    --skip-grant-tables server option.
  */
  sql_command_flags[SQLCOM_SET_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_ALTER_USER_DEFAULT_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_CREATE_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_DROP_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_GRANT_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_ALTER_USER] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_GRANT] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_REVOKE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_REVOKE_ALL] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_REVOKE_ROLE] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_CREATE_USER] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_DROP_USER] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_RENAME_USER] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_SHOW_GRANTS] |= CF_REQUIRE_ACL_CACHE;
  sql_command_flags[SQLCOM_SET_PASSWORD] |= CF_REQUIRE_ACL_CACHE;
}

bool sqlcom_can_generate_row_events(enum enum_sql_command command) {
  return (sql_command_flags[command] & CF_CAN_GENERATE_ROW_EVENTS);
}

bool is_update_query(enum enum_sql_command command) {
  assert(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CHANGES_DATA) != 0;
}

bool is_explainable_query(enum enum_sql_command command) {
  assert(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CAN_BE_EXPLAINED) != 0;
}

/**
  Check if a sql command is allowed to write to log tables.
  @param command The SQL command
  @return true if writing is allowed
*/
bool is_log_table_write_query(enum enum_sql_command command) {
  assert(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_WRITE_LOGS_COMMAND) != 0;
}

/**
  Check if statement (typically DDL) needs auto-commit mode temporarily
  turned off.

  @note This is necessary to prevent InnoDB from automatically committing
        InnoDB transaction each time data-dictionary tables are closed
        after being updated.
*/
static bool sqlcom_needs_autocommit_off(const LEX *lex) {
  return (sql_command_flags[lex->sql_command] & CF_NEEDS_AUTOCOMMIT_OFF) ||
         (lex->sql_command == SQLCOM_CREATE_TABLE &&
          !(lex->create_info->options & HA_LEX_CREATE_TMP_TABLE)) ||
         (lex->sql_command == SQLCOM_DROP_TABLE && !lex->drop_temporary);
}

void execute_init_command(THD *thd, LEX_STRING *init_command,
                          mysql_rwlock_t *var_lock) {
  Protocol_classic *protocol = thd->get_protocol_classic();
  Vio *save_vio;
  ulong save_client_capabilities;
  COM_DATA com_data;

  mysql_rwlock_rdlock(var_lock);
  if (!init_command->length) {
    mysql_rwlock_unlock(var_lock);
    return;
  }

  /*
    copy the value under a lock, and release the lock.
    init_command has to be executed without a lock held,
    as it may try to change itself
  */
  size_t len = init_command->length;
  char *buf = thd->strmake(init_command->str, len);
  mysql_rwlock_unlock(var_lock);

#if defined(ENABLED_PROFILING)
  thd->profiling->start_new_query();
  thd->profiling->set_query_source(buf, len);
#endif

  /*
    Clear the DA in anticipation of possible failures in anticipation
    of possible command parsing failures.
  */
  thd->get_stmt_da()->reset_diagnostics_area();

  /* For per-query performance counters with log_slow_statement */
  struct System_status_var query_start_status;
  thd->clear_copy_status_var();
  if (opt_log_slow_extra) {
    thd->copy_status_var(&query_start_status);
  }

  THD_STAGE_INFO(thd, stage_execution_of_init_command);
  save_client_capabilities = protocol->get_client_capabilities();
  protocol->add_client_capability(CLIENT_MULTI_QUERIES);
  /*
    We do not prepare a COM_QUERY packet with query attributes
    since the init commands have nobody to supply query attributes.
  */
  protocol->remove_client_capability(CLIENT_QUERY_ATTRIBUTES);
  /*
    We don't need return result of execution to client side.
    To forbid this we should set thd->net.vio to 0.
  */
  save_vio = protocol->get_vio();
  protocol->set_vio(nullptr);
  if (!protocol->create_command(&com_data, COM_QUERY, (uchar *)buf, len))
    dispatch_command(thd, &com_data, COM_QUERY);
  protocol->set_client_capabilities(save_client_capabilities);
  protocol->set_vio(save_vio);

#if defined(ENABLED_PROFILING)
  thd->profiling->finish_current_query();
#endif
}

/* This works because items are allocated with (*THR_MALLOC)->Alloc() */

void free_items(Item *item) {
  Item *next;
  DBUG_TRACE;
  for (; item; item = next) {
    next = item->next_free;
    item->delete_self();
  }
}

/**
   This works because items are allocated with (*THR_MALLOC)->Alloc().
   @note The function also handles null pointers (empty list).
*/
void cleanup_items(Item *item) {
  DBUG_TRACE;
  for (; item; item = item->next_free) item->cleanup();
}

/**
  Bind Item fields to Field objects.

  @param first   Pointer to first item, follow "next" chain to visit all items
*/
void bind_fields(Item *first) {
  for (Item *item = first; item; item = item->next_free) item->bind_fields();
}

/**
  Read one command from connection and execute it (query or simple command).
  This function is called in loop from thread function.

  For profiling to work, it must never be called recursively.

  @retval
    0  success
  @retval
    1  request of thread shutdown (see dispatch_command() description)
*/

bool do_command(THD *thd) {
  bool return_value;
  int rc;
  NET *net = nullptr;
  enum enum_server_command command = COM_SLEEP;
  COM_DATA com_data;
  DBUG_TRACE;
  assert(thd->is_classic_protocol());

  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  thd->lex->set_current_query_block(nullptr);

  /*
    XXX: this code is here only to clear possible errors of init_connect.
    Consider moving to prepare_new_connection_state() instead.
    That requires making sure the DA is cleared before non-parsing statements
    such as COM_QUIT.
  */
  thd->clear_error();  // Clear error message
  thd->get_stmt_da()->reset_diagnostics_area();

  /*
    This thread will do a blocking read from the client which
    will be interrupted when the next command is received from
    the client, the connection is closed or "net_wait_timeout"
    number of seconds has passed.
  */
  net = thd->get_protocol_classic()->get_net();
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);
  net_new_transaction(net);

  /*
    Synchronization point for testing of KILL_CONNECTION.
    This sync point can wait here, to simulate slow code execution
    between the last test of thd->killed and blocking in read().

    The goal of this test is to verify that a connection does not
    hang, if it is killed at this point of execution.
    (Bug#37780 - main.kill fails randomly)

    Note that the sync point wait itself will be terminated by a
    kill. In this case it consumes a condition broadcast, but does
    not change anything else. The consumed broadcast should not
    matter here, because the read/recv() below doesn't use it.
  */
  DEBUG_SYNC(thd, "before_do_command_net_read");

  /* For per-query performance counters with log_slow_statement */
  struct System_status_var query_start_status;
  thd->clear_copy_status_var();
  if (opt_log_slow_extra) {
    thd->copy_status_var(&query_start_status);
  }

  rc = thd->m_mem_cnt.reset();
  if (rc)
    thd->m_mem_cnt.set_thd_error_status();
  else {
    /*
      Because of networking layer callbacks in place,
      this call will maintain the following instrumentation:
      - IDLE events
      - SOCKET events
      - STATEMENT events
      - STAGE events
      when reading a new network packet.
      In particular, a new instrumented statement is started.
      See init_net_server_extension()
    */
    thd->m_server_idle = true;
    rc = thd->get_protocol()->get_command(&com_data, &command);
    thd->m_server_idle = false;
  }

  if (rc) {
#ifndef NDEBUG
    char desc[VIO_DESCRIPTION_SIZE];
    vio_description(net->vio, desc);
    DBUG_PRINT("info", ("Got error %d reading command from socket %s",
                        net->error, desc));
#endif  // NDEBUG
    /* Instrument this broken statement as "statement/com/error" */
    thd->m_statement_psi = MYSQL_REFINE_STATEMENT(
        thd->m_statement_psi, com_statement_info[COM_END].m_key);

    /* Check if we can continue without closing the connection */

    /* The error must be set. */
    assert(thd->is_error());
    thd->send_statement_status();

    /* Mark the statement completed. */
    MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
    thd->m_statement_psi = nullptr;
    thd->m_digest = nullptr;

    if (rc < 0) {
      return_value = true;  // We have to close it.
      goto out;
    }
    net->error = NET_ERROR_UNSET;
    return_value = false;
    goto out;
  }

#ifndef NDEBUG
  char desc[VIO_DESCRIPTION_SIZE];
  vio_description(net->vio, desc);
  DBUG_PRINT("info", ("Command on %s = %d (%s)", desc, command,
                      Command_names::str_notranslate(command).c_str()));
  expected_from_debug_flag = TDM::ANY;
  DBUG_EXECUTE_IF("tdon", { expected_from_debug_flag = TDM::ON; });
  DBUG_EXECUTE_IF("tdzero", { expected_from_debug_flag = TDM::ZERO; });
  DBUG_EXECUTE_IF("tdna", { expected_from_debug_flag = TDM::NOT_AVAILABLE; });
#endif  // NDEBUG
  DBUG_PRINT("info", ("packet: '%*.s'; command: %d",
                      (int)thd->get_protocol_classic()->get_packet_length(),
                      thd->get_protocol_classic()->get_raw_packet(), command));
  if (thd->get_protocol_classic()->bad_packet)
    assert(0);  // Should be caught earlier

  // Reclaim some memory
  thd->get_protocol_classic()->get_output_packet()->shrink(
      thd->variables.net_buffer_length);
  /* Restore read timeout value */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);

  DEBUG_SYNC(thd, "before_command_dispatch");

  return_value = dispatch_command(thd, &com_data, command);
  thd->get_protocol_classic()->get_output_packet()->shrink(
      thd->variables.net_buffer_length);

out:
  /* The statement instrumentation must be closed in all cases. */
  assert(thd->m_digest == nullptr);
  assert(thd->m_statement_psi == nullptr);
  return return_value;
}

/**
  @brief Determine if an attempt to update a non-temporary table while the
    read-only option was enabled has been made.

  This is a helper function to mysql_execute_command.

  @note SQLCOM_UPDATE_MULTI is an exception and delt with elsewhere.

  @see mysql_execute_command
  @returns Status code
    @retval true The statement should be denied.
    @retval false The statement isn't updating any relevant tables.
*/
static bool deny_updates_if_read_only_option(THD *thd, Table_ref *all_tables) {
  DBUG_TRACE;

  if (!check_readonly(thd, false)) return false;

  LEX *lex = thd->lex;
  if (!(sql_command_flags[lex->sql_command] & CF_CHANGES_DATA)) return false;

  /* Multi update is an exception and is dealt with later. */
  if (lex->sql_command == SQLCOM_UPDATE_MULTI) return false;

  const bool create_temp_tables =
      (lex->sql_command == SQLCOM_CREATE_TABLE) &&
      (lex->create_info->options & HA_LEX_CREATE_TMP_TABLE);

  const bool create_real_tables =
      (lex->sql_command == SQLCOM_CREATE_TABLE) &&
      !(lex->create_info->options & HA_LEX_CREATE_TMP_TABLE);

  const bool drop_temp_tables =
      (lex->sql_command == SQLCOM_DROP_TABLE) && lex->drop_temporary;

  /* RENAME TABLES ignores shadowing temporary tables. */
  const bool rename_tables = (lex->sql_command == SQLCOM_RENAME_TABLE);

  const bool update_real_tables =
      ((create_real_tables || rename_tables ||
        some_non_temp_table_to_be_updated(thd, all_tables)) &&
       !(create_temp_tables || drop_temp_tables));

  const bool create_or_drop_databases =
      (lex->sql_command == SQLCOM_CREATE_DB) ||
      (lex->sql_command == SQLCOM_DROP_DB);

  if (update_real_tables || create_or_drop_databases) {
    /*
      An attempt was made to modify one or more non-temporary tables.
    */
    return true;
  }

  /* Assuming that only temporary tables are modified. */
  return false;
}

/**
  Check if a statement should be restarted in another storage engine,
  and restart the statement if needed.

  @param thd            the session
  @param parser_state   the parser state
  @param query_string   the query to reprepare and execute
  @param query_length   the length of the query
*/
static void check_secondary_engine_statement(THD *thd,
                                             Parser_state *parser_state,
                                             const char *query_string,
                                             size_t query_length) {
  bool use_secondary_engine = false;

  // Only restart the statement if a non-fatal error was raised.
  if (!thd->is_error() || thd->is_killed() || thd->is_fatal_error()) return;

  // Only SQL commands can be restarted with another storage engine.
  if (thd->lex->m_sql_cmd == nullptr) return;

  // The query cannot be restarted if it had started executing, since
  // it may have started sending results to the client.
  if (thd->lex->is_exec_completed()) return;

  // Decide which storage engine to use when retrying.
  switch (thd->secondary_engine_optimization()) {
    case Secondary_engine_optimization::PRIMARY_TENTATIVELY:
      // If a request to prepare for the secondary engine was
      // signalled, retry in the secondary engine.
      if (thd->get_stmt_da()->mysql_errno() != ER_PREPARE_FOR_SECONDARY_ENGINE)
        return;
      thd->set_secondary_engine_optimization(
          Secondary_engine_optimization::SECONDARY);
      use_secondary_engine = true;
      break;
    case Secondary_engine_optimization::SECONDARY:
      // If the query failed during offloading to a secondary engine,
      // retry in the primary engine. Don't retry if the failing query
      // was already using the primary storage engine.
      if (!thd->lex->m_sql_cmd->using_secondary_storage_engine()) return;
      thd->set_secondary_engine_optimization(
          Secondary_engine_optimization::PRIMARY_ONLY);
      break;
    default:
      return;
  }

  // Forget about the error raised in the previous attempt at preparing the
  // query.
  thd->clear_error();

  // Tell performance schema that the statement is restarted.
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());

  mysql_thread_set_secondary_engine(use_secondary_engine);

  thd->m_statement_psi = MYSQL_START_STATEMENT(
      &thd->m_statement_state, com_statement_info[thd->get_command()].m_key,
      thd->db().str, thd->db().length, thd->charset(), nullptr);

  mysql_statement_set_secondary_engine(thd->m_statement_psi,
                                       use_secondary_engine);

  DEBUG_SYNC(thd, "retry_secondary_engine");

  // Reset the statement digest state.
  thd->m_digest = &thd->m_digest_state;
  thd->m_digest->reset(thd->m_token_array, max_digest_length);

  // Reset the parser state.
  thd->set_query(query_string, query_length);
  parser_state->reset(query_string, query_length);

  // Disable the general log. The query was written to the general log in the
  // first attempt to execute it. No need to write it twice.
  const uint64_t saved_option_bits = thd->variables.option_bits;
  thd->variables.option_bits |= OPTION_LOG_OFF;

  // Restart the statement.
  dispatch_sql_command(thd, parser_state);

  // Restore the original option bits.
  thd->variables.option_bits = saved_option_bits;

  // Check if the restarted statement failed, and if so, if it needs
  // another restart/fallback to the primary storage engine.
  check_secondary_engine_statement(thd, parser_state, query_string,
                                   query_length);
}

/*Reference to the GR callback that receives incoming connections*/
static std::atomic<gr_incoming_connection_cb> com_incoming_gr_stream_cb;

void set_gr_incoming_connection(gr_incoming_connection_cb x) {
  com_incoming_gr_stream_cb.store(x);
}

gr_incoming_connection_cb get_gr_incoming_connection() {
  gr_incoming_connection_cb retval = nullptr;
  retval = com_incoming_gr_stream_cb.load();
  return retval;
}

void call_gr_incoming_connection_cb(THD *thd, int fd, SSL *ssl_ctx) {
  gr_incoming_connection_cb gr_connection_callback =
      get_gr_incoming_connection();

  if (gr_connection_callback) {
    gr_connection_callback(thd, fd, ssl_ctx);

    PSI_stage_info saved_stage;
    mysql_mutex_lock(&thd->LOCK_group_replication_connection_mutex);
    thd->ENTER_COND(&thd->COND_group_replication_connection_cond_var,
                    &thd->LOCK_group_replication_connection_mutex,
                    &stage_communication_delegation, &saved_stage);
    while (thd->is_killed() == THD::NOT_KILLED) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&thd->COND_group_replication_connection_cond_var,
                           &thd->LOCK_group_replication_connection_mutex,
                           &abstime);
    }
    mysql_mutex_unlock(&thd->LOCK_group_replication_connection_mutex);
    thd->EXIT_COND(&saved_stage);
  }
}

/**
  Deep copy the name and value of named parameters into the THD memory.

  We need to do this since the packet bytes will go away during query
  processing.
  It doesn't need to be done for the unnamed ones since they're being
  copied by and into Item_param. So we don't want to duplicate this.
  @sa @ref Item_param

  @param thd the thread to copy the parameters to.
  @param parameters the values to copy
  @param count the number of parameters to copy
*/
static void copy_bind_parameter_values(THD *thd, PS_PARAM *parameters,
                                       unsigned long count) {
  thd->bind_parameter_values = parameters;
  thd->bind_parameter_values_count = count;
  unsigned long inx;
  PS_PARAM *par;
  for (inx = 0, par = thd->bind_parameter_values; inx < count; inx++, par++) {
    if (par->name_length && par->name) {
      void *newd = thd->alloc(par->name_length);
      memcpy(newd, par->name, par->name_length);
      par->name = reinterpret_cast<unsigned char *>(newd);
    }
    if (par->length && par->value) {
      void *newd = thd->alloc(par->length);
      memcpy(newd, par->value, par->length);
      par->value = reinterpret_cast<unsigned char *>(newd);
    }
  }
}

/**
  Perform one connection-level (COM_XXXX) command.

  @param thd             connection handle
  @param command         type of command to perform
  @param com_data        com_data union to store the generated command

  @todo
    set thd->lex->sql_command to SQLCOM_END here.
  @todo
    The following has to be changed to an 8 byte integer

  @retval
    0   ok
  @retval
    1   request of thread shutdown, i. e. if command is
        COM_QUIT
*/
bool dispatch_command(THD *thd, const COM_DATA *com_data,
                      enum enum_server_command command) {
  assert(thd->lex->m_IS_table_stats.is_valid() == false);
  assert(thd->lex->m_IS_tablespace_stats.is_valid() == false);
#ifndef NDEBUG
  auto tabstat_grd = create_scope_guard([&]() {
    assert(thd->lex->m_IS_table_stats.is_valid() == false);
    assert(thd->lex->m_IS_tablespace_stats.is_valid() == false);
  });
#endif /* NDEBUG */
  bool error = false;
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  DBUG_TRACE;
  DBUG_PRINT("info", ("command: %d", command));

  Sql_cmd_clone *clone_cmd = nullptr;

  /* SHOW PROFILE instrumentation, begin */
#if defined(ENABLED_PROFILING)
  thd->profiling->start_new_query();
#endif

  /* Performance Schema Interface instrumentation, begin */
  thd->m_statement_psi = MYSQL_REFINE_STATEMENT(
      thd->m_statement_psi, com_statement_info[command].m_key);

  thd->set_command(command);
  /*
    Commands which always take a long time are logged into
    the slow log only if opt_log_slow_admin_statements is set.
  */
  thd->enable_slow_log = true;
  thd->lex->sql_command = SQLCOM_END; /* to avoid confusing VIEW detectors */
  /*
    KILL QUERY may come after cleanup in mysql_execute_command(). Next query
    execution is interrupted due to this. So resetting THD::killed here.

    THD::killed value can not be KILL_TIMEOUT here as timer used for statement
    max execution time is disarmed in the cleanup stage of
    mysql_execute_command. KILL CONNECTION should terminate the connection.
    Hence resetting THD::killed only for KILL QUERY case here.
  */
  if (thd->killed == THD::KILL_QUERY) thd->killed = THD::NOT_KILLED;
  thd->set_time();
  if (is_time_t_valid_for_timestamp(thd->query_start_in_secs()) == false) {
    /*
      If the time has gone past end of epoch we need to shutdown the server. But
      there is possibility of getting invalid time value on some platforms.
      For example, gettimeofday() might return incorrect value on solaris
      platform. Hence validating the current time with 5 iterations before
      initiating the normal server shutdown process because of time getting
      past 2038.
    */
    const int max_tries = 5;
    LogErr(WARNING_LEVEL, ER_CONFIRMING_THE_FUTURE, max_tries);

    int tries = 0;
    while (++tries <= max_tries) {
      thd->set_time();
      if (is_time_t_valid_for_timestamp(thd->query_start_in_secs()) == true) {
        LogErr(WARNING_LEVEL, ER_BACK_IN_TIME, tries);
        break;
      }
      LogErr(WARNING_LEVEL, ER_FUTURE_DATE, tries);
    }
    if (tries > max_tries) {
      /*
        If the time has got past epoch, we need to shut this server down.
        We do this by making sure every command is a shutdown and we
        have enough privileges to shut the server down

        TODO: remove this when we have full 64 bit my_time_t support
      */
      LogErr(ERROR_LEVEL, ER_UNSUPPORTED_DATE);
      ulong master_access = thd->security_context()->master_access();
      thd->security_context()->set_master_access(master_access | SHUTDOWN_ACL);
      error = true;
      kill_mysql();
    }
  }
  thd->set_query_id(next_query_id());
  thd->reset_rewritten_query();
  thd_manager->inc_thread_running();

  if (!(server_command_flags[command] & CF_SKIP_QUESTIONS))
    thd->status_var.questions++;

  /**
    Clear the set of flags that are expected to be cleared at the
    beginning of each command.
  */
  thd->server_status &= ~SERVER_STATUS_CLEAR_SET;

  if (thd->get_protocol()->type() == Protocol::PROTOCOL_PLUGIN &&
      !(server_command_flags[command] & CF_ALLOW_PROTOCOL_PLUGIN)) {
    my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
    thd->killed = THD::KILL_CONNECTION;
    error = true;
    goto done;
  }

  /**
    Enforce password expiration for all RPC commands, except the
    following:

    COM_QUERY/COM_STMT_PREPARE and COM_STMT_EXECUTE do a more
    fine-grained check later.
    COM_STMT_CLOSE and COM_STMT_SEND_LONG_DATA don't return anything.
    COM_PING only discloses information that the server is running,
       and that's available through other means.
    COM_QUIT should work even for expired statements.
  */
  if (unlikely(thd->security_context()->password_expired() &&
               command != COM_QUERY && command != COM_STMT_CLOSE &&
               command != COM_STMT_SEND_LONG_DATA && command != COM_PING &&
               command != COM_QUIT && command != COM_STMT_PREPARE &&
               command != COM_STMT_EXECUTE)) {
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    goto done;
  }

  if (mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_COMMAND_START), command,
                         Command_names::str_global(command).c_str())) {
    goto done;
  }

  switch (command) {
    case COM_INIT_DB: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      LEX_STRING tmp;
      thd->status_var.com_stat[SQLCOM_CHANGE_DB]++;
      thd->convert_string(&tmp, system_charset_info,
                          com_data->com_init_db.db_name,
                          com_data->com_init_db.length, thd->charset());

      LEX_CSTRING tmp_cstr = {tmp.str, tmp.length};
      if (!mysql_change_db(thd, tmp_cstr, false)) {
        query_logger.general_log_write(thd, command, thd->db().str,
                                       thd->db().length);
        my_ok(thd);
      }
      break;
    }
    case COM_REGISTER_SLAVE: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      // TODO: access of protocol_classic should be removed
      if (!register_replica(thd, thd->get_protocol_classic()->get_raw_packet(),
                            thd->get_protocol_classic()->get_packet_length()))
        my_ok(thd);
      break;
    }
    case COM_RESET_CONNECTION: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      thd->status_var.com_other++;
      thd->cleanup_connection();
      my_ok(thd);
      break;
    }
    case COM_CLONE: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      thd->status_var.com_other++;

      /* Try loading clone plugin */
      clone_cmd = new (thd->mem_root) Sql_cmd_clone();

      if (clone_cmd && clone_cmd->load(thd)) {
        clone_cmd = nullptr;
      }

      thd->lex->m_sql_cmd = clone_cmd;
      thd->lex->sql_command = SQLCOM_CLONE;

      break;
    }
    case COM_SUBSCRIBE_GROUP_REPLICATION_STREAM: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      Security_context *sctx = thd->security_context();
      if (!sctx->has_global_grant(STRING_WITH_LEN("GROUP_REPLICATION_STREAM"))
               .first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "");
        error = true;
        break;
      }

      if (!error && get_gr_incoming_connection() == nullptr) {
        my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
        error = true;
        break;
      }

      my_ok(thd);

      break;
    }
    case COM_CHANGE_USER: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);

      int auth_rc;
      thd->status_var.com_other++;

      thd->cleanup_connection();
      USER_CONN *save_user_connect =
          const_cast<USER_CONN *>(thd->get_user_connect());
      LEX_CSTRING save_db = thd->db();

      /*
        LOCK_thd_security_ctx protects the THD's security-context from
        inspection by SHOW PROCESSLIST while we're updating it. However,
        there is no need to protect this context while we're reading it,
        sinceother threads are not supposed to modify it.
        Nested acquiring of LOCK_thd_data is fine (see below).
      */
      Security_context save_security_ctx(*(thd->security_context()));

      MUTEX_LOCK(grd_secctx, &thd->LOCK_thd_security_ctx);

      auth_rc = acl_authenticate(thd, COM_CHANGE_USER);
      auth_rc |= mysql_audit_notify(
          thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_CHANGE_USER));
      if (auth_rc) {
        *thd->security_context() = save_security_ctx;
        thd->set_user_connect(save_user_connect);
        thd->reset_db(save_db);

        my_error(ER_ACCESS_DENIED_CHANGE_USER_ERROR, MYF(0),
                 thd->security_context()->user().str,
                 thd->security_context()->host_or_ip().str,
                 (thd->password ? ER_THD(thd, ER_YES) : ER_THD(thd, ER_NO)));
        thd->killed = THD::KILL_CONNECTION;
        error = true;
      } else {
#ifdef HAVE_PSI_THREAD_INTERFACE
        /* we've authenticated new user */
        PSI_THREAD_CALL(notify_session_change_user)(thd->get_psi());
#endif /* HAVE_PSI_THREAD_INTERFACE */

        if (save_user_connect) decrease_user_connections(save_user_connect);
        mysql_mutex_lock(&thd->LOCK_thd_data);
        my_free(const_cast<char *>(save_db.str));
        save_db = NULL_CSTR;
        mysql_mutex_unlock(&thd->LOCK_thd_data);
      }
      break;
    }
    case COM_STMT_EXECUTE: {
      /* Clear possible warnings from the previous command */
      thd->reset_for_next_command();

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      Prepared_statement *stmt = nullptr;
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt)) {
        PS_PARAM *parameters = com_data->com_stmt_execute.parameters;
        copy_bind_parameter_values(thd, parameters,
                                   com_data->com_stmt_execute.parameter_count);

        mysqld_stmt_execute(thd, stmt, com_data->com_stmt_execute.has_new_types,
                            com_data->com_stmt_execute.open_cursor, parameters);
        thd->bind_parameter_values = nullptr;
        thd->bind_parameter_values_count = 0;
      }
      break;
    }
    case COM_STMT_FETCH: {
      /* Clear possible warnings from the previous command */
      thd->reset_for_next_command();

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      Prepared_statement *stmt = nullptr;
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt))
        mysqld_stmt_fetch(thd, stmt, com_data->com_stmt_fetch.num_rows);

      break;
    }
    case COM_STMT_SEND_LONG_DATA: {
      Prepared_statement *stmt;
      thd->get_stmt_da()->disable_status();

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt))
        mysql_stmt_get_longdata(thd, stmt,
                                com_data->com_stmt_send_long_data.param_number,
                                com_data->com_stmt_send_long_data.longdata,
                                com_data->com_stmt_send_long_data.length);
      break;
    }
    case COM_STMT_PREPARE: {
      /* Clear possible warnings from the previous command */
      thd->reset_for_next_command();
      Prepared_statement *stmt = nullptr;

      DBUG_EXECUTE_IF("parser_stmt_to_error_log", {
        LogErr(INFORMATION_LEVEL, ER_PARSER_TRACE,
               com_data->com_stmt_prepare.query);
      });
      DBUG_EXECUTE_IF("parser_stmt_to_error_log_with_system_prio", {
        LogErr(SYSTEM_LEVEL, ER_PARSER_TRACE, com_data->com_stmt_prepare.query);
      });

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt))
        mysqld_stmt_prepare(thd, com_data->com_stmt_prepare.query,
                            com_data->com_stmt_prepare.length, stmt);
      break;
    }
    case COM_STMT_CLOSE: {
      Prepared_statement *stmt = nullptr;
      thd->get_stmt_da()->disable_status();

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt))
        mysqld_stmt_close(thd, stmt);
      break;
    }
    case COM_STMT_RESET: {
      /* Clear possible warnings from the previous command */
      thd->reset_for_next_command();

      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      Prepared_statement *stmt = nullptr;
      if (!mysql_stmt_precheck(thd, com_data, command, &stmt))
        mysqld_stmt_reset(thd, stmt);
      break;
    }
    case COM_QUERY: {
      assert(thd->m_digest == nullptr);
      thd->m_digest = &thd->m_digest_state;
      thd->m_digest->reset(thd->m_token_array, max_digest_length);

      if (alloc_query(thd, com_data->com_query.query,
                      com_data->com_query.length))
        break;  // fatal error is set

      const char *packet_end = thd->query().str + thd->query().length;

      if (opt_general_log_raw)
        query_logger.general_log_write(thd, command, thd->query().str,
                                       thd->query().length);

      DBUG_PRINT("query", ("%-.4096s", thd->query().str));

#if defined(ENABLED_PROFILING)
      thd->profiling->set_query_source(thd->query().str, thd->query().length);
#endif

      const LEX_CSTRING orig_query = thd->query();

      Parser_state parser_state;
      if (parser_state.init(thd, thd->query().str, thd->query().length)) break;

      parser_state.m_input.m_has_digest = true;

      // we produce digest if it's not explicitly turned off
      // by setting maximum digest length to zero
      if (get_max_digest_length() != 0)
        parser_state.m_input.m_compute_digest = true;

      // Initially, prepare and optimize the statement for the primary
      // storage engine. If an eligible secondary storage engine is
      // found, the statement may be reprepared for the secondary
      // storage engine later.
      const auto saved_secondary_engine = thd->secondary_engine_optimization();
      thd->set_secondary_engine_optimization(
          Secondary_engine_optimization::PRIMARY_TENTATIVELY);

      copy_bind_parameter_values(thd, com_data->com_query.parameters,
                                 com_data->com_query.parameter_count);

      dispatch_sql_command(thd, &parser_state);

      // Check if the statement failed and needs to be restarted in
      // another storage engine.
      check_secondary_engine_statement(thd, &parser_state, orig_query.str,
                                       orig_query.length);

      thd->set_secondary_engine_optimization(saved_secondary_engine);

      DBUG_EXECUTE_IF("parser_stmt_to_error_log", {
        LogErr(INFORMATION_LEVEL, ER_PARSER_TRACE, thd->query().str);
      });
      DBUG_EXECUTE_IF("parser_stmt_to_error_log_with_system_prio", {
        LogErr(SYSTEM_LEVEL, ER_PARSER_TRACE, thd->query().str);
      });

      while (!thd->killed && (parser_state.m_lip.found_semicolon != nullptr) &&
             !thd->is_error()) {
        /*
          Multiple queries exits, execute them individually
        */
        const char *beginning_of_next_stmt = parser_state.m_lip.found_semicolon;

        /* Finalize server status flags after executing a statement. */
        thd->update_slow_query_status();
        thd->send_statement_status();

        const std::string &cn = Command_names::str_global(command);
        mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_STATUS),
                           thd->get_stmt_da()->is_error()
                               ? thd->get_stmt_da()->mysql_errno()
                               : 0,
                           cn.c_str(), cn.length());

        size_t length =
            static_cast<size_t>(packet_end - beginning_of_next_stmt);

        log_slow_statement(thd);

        thd->reset_copy_status_var();

        /* Remove garbage at start of query */
        while (length > 0 &&
               my_isspace(thd->charset(), *beginning_of_next_stmt)) {
          beginning_of_next_stmt++;
          length--;
        }

        /* PSI end */
        MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
        thd->m_statement_psi = nullptr;
        thd->m_digest = nullptr;

/* SHOW PROFILE end */
#if defined(ENABLED_PROFILING)
        thd->profiling->finish_current_query();
#endif

/* SHOW PROFILE begin */
#if defined(ENABLED_PROFILING)
        thd->profiling->start_new_query("continuing");
        thd->profiling->set_query_source(beginning_of_next_stmt, length);
#endif

        mysql_thread_set_secondary_engine(false);

        /* PSI begin */
        thd->m_digest = &thd->m_digest_state;
        thd->m_digest->reset(thd->m_token_array, max_digest_length);

        thd->m_statement_psi = MYSQL_START_STATEMENT(
            &thd->m_statement_state, com_statement_info[command].m_key,
            thd->db().str, thd->db().length, thd->charset(), nullptr);
        THD_STAGE_INFO(thd, stage_starting);

        thd->set_query(beginning_of_next_stmt, length);
        thd->set_query_id(next_query_id());

        /*
          Count each statement from the client.
        */
        thd->status_var.questions++;
        thd->set_time(); /* Reset the query start time. */
        parser_state.reset(beginning_of_next_stmt, length);
        thd->set_secondary_engine_optimization(
            Secondary_engine_optimization::PRIMARY_TENTATIVELY);
        /* TODO: set thd->lex->sql_command to SQLCOM_END here */
        dispatch_sql_command(thd, &parser_state);

        check_secondary_engine_statement(thd, &parser_state,
                                         beginning_of_next_stmt, length);

        thd->set_secondary_engine_optimization(saved_secondary_engine);
      }

      thd->bind_parameter_values = nullptr;
      thd->bind_parameter_values_count = 0;

      /* Need to set error to true for graceful shutdown */
      if ((thd->lex->sql_command == SQLCOM_SHUTDOWN) &&
          (thd->get_stmt_da()->is_ok()))
        error = true;

      DBUG_PRINT("info", ("query ready"));
      break;
    }
    case COM_FIELD_LIST:  // This isn't actually needed
    {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      char *fields;
      /* Locked closure of all tables */
      LEX_STRING table_name;
      LEX_STRING db;
      push_deprecated_warn(thd, "COM_FIELD_LIST",
                           "SHOW COLUMNS FROM statement");
      /*
        SHOW statements should not add the used tables to the list of tables
        used in a transaction.
      */
      MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

      thd->status_var.com_stat[SQLCOM_SHOW_FIELDS]++;
      if (thd->copy_db_to(&db.str, &db.length)) break;
      thd->convert_string(&table_name, system_charset_info,
                          (char *)com_data->com_field_list.table_name,
                          com_data->com_field_list.table_name_length,
                          thd->charset());
      Ident_name_check ident_check_status =
          check_table_name(table_name.str, table_name.length);
      if (ident_check_status == Ident_name_check::WRONG) {
        /* this is OK due to convert_string() null-terminating the string */
        my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name.str);
        break;
      } else if (ident_check_status == Ident_name_check::TOO_LONG) {
        my_error(ER_TOO_LONG_IDENT, MYF(0), table_name.str);
        break;
      }
      mysql_reset_thd_for_next_command(thd);
      lex_start(thd);
      /* Must be before we init the table list. */
      if (lower_case_table_names && !is_infoschema_db(db.str, db.length))
        table_name.length = my_casedn_str(files_charset_info, table_name.str);
      Table_ref table_list(db.str, db.length, table_name.str, table_name.length,
                           table_name.str, TL_READ);
      /*
        Init Table_ref members necessary when the undelrying
        table is view.
      */
      table_list.query_block = thd->lex->query_block;
      thd->lex->query_block->m_table_list.link_in_list(&table_list,
                                                       &table_list.next_local);
      thd->lex->add_to_query_tables(&table_list);

      if (is_infoschema_db(table_list.db, table_list.db_length)) {
        ST_SCHEMA_TABLE *schema_table =
            find_schema_table(thd, table_list.alias);
        if (schema_table) table_list.schema_table = schema_table;
      }

      if (!(fields =
                (char *)thd->memdup(com_data->com_field_list.query,
                                    com_data->com_field_list.query_length)))
        break;
      // Don't count end \0
      thd->set_query(fields, com_data->com_field_list.query_length - 1);
      query_logger.general_log_print(thd, command, "%s %s",
                                     table_list.table_name, fields);

      if (open_temporary_tables(thd, &table_list)) break;

      if (check_table_access(thd, SELECT_ACL, &table_list, true, UINT_MAX,
                             false))
        break;

      thd->lex->sql_command = SQLCOM_SHOW_FIELDS;
      // See comment in opt_trace_disable_if_no_security_context_access()
      Opt_trace_start ots(thd, &table_list, thd->lex->sql_command, nullptr,
                          nullptr, 0, nullptr, nullptr);

      mysqld_list_fields(thd, &table_list, fields);

      thd->lex->cleanup(true);
      /* No need to rollback statement transaction, it's not started. */
      assert(thd->get_transaction()->is_empty(Transaction_ctx::STMT));
      close_thread_tables(thd);
      thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

      if (thd->transaction_rollback_request) {
        /*
          Transaction rollback was requested since MDL deadlock was
          discovered while trying to open tables. Rollback transaction
          in all storage engines including binary log and release all
          locks.
        */
        trans_rollback_implicit(thd);
        thd->mdl_context.release_transactional_locks();
      }

      thd->cleanup_after_query();
      thd->lex->destroy();
      break;
    }
    case COM_QUIT:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      /* Prevent results of the form, "n>0 rows sent, 0 bytes sent" */
      thd->set_sent_row_count(0);
      /* We don't calculate statistics for this command */
      query_logger.general_log_print(thd, command, NullS);
      // Don't give 'abort' message
      // TODO: access of protocol_classic should be removed
      if (thd->is_classic_protocol())
        thd->get_protocol_classic()->get_net()->error = NET_ERROR_UNSET;
      thd->get_stmt_da()->disable_status();  // Don't send anything back
      error = true;                          // End server
      break;
    case COM_BINLOG_DUMP_GTID:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      // TODO: access of protocol_classic should be removed
      error = com_binlog_dump_gtid(
          thd, (char *)thd->get_protocol_classic()->get_raw_packet(),
          thd->get_protocol_classic()->get_packet_length());
      break;
    case COM_BINLOG_DUMP:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      // TODO: access of protocol_classic should be removed
      error = com_binlog_dump(
          thd, (char *)thd->get_protocol_classic()->get_raw_packet(),
          thd->get_protocol_classic()->get_packet_length());
      break;
    case COM_REFRESH: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      int not_used;
      push_deprecated_warn(thd, "COM_REFRESH", "FLUSH statement");
      /*
        Initialize thd->lex since it's used in many base functions, such as
        open_tables(). Otherwise, it remains uninitialized and may cause crash
        during execution of COM_REFRESH.
      */
      lex_start(thd);

      thd->status_var.com_stat[SQLCOM_FLUSH]++;
      ulong options = (ulong)com_data->com_refresh.options;
      if (trans_commit_implicit(thd)) break;
      thd->mdl_context.release_transactional_locks();
      if (check_global_access(thd, RELOAD_ACL)) break;
      query_logger.general_log_print(thd, command, NullS);
#ifndef NDEBUG
      bool debug_simulate = false;
      DBUG_EXECUTE_IF("simulate_detached_thread_refresh",
                      debug_simulate = true;);
      if (debug_simulate) {
        /*
          Simulate a reload without a attached thread session.
          Provides a environment similar to that of when the
          server receives a SIGHUP signal and reloads caches
          and flushes tables.
        */
        bool res;
        current_thd = nullptr;
        res = handle_reload_request(nullptr, options | REFRESH_FAST, nullptr,
                                    &not_used);
        current_thd = thd;
        if (res) break;
      } else
#endif
          if (handle_reload_request(thd, options, (Table_ref *)nullptr,
                                    &not_used))
        break;
      if (trans_commit_implicit(thd)) break;
      close_thread_tables(thd);
      thd->mdl_context.release_transactional_locks();
      thd->lex->destroy();
      my_ok(thd);
      break;
    }
    case COM_STATISTICS: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      System_status_var current_global_status_var;
      ulong uptime;
      size_t length [[maybe_unused]];
      ulonglong queries_per_second1000;
      char buff[250];
      size_t buff_len = sizeof(buff);

      query_logger.general_log_print(thd, command, NullS);
      thd->status_var.com_stat[SQLCOM_SHOW_STATUS]++;
      mysql_mutex_lock(&LOCK_status);
      calc_sum_of_all_status(&current_global_status_var);
      mysql_mutex_unlock(&LOCK_status);
      if (!(uptime = (ulong)(thd->query_start_in_secs() - server_start_time)))
        queries_per_second1000 = 0;
      else
        queries_per_second1000 = thd->query_id * 1000LL / uptime;

      length = snprintf(buff, buff_len - 1,
                        "Uptime: %lu  Threads: %d  Questions: %lu  "
                        "Slow queries: %llu  Opens: %llu  Flush tables: %lu  "
                        "Open tables: %u  Queries per second avg: %u.%03u",
                        uptime, (int)thd_manager->get_thd_count(),
                        (ulong)thd->query_id,
                        current_global_status_var.long_query_count,
                        current_global_status_var.opened_tables,
                        refresh_version, table_cache_manager.cached_tables(),
                        (uint)(queries_per_second1000 / 1000),
                        (uint)(queries_per_second1000 % 1000));
      // TODO: access of protocol_classic should be removed.
      // should be rewritten using store functions
      if (thd->get_protocol_classic()->write(pointer_cast<const uchar *>(buff),
                                             length))
        break;
      if (thd->get_protocol()->flush()) break;
      thd->get_stmt_da()->disable_status();
      break;
    }
    case COM_PING:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      thd->status_var.com_other++;
      my_ok(thd);  // Tell client we are alive
      break;
    case COM_PROCESS_INFO:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      bool global_access;
      LEX_CSTRING db_saved;
      thd->status_var.com_stat[SQLCOM_SHOW_PROCESSLIST]++;
      push_deprecated_warn(thd, "COM_PROCESS_INFO",
                           "SHOW PROCESSLIST statement");
      global_access = (check_global_access(thd, PROCESS_ACL) == 0);
      if (!thd->security_context()->priv_user().str[0] && !global_access) break;
      query_logger.general_log_print(thd, command, NullS);
      db_saved = thd->db();

      DBUG_EXECUTE_IF("force_db_name_to_null", thd->reset_db(NULL_CSTR););

      mysqld_list_processes(
          thd, global_access ? NullS : thd->security_context()->priv_user().str,
          false, false);

      DBUG_EXECUTE_IF("force_db_name_to_null", thd->reset_db(db_saved););
      break;
    case COM_PROCESS_KILL: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      push_deprecated_warn(thd, "COM_PROCESS_KILL",
                           "KILL CONNECTION/QUERY statement");
      if (thd_manager->get_thread_id() & (~0xfffffffful))
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "thread_id", "mysql_kill()");
      else {
        thd->status_var.com_stat[SQLCOM_KILL]++;
        sql_kill(thd, com_data->com_kill.id, false);
      }
      break;
    }
    case COM_SET_OPTION: {
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      thd->status_var.com_stat[SQLCOM_SET_OPTION]++;

      switch (com_data->com_set_option.opt_command) {
        case (int)MYSQL_OPTION_MULTI_STATEMENTS_ON:
          // TODO: access of protocol_classic should be removed
          thd->get_protocol_classic()->add_client_capability(
              CLIENT_MULTI_STATEMENTS);
          my_eof(thd);
          break;
        case (int)MYSQL_OPTION_MULTI_STATEMENTS_OFF:
          thd->get_protocol_classic()->remove_client_capability(
              CLIENT_MULTI_STATEMENTS);
          my_eof(thd);
          break;
        default:
          my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
          break;
      }
      break;
    }
    case COM_DEBUG:
      MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi, false);
      thd->status_var.com_other++;
      if (check_global_access(thd, SUPER_ACL)) break; /* purecov: inspected */
      query_logger.general_log_print(thd, command, NullS);
      my_eof(thd);
#ifdef WITH_LOCK_ORDER
      LO_dump();
#endif /* WITH_LOCK_ORDER */
      break;
    case COM_SLEEP:
    case COM_CONNECT:         // Impossible here
    case COM_TIME:            // Impossible from client
    case COM_DELAYED_INSERT:  // INSERT DELAYED has been removed.
    case COM_END:
    default:
      my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
      break;
  }

done:
  assert(thd->open_tables == nullptr ||
         (thd->locked_tables_mode == LTM_LOCK_TABLES));

  /* Finalize server status flags after executing a command. */
  thd->update_slow_query_status();
  if (thd->killed) thd->send_kill_message();
  thd->send_statement_status();

  /* After sending response, switch to clone protocol */
  if (clone_cmd != nullptr) {
    assert(command == COM_CLONE);
    error = clone_cmd->execute_server(thd);
  }

  if (command == COM_SUBSCRIBE_GROUP_REPLICATION_STREAM && !error) {
    call_gr_incoming_connection_cb(
        thd, thd->active_vio->mysql_socket.fd,
        thd->active_vio->ssl_arg ? static_cast<SSL *>(thd->active_vio->ssl_arg)
                                 : nullptr);
  }

  thd->rpl_thd_ctx.session_gtids_ctx().notify_after_response_packet(thd);

  if (!thd->is_error() && !thd->killed)
    mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_RESULT), 0, nullptr,
                       0);

  const std::string &cn = Command_names::str_global(command);
  mysql_audit_notify(
      thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_STATUS),
      thd->get_stmt_da()->is_error() ? thd->get_stmt_da()->mysql_errno() : 0,
      cn.c_str(), cn.length());

  /* command_end is informational only. The plugin cannot abort
     execution of the command at this point. */
  mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_COMMAND_END), command,
                     cn.c_str());

  log_slow_statement(thd);

  THD_STAGE_INFO(thd, stage_cleaning_up);

  thd->reset_query();
  thd->set_command(COM_SLEEP);
  thd->set_proc_info(nullptr);
  thd->lex->sql_command = SQLCOM_END;

  /* Performance Schema Interface instrumentation, end */
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  thd->m_statement_psi = nullptr;
  thd->m_digest = nullptr;
  thd->reset_query_for_display();

  /* Prevent rewritten query from getting "stuck" in SHOW PROCESSLIST. */
  thd->reset_rewritten_query();

  thd_manager->dec_thread_running();

  /* Freeing the memroot will leave the THD::work_part_info invalid. */
  thd->work_part_info = nullptr;

  /*
    If we've allocated a lot of memory (compared to the default preallocation
    size = 8192; note that we don't actually preallocate anymore), free
    it so that one big query won't cause us to hold on to a lot of RAM forever.
    If not, keep the last block so that the next query will hopefully be able to
    run without allocating memory from the OS.

    The factor 5 is pretty much arbitrary, but ends up allowing three
    allocations (1 + 1.5 + 1.5²) under the current allocation policy.
  */
  constexpr size_t kPreallocSz = 40960;
  if (thd->mem_root->allocated_size() < kPreallocSz)
    thd->mem_root->ClearForReuse();
  else
    thd->mem_root->Clear();

    /* SHOW PROFILE instrumentation, end */
#if defined(ENABLED_PROFILING)
  thd->profiling->finish_current_query();
#endif

  return error;
}

/**
  Shutdown the mysqld server.

  @param  thd        Thread (session) context.
  @param  level      Shutdown level.

  @retval
    true                 success
  @retval
    false                When user has insufficient privilege or unsupported
  shutdown level

*/

bool shutdown(THD *thd, enum mysql_enum_shutdown_level level) {
  DBUG_TRACE;
  bool res = false;
  thd->lex->no_write_to_binlog = true;

  if (check_global_access(thd, SHUTDOWN_ACL))
    goto error; /* purecov: inspected */

  if (level == SHUTDOWN_DEFAULT)
    level = SHUTDOWN_WAIT_ALL_BUFFERS;  // soon default will be configurable
  else if (level != SHUTDOWN_WAIT_ALL_BUFFERS) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "this shutdown level");
    goto error;
    ;
  }

  my_ok(thd);

  LogErr(SYSTEM_LEVEL, ER_SERVER_SHUTDOWN_INFO,
         thd->security_context()->user().str, server_version,
         MYSQL_COMPILATION_COMMENT_SERVER);

  DBUG_PRINT("quit", ("Got shutdown command for level %u", level));
  query_logger.general_log_print(thd, COM_QUERY, NullS);
  kill_mysql();
  res = true;

error:
  return res;
}

/**
  Create a Table_ref object for an INFORMATION_SCHEMA table.

    This function is used in the parser to convert a SHOW or DESCRIBE
    table_name command to a SELECT from INFORMATION_SCHEMA.
    It prepares a Query_block and a Table_ref object to represent the
    given command as a SELECT parse tree.

  @param thd              thread handle
  @param lex              current lex
  @param table_ident      table alias if it's used
  @param schema_table_idx the type of the INFORMATION_SCHEMA table to be
                          created

  @note
    Due to the way this function works with memory and LEX it cannot
    be used outside the parser (parse tree transformations outside
    the parser break PS and SP).

  @retval
    0                 success
  @retval
    1                 out of memory or SHOW commands are not allowed
                      in this version of the server.
*/

int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx) {
  Query_block *schema_query_block = nullptr;
  DBUG_TRACE;

  switch (schema_table_idx) {
    case SCH_TMP_TABLE_COLUMNS:
    case SCH_TMP_TABLE_KEYS: {
      assert(table_ident);
      Table_ref **query_tables_last = lex->query_tables_last;
      if ((schema_query_block = lex->new_empty_query_block()) == nullptr)
        return 1; /* purecov: inspected */
      if (!schema_query_block->add_table_to_list(thd, table_ident, nullptr, 0,
                                                 TL_READ, MDL_SHARED_READ))
        return 1;
      lex->query_tables_last = query_tables_last;
      break;
    }
    case SCH_PROFILES:
      /*
        Mark this current profiling record to be discarded.  We don't
        wish to have SHOW commands show up in profiling->
      */
#if defined(ENABLED_PROFILING)
      thd->profiling->discard_current_query();
#endif
      break;
    case SCH_OPTIMIZER_TRACE:
    case SCH_OPEN_TABLES:
    case SCH_ENGINES:
    case SCH_USER_PRIVILEGES:
    case SCH_SCHEMA_PRIVILEGES:
    case SCH_TABLE_PRIVILEGES:
    case SCH_COLUMN_PRIVILEGES:
    default:
      break;
  }

  Query_block *query_block = lex->current_query_block();
  if (make_schema_query_block(thd, query_block, schema_table_idx)) {
    return 1;
  }
  Table_ref *table_list = query_block->get_table_list();
  table_list->schema_query_block = schema_query_block;
  table_list->schema_table_reformed = true;
  return 0;
}

/**
  Read query from packet and store in thd->query.
  Used in COM_QUERY and COM_STMT_PREPARE.

    Sets the following THD variables:
  - query
  - query_length

  @retval
    false ok
  @retval
    true  error;  In this case thd->fatal_error is set
*/

bool alloc_query(THD *thd, const char *packet, size_t packet_length) {
  DBUG_TRACE;
  /* Remove garbage at start and end of query */
  while (packet_length > 0 && my_isspace(thd->charset(), packet[0])) {
    packet++;
    packet_length--;
  }
  const char *pos = packet + packet_length;  // Point at end null
  while (packet_length > 0 &&
         (pos[-1] == ';' || my_isspace(thd->charset(), pos[-1]))) {
    pos--;
    packet_length--;
  }

  char *query = static_cast<char *>(thd->alloc(packet_length + 1));
  if (!query) return true;
  memcpy(query, packet, packet_length);
  query[packet_length] = '\0';

  thd->set_query(query, packet_length);
  DBUG_PRINT("thd_query", ("thd->thread_id():%u thd:%p query:%s",
                           thd->thread_id(), thd, query));
  return false;
}

static bool sp_process_definer(THD *thd) {
  DBUG_TRACE;

  LEX *lex = thd->lex;

  /*
    If the definer is not specified, this means that CREATE-statement missed
    DEFINER-clause. DEFINER-clause can be missed in two cases:

      - The user submitted a statement w/o the clause. This is a normal
        case, we should assign CURRENT_USER as definer.

      - Our slave received an updated from the master, that does not
        replicate definer for stored routines. We should also assign
        CURRENT_USER as definer here, but also we should mark this routine
        as NON-SUID. This is essential for the sake of backward
        compatibility.

        The problem is the slave thread is running under "special" user (@),
        that actually does not exist. In the older versions we do not fail
        execution of a stored routine if its definer does not exist and
        continue the execution under the authorization of the invoker
        (BUG#13198). And now if we try to switch to slave-current-user (@),
        we will fail.

        Actually, this leads to the inconsistent state of master and
        slave (different definers, different SUID behaviour), but it seems,
        this is the best we can do.
  */

  if (!lex->definer) {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    lex->definer = create_default_definer(thd);

    /* Error has been already reported. */
    if (lex->definer == nullptr) return true;

    if (thd->slave_thread && lex->sphead)
      lex->sphead->m_chistics->suid = SP_IS_NOT_SUID;
  } else {
    /*
      If the specified definer differs from the current user, we
      should check that the current user has a set_user_id privilege
      (in order to create a stored routine under another user one must
       have a set_user_id privilege).
    */
    Security_context *sctx = thd->security_context();
    if ((strcmp(lex->definer->user.str,
                thd->security_context()->priv_user().str) ||
         my_strcasecmp(system_charset_info, lex->definer->host.str,
                       thd->security_context()->priv_host().str))) {
      if (!(sctx->check_access(SUPER_ACL) ||
            sctx->has_global_grant(STRING_WITH_LEN("SET_USER_ID")).first)) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or SET_USER_ID");
        return true;
      }
      if (sctx->can_operate_with({lex->definer}, consts::system_user))
        return true;
    }
  }

  /* Check that the specified definer exists. Emit a warning if not. */

  if (!is_acl_user(thd, lex->definer->host.str, lex->definer->user.str)) {
    push_warning_printf(thd, Sql_condition::SL_NOTE, ER_NO_SUCH_USER,
                        ER_THD(thd, ER_NO_SUCH_USER), lex->definer->user.str,
                        lex->definer->host.str);
  }

  return false;
}

/**
  Auxiliary call that opens and locks tables for LOCK TABLES statement
  and initializes the list of locked tables.

  @param thd     Thread context.
  @param tables  List of tables to be locked.

  @return false in case of success, true in case of error.
*/

static bool lock_tables_open_and_lock_tables(THD *thd, Table_ref *tables) {
  Lock_tables_prelocking_strategy lock_tables_prelocking_strategy;
  MDL_deadlock_and_lock_abort_error_handler deadlock_handler;
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();
  uint counter;
  Table_ref *table;

  thd->in_lock_tables = true;

retry:

  if (open_tables(thd, &tables, &counter, 0, &lock_tables_prelocking_strategy))
    goto err;

  deadlock_handler.init();
  thd->push_internal_handler(&deadlock_handler);

  for (table = tables; table; table = table->next_global) {
    if (!table->is_placeholder()) {
      if (table->table->s->tmp_table) {
        /*
          We allow to change temporary tables even if they were locked for read
          by LOCK TABLES. To avoid a discrepancy between lock acquired at LOCK
          TABLES time and by the statement which is later executed under LOCK
          TABLES we ensure that for temporary tables we always request a write
          lock (such discrepancy can cause problems for the storage engine).
          We don't set Table_ref::lock_type in this case as this might
          result in extra warnings from THD::decide_logging_format() even though
          binary logging is totally irrelevant for LOCK TABLES.
        */
        table->table->reginfo.lock_type = TL_WRITE;
      } else if (table->lock_descriptor().type == TL_READ &&
                 !table->prelocking_placeholder &&
                 table->table->file->ha_table_flags() & HA_NO_READ_LOCAL_LOCK) {
        /*
          In case when LOCK TABLE ... READ LOCAL was issued for table with
          storage engine which doesn't support READ LOCAL option and doesn't
          use THR_LOCK locks we need to upgrade weak SR metadata lock acquired
          in open_tables() to stronger SRO metadata lock.
          This is not needed for tables used through stored routines or
          triggers as we always acquire SRO (or even stronger SNRW) metadata
          lock for them.
        */
        bool result = thd->mdl_context.upgrade_shared_lock(
            table->table->mdl_ticket, MDL_SHARED_READ_ONLY,
            thd->variables.lock_wait_timeout);

        if (deadlock_handler.need_reopen()) {
          /*
            Deadlock occurred during upgrade of metadata lock.
            Let us restart acquiring and opening tables for LOCK TABLES.
          */
          thd->pop_internal_handler();
          close_tables_for_reopen(thd, &tables, mdl_savepoint);
          if (open_temporary_tables(thd, tables)) goto err;
          goto retry;
        }

        if (result) {
          thd->pop_internal_handler();
          goto err;
        }
      }
    }
  }

  thd->pop_internal_handler();

  if (lock_tables(thd, tables, counter, 0) ||
      thd->locked_tables_list.init_locked_tables(thd))
    goto err;

  thd->in_lock_tables = false;

  return false;

err:
  thd->in_lock_tables = false;

  trans_rollback_stmt(thd);
  /*
    Need to end the current transaction, so the storage engine (InnoDB)
    can free its locks if LOCK TABLES locked some tables before finding
    that it can't lock a table in its list
  */
  trans_rollback(thd);
  /* Close tables and release metadata locks. */
  close_thread_tables(thd);
  assert(!thd->locked_tables_mode);
  thd->mdl_context.release_transactional_locks();
  return true;
}

/**
  This is a wrapper for MYSQL_BIN_LOG::gtid_end_transaction. For normal
  statements, the function gtid_end_transaction is called in the commit
  handler. However, if the statement is filtered out or not written to
  the binary log, the commit handler is not invoked. Therefore, this
  wrapper calls gtid_end_transaction in case the current statement is
  committing but was not written to the binary log.
  (The function gtid_end_transaction ensures that gtid-related
  end-of-transaction operations are performed; this includes
  generating an empty transaction and calling
  Gtid_state::update_gtids_impl.)

  @param thd Thread (session) context.
*/

static inline void binlog_gtid_end_transaction(THD *thd) {
  DBUG_TRACE;

  /*
    This performs end-of-transaction actions needed by GTIDs:
    in particular, it generates an empty transaction if
    needed (e.g., if the statement was filtered out).

    It is executed at the end of an implicitly or explicitly
    committing statement.

    In addition, it is executed after CREATE TEMPORARY TABLE
    or DROP TEMPORARY TABLE when they occur outside
    transactional context.  When enforce_gtid_consistency is
    enabled, these statements cannot occur in transactional
    context, and then they behave exactly as implicitly
    committing: they are written to the binary log
    immediately, not wrapped in BEGIN/COMMIT, and cannot be
    rolled back. However, they do not count as implicitly
    committing according to stmt_causes_implicit_commit(), so
    we need to add special cases in the condition below. Hence
    the clauses for SQLCOM_CREATE_TABLE and SQLCOM_DROP_TABLE.

    If enforce_gtid_consistency=off, CREATE TEMPORARY TABLE
    and DROP TEMPORARY TABLE can occur in the middle of a
    transaction.  Then they do not behave as DDL; they are
    written to the binary log inside BEGIN/COMMIT.

    (For base tables, SQLCOM_[CREATE|DROP]_TABLE match both
    the stmt_causes_implicit_commit(...) clause and the
    thd->lex->sql_command == SQLCOM_* clause; for temporary
    tables they match only thd->lex->sql_command == SQLCOM_*.)
  */
  if (thd->lex->sql_command == SQLCOM_COMMIT ||
      thd->lex->sql_command == SQLCOM_XA_PREPARE ||
      thd->lex->sql_command == SQLCOM_XA_COMMIT ||
      thd->lex->sql_command == SQLCOM_XA_ROLLBACK ||
      stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END) ||
      ((thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
        thd->lex->sql_command == SQLCOM_DROP_TABLE) &&
       !thd->in_multi_stmt_transaction_mode()))
    (void)mysql_bin_log.gtid_end_transaction(thd);
}

/**
  Execute command saved in thd and lex->sql_command.

  @param thd                       Thread handle
  @param first_level               whether invocation of the
  mysql_execute_command() is a top level query or sub query. At the highest
  level, first_level value is true. Stored procedures can execute sub queries.
  In such cases first_level (recursive mysql_execute_command() call) will be
  false.

  @todo this is workaround. right way will be move invalidating in
    the unlock procedure.
  @todo use check_change_password()

  @retval false       OK
  @retval true        Error
*/

int mysql_execute_command(THD *thd, bool first_level) {
  int res = false;
  LEX *const lex = thd->lex;
  /* first Query_block (have special meaning for many of non-SELECTcommands) */
  Query_block *const query_block = lex->query_block;
  /* first table of first Query_block */
  Table_ref *const first_table = query_block->get_table_list();
  /* list of all tables in query */
  Table_ref *all_tables;
  // keep GTID violation state in order to roll it back on statement failure
  bool gtid_consistency_violation_state = thd->has_gtid_consistency_violation;
  assert(query_block->master_query_expression() == lex->unit);
  DBUG_TRACE;
  /* EXPLAIN OTHER isn't explainable command, but can have describe flag. */
  assert(!lex->is_explain() || is_explainable_query(lex->sql_command) ||
         lex->sql_command == SQLCOM_EXPLAIN_OTHER);

  assert(!thd->m_transactional_ddl.inited() ||
         thd->in_active_multi_stmt_transaction());

  bool early_error_on_rep_command{false};

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_execute_command");

  /*
    If there is a CREATE TABLE...START TRANSACTION command which
    is not yet committed or rollbacked, then we should allow only
    BINLOG INSERT, COMMIT or ROLLBACK command.
    TODO: Should we really check name of table when we cable BINLOG INSERT ?
  */
  if (thd->m_transactional_ddl.inited() && lex->sql_command != SQLCOM_COMMIT &&
      lex->sql_command != SQLCOM_ROLLBACK &&
      lex->sql_command != SQLCOM_BINLOG_BASE64_EVENT) {
    my_error(ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION, MYF(0));
    binlog_gtid_end_transaction(thd);
    return 1;
  }

  thd->work_part_info = nullptr;

  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SUBQUERY_TO_DERIVED))
    lex->add_statement_options(OPTION_NO_CONST_TABLES);

  /*
    Each statement or replication event which might produce deadlock
    should handle transaction rollback on its own. So by the start of
    the next statement transaction rollback request should be fulfilled
    already.
  */
  assert(!thd->transaction_rollback_request || thd->in_sub_stmt);
  /*
    In many cases first table of main Query_block have special meaning =>
    check that it is first table in global list and relink it first in
    queries_tables list if it is necessary (we need such relinking only
    for queries with subqueries in select list, in this case tables of
    subqueries will go to global list first)

    all_tables will differ from first_table only if most upper Query_block
    do not contain tables.

    Because of above in place where should be at least one table in most
    outer Query_block we have following check:
    assert(first_table == all_tables);
    assert(first_table == all_tables && first_table != 0);
  */
  lex->first_lists_tables_same();
  /* should be assigned after making first tables same */
  all_tables = lex->query_tables;
  /* set context for commands which do not use setup_tables */
  query_block->context.resolve_in_table_list_only(
      query_block->get_table_list());

  thd->get_stmt_da()->reset_diagnostics_area();
  if ((thd->lex->keep_diagnostics != DA_KEEP_PARSE_ERROR) &&
      (thd->lex->keep_diagnostics != DA_KEEP_DIAGNOSTICS)) {
    /*
      No parse errors, and it's not a diagnostic statement:
      remove the sql conditions from the DA!
      For diagnostic statements we need to keep the conditions
      around so we can inspec them.
    */
    thd->get_stmt_da()->reset_condition_info(thd);
  }

  if (thd->resource_group_ctx()->m_warn != 0) {
    auto res_grp_name = thd->resource_group_ctx()->m_switch_resource_group_str;
    switch (thd->resource_group_ctx()->m_warn) {
      case WARN_RESOURCE_GROUP_UNSUPPORTED: {
        auto res_grp_mgr = resourcegroups::Resource_group_mgr::instance();
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_FEATURE_UNSUPPORTED,
                            ER_THD(thd, ER_FEATURE_UNSUPPORTED),
                            "Resource groups", res_grp_mgr->unsupport_reason());
        break;
      }
      case WARN_RESOURCE_GROUP_UNSUPPORTED_HINT:
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_WARN_UNSUPPORTED_HINT,
                            ER_THD(thd, ER_WARN_UNSUPPORTED_HINT),
                            "Subquery or Stored procedure or Trigger");
        break;
      case WARN_RESOURCE_GROUP_TYPE_MISMATCH: {
        ulonglong pfs_thread_id = 0;
        /*
          Resource group is unsupported with DISABLE_PSI_THREAD.
          The below #ifdef is required for compilation when DISABLE_PSI_THREAD
          is enabled.
        */
#ifdef HAVE_PSI_THREAD_INTERFACE
        pfs_thread_id = PSI_THREAD_CALL(get_current_thread_internal_id)();
#endif  // HAVE_PSI_THREAD_INTERFACE
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_RESOURCE_GROUP_BIND_FAILED,
                            ER_THD(thd, ER_RESOURCE_GROUP_BIND_FAILED),
                            res_grp_name, pfs_thread_id,
                            "System resource group can't be bound"
                            " with a session thread");
        break;
      }
      case WARN_RESOURCE_GROUP_NOT_EXISTS:
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_RESOURCE_GROUP_NOT_EXISTS,
            ER_THD(thd, ER_RESOURCE_GROUP_NOT_EXISTS), res_grp_name);
        break;
      case WARN_RESOURCE_GROUP_ACCESS_DENIED:
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_SPECIFIC_ACCESS_DENIED_ERROR,
                            ER_THD(thd, ER_SPECIFIC_ACCESS_DENIED_ERROR),
                            "SUPER OR RESOURCE_GROUP_ADMIN OR "
                            "RESOURCE_GROUP_USER");
    }
    thd->resource_group_ctx()->m_warn = 0;
    res_grp_name[0] = '\0';
  }

  if (unlikely(thd->get_protocol()->has_client_capability(CLIENT_NO_SCHEMA))) {
    push_warning(thd, ER_WARN_DEPRECATED_CLIENT_NO_SCHEMA_OPTION);
  }

  if (unlikely(thd->slave_thread)) {
    if (!check_database_filters(thd, thd->db().str, lex->sql_command)) {
      binlog_gtid_end_transaction(thd);
      return 0;
    }

    if (lex->sql_command == SQLCOM_DROP_TRIGGER) {
      /*
        When dropping a trigger, we need to load its table name
        before checking slave filter rules.
      */
      Table_ref *trigger_table = nullptr;
      (void)get_table_for_trigger(thd, lex->spname->m_db, lex->spname->m_name,
                                  true, &trigger_table);
      if (trigger_table != nullptr) {
        lex->add_to_query_tables(trigger_table);
        all_tables = trigger_table;
      } else {
        /*
          If table name cannot be loaded,
          it means the trigger does not exists possibly because
          CREATE TRIGGER was previously skipped for this trigger
          according to slave filtering rules.
          Returning success without producing any errors in this case.
        */
        binlog_gtid_end_transaction(thd);
        return 0;
      }

      // force searching in slave.cc:tables_ok()
      all_tables->updating = true;
    }

    /*
      For fix of BUG#37051, the master stores the table map for update
      in the Query_log_event, and the value is assigned to
      thd->table_map_for_update before executing the update
      query.

      If thd->table_map_for_update is set, then we are
      replicating from a new master, we can use this value to apply
      filter rules without opening all the tables. However If
      thd->table_map_for_update is not set, then we are
      replicating from an old master, so we just skip this and
      continue with the old method. And of course, the bug would still
      exist for old masters.
    */
    if (lex->sql_command == SQLCOM_UPDATE_MULTI && thd->table_map_for_update) {
      table_map table_map_for_update = thd->table_map_for_update;
      uint nr = 0;
      Table_ref *table;
      for (table = all_tables; table; table = table->next_global, nr++) {
        if (table_map_for_update & ((table_map)1 << nr))
          table->updating = true;
        else
          table->updating = false;
      }

      if (all_tables_not_ok(thd, all_tables)) {
        /* we warn the slave SQL thread */
        my_error(ER_REPLICA_IGNORED_TABLE, MYF(0));
        binlog_gtid_end_transaction(thd);
        return 0;
      }

      for (table = all_tables; table; table = table->next_global)
        table->updating = true;
    }

    /*
      Check if statement should be skipped because of slave filtering
      rules

      Exceptions are:
      - UPDATE MULTI: For this statement, we want to check the filtering
        rules later in the code
      - SET: we always execute it (Not that many SET commands exists in
        the binary log anyway -- only 4.1 masters write SET statements,
        in 5.0 there are no SET statements in the binary log)
      - DROP TEMPORARY TABLE IF EXISTS: we always execute it (otherwise we
        have stale files on slave caused by exclusion of one tmp table).
    */
    if (!(lex->sql_command == SQLCOM_UPDATE_MULTI) &&
        !(lex->sql_command == SQLCOM_SET_OPTION) &&
        !(lex->sql_command == SQLCOM_DROP_TABLE && lex->drop_temporary &&
          lex->drop_if_exists) &&
        all_tables_not_ok(thd, all_tables)) {
      /* we warn the slave SQL thread */
      my_error(ER_REPLICA_IGNORED_TABLE, MYF(0));
      binlog_gtid_end_transaction(thd);
      return 0;
    }
    /*
       Execute deferred events first
    */
    if (slave_execute_deferred_events(thd)) return -1;

    int ret = launch_hook_trans_begin(thd, all_tables);
    if (ret) {
      my_error(ret, MYF(0));
      return -1;
    }

  } else {
    int ret = launch_hook_trans_begin(thd, all_tables);
    if (ret) {
      my_error(ret, MYF(0));
      return -1;
    }

    /*
      When option readonly is set deny operations which change non-temporary
      tables. Except for the replication thread and the 'super' users.
    */
    if (deny_updates_if_read_only_option(thd, all_tables)) {
      err_readonly(thd);
      return -1;
    }
  } /* endif unlikely slave */

  thd->status_var.com_stat[lex->sql_command]++;

  Opt_trace_start ots(thd, all_tables, lex->sql_command, &lex->var_list,
                      thd->query().str, thd->query().length, nullptr,
                      thd->variables.character_set_client);

  Opt_trace_object trace_command(&thd->opt_trace);
  Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

  if (lex->m_sql_cmd && lex->m_sql_cmd->owner())
    lex->m_sql_cmd->owner()->trace_parameter_types(thd);

  assert(thd->get_transaction()->cannot_safely_rollback(
             Transaction_ctx::STMT) == false);

  switch (gtid_pre_statement_checks(thd)) {
    case GTID_STATEMENT_EXECUTE:
      break;
    case GTID_STATEMENT_CANCEL:
      return -1;
    case GTID_STATEMENT_SKIP:
      my_ok(thd);
      binlog_gtid_end_transaction(thd);
      return 0;
  }

  if (check_and_report_require_row_format_violation(thd) ||
      run_post_replication_filters_actions(thd))
    return -1;

  /*
    End a active transaction so that this command will have it's
    own transaction and will also sync the binary log. If a DDL is
    not run in it's own transaction it may simply never appear on
    the slave in case the outside transaction rolls back.
  */
  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN)) {
    /*
      Note that this should never happen inside of stored functions
      or triggers as all such statements prohibited there.
    */
    assert(!thd->in_sub_stmt);
    /* Statement transaction still should not be started. */
    assert(thd->get_transaction()->is_empty(Transaction_ctx::STMT));

    /*
      Implicit commit is not allowed with an active XA transaction.
      In this case we should not release metadata locks as the XA transaction
      will not be rolled back. Therefore we simply return here.
    */
    if (trans_check_state(thd)) return -1;

    /* Commit the normal transaction if one is active. */
    if (trans_commit_implicit(thd)) return -1;
    /* Release metadata locks acquired in this transaction. */
    thd->mdl_context.release_transactional_locks();
  }

  DEBUG_SYNC(thd, "after_implicit_pre_commit");

  if (gtid_pre_statement_post_implicit_commit_checks(thd)) return -1;

  if (mysql_audit_notify(thd,
                         first_level ? MYSQL_AUDIT_QUERY_START
                                     : MYSQL_AUDIT_QUERY_NESTED_START,
                         first_level ? "MYSQL_AUDIT_QUERY_START"
                                     : "MYSQL_AUDIT_QUERY_NESTED_START")) {
    return 1;
  }

#ifndef NDEBUG
  if (lex->sql_command != SQLCOM_SET_OPTION)
    DEBUG_SYNC(thd, "before_execute_sql_command");
#endif

  /*
    Start a new transaction if CREATE TABLE has START TRANSACTION clause.
    Disable binlog so that the BEGIN is not logged in binlog.
   */
  if (lex->create_info && lex->create_info->m_transactional_ddl &&
      !thd->slave_thread) {
    Disable_binlog_guard binlog_guard(thd);
    if (trans_begin(thd, MYSQL_START_TRANS_OPT_READ_WRITE)) return true;
  }

  /*
    For statements which need this, prevent InnoDB from automatically
    committing InnoDB transaction each time data-dictionary tables are
    closed after being updated.
  */
  Disable_autocommit_guard autocommit_guard(
      sqlcom_needs_autocommit_off(lex) && !thd->is_plugin_fake_ddl() ? thd
                                                                     : nullptr);

  /*
    Check if we are in a read-only transaction and we're trying to
    execute a statement which should always be disallowed in such cases.

    Note that this check is done after any implicit commits.
  */
  if (thd->tx_read_only &&
      (sql_command_flags[lex->sql_command] & CF_DISALLOW_IN_RO_TRANS)) {
    my_error(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, MYF(0));
    goto error;
  }

  /*
    Close tables open by HANDLERs before executing DDL statement
    which is going to affect those tables.

    This should happen before temporary tables are pre-opened as
    otherwise we will get errors about attempt to re-open tables
    if table to be changed is open through HANDLER.

    Note that even although this is done before any privilege
    checks there is no security problem here as closing open
    HANDLER doesn't require any privileges anyway.
  */
  if (sql_command_flags[lex->sql_command] & CF_HA_CLOSE)
    mysql_ha_rm_tables(thd, all_tables);

  /*
    Check that the command is allowed on the PROTOCOL_PLUGIN
  */
  if (thd->get_protocol()->type() == Protocol::PROTOCOL_PLUGIN &&
      !(sql_command_flags[lex->sql_command] & CF_ALLOW_PROTOCOL_PLUGIN)) {
    my_error(ER_PLUGGABLE_PROTOCOL_COMMAND_NOT_SUPPORTED, MYF(0));
    goto error;
  }

  /*
    Open all temporary tables referenced in statement.
    A session has all privileges for any temporary table that it has created,
    however a table must be opened in order to identify it as a temporary table.
  */
  if (sql_command_flags[lex->sql_command] & CF_PREOPEN_TMP_TABLES) {
    if (open_temporary_tables(thd, all_tables)) goto error;
  }

  // Save original info for EXPLAIN FOR CONNECTION
  if (!thd->in_sub_stmt)
    thd->query_plan.set_query_plan(lex->sql_command, lex,
                                   !thd->stmt_arena->is_regular());

  /* Update system variables specified in SET_VAR hints. */
  if (lex->opt_hints_global && lex->opt_hints_global->sys_var_hint)
    lex->opt_hints_global->sys_var_hint->update_vars(thd);

  /* Check if the statement fulfill the requirements on ACL CACHE */
  if (!command_satisfy_acl_cache_requirement(lex->sql_command)) {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    goto error;
  }

  DBUG_EXECUTE_IF(
      "force_rollback_in_replica_on_transactional_ddl_commit",
      if (thd->m_transactional_ddl.inited() &&
          thd->lex->sql_command == SQLCOM_COMMIT) {
        lex->sql_command = SQLCOM_ROLLBACK;
      });

  /*
    We do not flag "is DML" (TX_STMT_DML) here as replication expects us to
    test for LOCK TABLE etc. first. To rephrase, we try not to set TX_STMT_DML
    until we have the MDL, and LOCK TABLE could massively delay this.
  */

  switch (lex->sql_command) {
    case SQLCOM_PREPARE: {
      mysql_sql_stmt_prepare(thd);
      break;
    }
    case SQLCOM_EXECUTE: {
      mysql_sql_stmt_execute(thd);
      break;
    }
    case SQLCOM_DEALLOCATE_PREPARE: {
      mysql_sql_stmt_close(thd);
      break;
    }

    case SQLCOM_EMPTY_QUERY:
      my_ok(thd);
      break;

    case SQLCOM_HELP:
      res = mysqld_help(thd, lex->help_arg);
      break;

    case SQLCOM_PURGE: {
      Security_context *sctx = thd->security_context();
      if (!sctx->check_access(SUPER_ACL) &&
          !sctx->has_global_grant(STRING_WITH_LEN("BINLOG_ADMIN")).first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or BINLOG_ADMIN");
        goto error;
      }
      /* PURGE MASTER LOGS TO 'file' */
      res = purge_source_logs_to_file(thd, lex->to_log);
      break;
    }
    case SQLCOM_PURGE_BEFORE: {
      Item *it;
      Security_context *sctx = thd->security_context();
      if (!sctx->check_access(SUPER_ACL) &&
          !sctx->has_global_grant(STRING_WITH_LEN("BINLOG_ADMIN")).first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or BINLOG_ADMIN");
        goto error;
      }
      /* PURGE MASTER LOGS BEFORE 'data' */
      it = lex->purge_value_list.head();
      if ((!it->fixed && it->fix_fields(lex->thd, &it)) || it->check_cols(1)) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "PURGE LOGS BEFORE");
        goto error;
      }
      it = new Item_func_unix_timestamp(it);
      /*
        it is OK only emulate fix_fieds, because we need only
        value of constant
      */
      it->quick_fix_field();
      time_t purge_time = static_cast<time_t>(it->val_int());
      if (thd->is_error()) goto error;
      res = purge_source_logs_before_date(thd, purge_time);
      break;
    }
    case SQLCOM_CHANGE_MASTER: {
      Security_context *sctx = thd->security_context();
      if (!sctx->check_access(SUPER_ACL) &&
          !sctx->has_global_grant(STRING_WITH_LEN("REPLICATION_SLAVE_ADMIN"))
               .first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or REPLICATION_SLAVE_ADMIN");
        goto error;
      }
      res = change_master_cmd(thd);
      break;
    }
    case SQLCOM_START_GROUP_REPLICATION: {
      Security_context *sctx = thd->security_context();
      if (!sctx->check_access(SUPER_ACL) &&
          !sctx->has_global_grant(STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"))
               .first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or GROUP_REPLICATION_ADMIN");
        goto error;
      }
      if (lex->slave_connection.password && !lex->slave_connection.user) {
        my_error(ER_GROUP_REPLICATION_USER_MANDATORY_MSG, MYF(0));
        goto error;
      }

      /*
        If the client thread has locked tables, a deadlock is possible.
        Assume that
        - the client thread does LOCK TABLE t READ.
        - then the client thread does START GROUP_REPLICATION.
             -try to make the server in super ready only mode
             -acquire MDL lock ownership which will be waiting for
              LOCK on table t to be released.
        To prevent that, refuse START GROUP_REPLICATION if the
        client thread has locked tables
      */
      if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction() ||
          thd->in_sub_stmt) {
        my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
        goto error;
      }

      if (thd->variables.gtid_next.type == ASSIGNED_GTID &&
          thd->owned_gtid.sidno > 0) {
        my_error(ER_CANT_EXECUTE_COMMAND_WITH_ASSIGNED_GTID_NEXT, MYF(0));
        early_error_on_rep_command = true;
        goto error;
      }

      if (Clone_handler::is_provisioning()) {
        my_error(ER_GROUP_REPLICATION_COMMAND_FAILURE, MYF(0),
                 "START GROUP_REPLICATION",
                 "This server is being provisioned by CLONE INSTANCE, "
                 "please wait until it is complete.");
        goto error;
      }

      char *error_message = nullptr;
      res = group_replication_start(&error_message, thd);

      // To reduce server dependency, server errors are not used here
      switch (res) {
        case 1:  // GROUP_REPLICATION_CONFIGURATION_ERROR
          my_error(ER_GROUP_REPLICATION_CONFIGURATION, MYF(0));
          goto error;
        case 2:  // GROUP_REPLICATION_ALREADY_RUNNING
          my_error(ER_GROUP_REPLICATION_RUNNING, MYF(0));
          goto error;
        case 3:  // GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR
          my_error(ER_GROUP_REPLICATION_APPLIER_INIT_ERROR, MYF(0));
          goto error;
        case 4:  // GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR
          my_error(ER_GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR,
                   MYF(0));
          goto error;
        case 5:  // GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR
          my_error(ER_GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR, MYF(0));
          goto error;
        case 7:  // GROUP_REPLICATION_MAX_GROUP_SIZE
          my_error(ER_GROUP_REPLICATION_MAX_GROUP_SIZE, MYF(0));
          goto error;
        case 8:  // GROUP_REPLICATION_COMMAND_FAILURE
          if (error_message == nullptr) {
            my_error(ER_GROUP_REPLICATION_COMMAND_FAILURE, MYF(0),
                     "START GROUP_REPLICATION",
                     "Please check error log for additional details.");
          } else {
            my_error(ER_GROUP_REPLICATION_COMMAND_FAILURE, MYF(0),
                     "START GROUP_REPLICATION", error_message);
            my_free(error_message);
          }
          goto error;
        case 9:  // GROUP_REPLICATION_SERVICE_MESSAGE_INIT_FAILURE
          my_error(ER_GRP_RPL_MESSAGE_SERVICE_INIT_FAILURE, MYF(0));
          goto error;
        case 10:  // GROUP_REPLICATION_RECOVERY_CHANNEL_STILL_RUNNING
          my_error(ER_GRP_RPL_RECOVERY_CHANNEL_STILL_RUNNING, MYF(0));
          goto error;
      }
      my_ok(thd);
      res = 0;
      break;
    }

    case SQLCOM_STOP_GROUP_REPLICATION: {
      Security_context *sctx = thd->security_context();
      if (!sctx->check_access(SUPER_ACL) &&
          !sctx->has_global_grant(STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"))
               .first) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                 "SUPER or GROUP_REPLICATION_ADMIN");
        goto error;
      }

      /*
        Please see explanation @SQLCOM_SLAVE_STOP case
        to know the reason for thd->locked_tables_mode in
        the below if condition.
      */
      if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction() ||
          thd->in_sub_stmt) {
        my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
        goto error;
      }

      if (thd->variables.gtid_next.type == ASSIGNED_GTID &&
          thd->owned_gtid.sidno > 0) {
        my_error(ER_CANT_EXECUTE_COMMAND_WITH_ASSIGNED_GTID_NEXT, MYF(0));
        early_error_on_rep_command = true;
        goto error;
      }

      char *error_message = nullptr;
      res = group_replication_stop(&error_message);
      if (res == 1)  // GROUP_REPLICATION_CONFIGURATION_ERROR
      {
        my_error(ER_GROUP_REPLICATION_CONFIGURATION, MYF(0));
        goto error;
      }
      if (res == 6)  // GROUP_REPLICATION_APPLIER_THREAD_TIMEOUT
      {
        my_error(ER_GROUP_REPLICATION_STOP_APPLIER_THREAD_TIMEOUT, MYF(0));
        goto error;
      }
      if (res == 8)  // GROUP_REPLICATION_COMMAND_FAILURE
      {
        if (error_message == nullptr) {
          my_error(ER_GROUP_REPLICATION_COMMAND_FAILURE, MYF(0),
                   "STOP GROUP_REPLICATION",
                   "Please check error log for additonal details.");
        } else {
          my_error(ER_GROUP_REPLICATION_COMMAND_FAILURE, MYF(0),
                   "STOP GROUP_REPLICATION", error_message);
          my_free(error_message);
        }
        goto error;
      }
      if (res == 11)  // GROUP_REPLICATION_STOP_WITH_RECOVERY_TIMEOUT
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_GRP_RPL_RECOVERY_CHANNEL_STILL_RUNNING,
                     ER_THD(thd, ER_GRP_RPL_RECOVERY_CHANNEL_STILL_RUNNING));

      // Allow the command to commit any underlying transaction
      lex->set_was_replication_command_executed();
      thd->set_skip_readonly_check();
      my_ok(thd);
      res = 0;
      break;
    }

    case SQLCOM_SLAVE_START: {
      res = start_slave_cmd(thd);
      break;
    }
    case SQLCOM_SLAVE_STOP: {
      /*
        If the client thread has locked tables, a deadlock is possible.
        Assume that
        - the client thread does LOCK TABLE t READ.
        - then the master updates t.
        - then the SQL slave thread wants to update t,
          so it waits for the client thread because t is locked by it.
        - then the client thread does SLAVE STOP.
          SLAVE STOP waits for the SQL slave thread to terminate its
          update t, which waits for the client thread because t is locked by it.
        To prevent that, refuse SLAVE STOP if the
        client thread has locked tables
      */
      if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction() ||
          thd->global_read_lock.is_acquired()) {
        my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
        goto error;
      }

      res = stop_slave_cmd(thd);
      break;
    }
    case SQLCOM_RENAME_TABLE: {
      assert(first_table == all_tables && first_table != nullptr);
      Table_ref *table;
      for (table = first_table; table; table = table->next_local->next_local) {
        if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
                         &table->grant.privilege, &table->grant.m_internal,
                         false, false) ||
            check_access(thd, INSERT_ACL | CREATE_ACL, table->next_local->db,
                         &table->next_local->grant.privilege,
                         &table->next_local->grant.m_internal, false, false))
          goto error;

        Table_ref old_list = table[0];
        Table_ref new_list = table->next_local[0];
        /*
          It's not clear what the above assignments actually want to
          accomplish. What we do know is that they do *not* want to copy the MDL
          requests, so we overwrite them with uninitialized request.
        */
        old_list.mdl_request = MDL_request();
        new_list.mdl_request = MDL_request();

        if (check_grant(thd, ALTER_ACL | DROP_ACL, &old_list, false, 1,
                        false) ||
            (!test_all_bits(table->next_local->grant.privilege,
                            INSERT_ACL | CREATE_ACL) &&
             check_grant(thd, INSERT_ACL | CREATE_ACL, &new_list, false, 1,
                         false)))
          goto error;
      }

      if (mysql_rename_tables(thd, first_table)) goto error;
      break;
    }
    case SQLCOM_CHECKSUM: {
      assert(first_table == all_tables && first_table != nullptr);
      if (check_table_access(thd, SELECT_ACL, all_tables, false, UINT_MAX,
                             false))
        goto error; /* purecov: inspected */

      res = mysql_checksum_table(thd, first_table, &lex->check_opt);
      break;
    }
    case SQLCOM_REPLACE:
    case SQLCOM_INSERT:
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_DELETE:
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_UPDATE:
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_CREATE_TABLE:
    case SQLCOM_CREATE_INDEX:
    case SQLCOM_DROP_INDEX:
    case SQLCOM_ASSIGN_TO_KEYCACHE:
    case SQLCOM_PRELOAD_KEYS:
    case SQLCOM_LOAD: {
      assert(first_table == all_tables && first_table != nullptr);
      assert(lex->m_sql_cmd != nullptr);
      res = lex->m_sql_cmd->execute(thd);
      break;
    }
    case SQLCOM_DROP_TABLE: {
      assert(first_table == all_tables && first_table != nullptr);
      if (!lex->drop_temporary) {
        if (check_table_access(thd, DROP_ACL, all_tables, false, UINT_MAX,
                               false))
          goto error; /* purecov: inspected */
      }
      /* DDL and binlog write order are protected by metadata locks. */
      res = mysql_rm_table(thd, first_table, lex->drop_if_exists,
                           lex->drop_temporary);
      /* when dropping temporary tables if @@session_track_state_change is ON
         then send the boolean tracker in the OK packet */
      if (!res && lex->drop_temporary) {
        if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
                ->is_enabled())
          thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
              ->mark_as_changed(thd, {});
      }
    } break;
    case SQLCOM_CHANGE_DB: {
      const LEX_CSTRING db_str = {query_block->db, strlen(query_block->db)};

      if (!mysql_change_db(thd, db_str, false)) my_ok(thd);

      break;
    }

    case SQLCOM_SET_OPTION: {
      List<set_var_base> *lex_var_list = &lex->var_list;

      if (check_table_access(thd, SELECT_ACL, all_tables, false, UINT_MAX,
                             false))
        goto error;

      if (open_tables_for_query(thd, all_tables, false)) goto error;
      if (!thd->stmt_arena->is_regular()) {
        lex->restore_cmd_properties();
        bind_fields(thd->stmt_arena->item_list());
        if (all_tables != nullptr &&
            !thd->stmt_arena->is_stmt_prepare_or_first_stmt_execute() &&
            query_block->check_privileges_for_subqueries(thd))
          return true;
      }
      if (!(res = sql_set_variables(thd, lex_var_list, true)))
        my_ok(thd);
      else {
        /*
          We encountered some sort of error, but no message was sent.
          Send something semi-generic here since we don't know which
          assignment in the list caused the error.
        */
        if (!thd->is_error()) my_error(ER_WRONG_ARGUMENTS, MYF(0), "SET");
        goto error;
      }

#ifndef NDEBUG
      /*
        Makes server crash when executing SET SESSION debug = 'd,crash_now';
        See mysql-test/include/dbug_crash[_all].inc
      */
      const bool force_server_crash_dbug = false;
      DBUG_EXECUTE_IF("crash_now", assert(force_server_crash_dbug););
      DBUG_EXECUTE_IF("crash_now_safe", DBUG_SUICIDE(););
#endif

      break;
    }
    case SQLCOM_SET_PASSWORD: {
      List<set_var_base> *lex_var_list = &lex->var_list;

      assert(lex_var_list->elements == 1);
      assert(all_tables == nullptr);
      Userhostpassword_list generated_passwords;
      if (!(res = sql_set_variables(thd, lex_var_list, false))) {
        List_iterator_fast<set_var_base> it(*lex_var_list);
        set_var_base *var;
        while ((var = it++)) {
          set_var_password *setpasswd = static_cast<set_var_password *>(var);
          if (setpasswd->has_generated_password()) {
            const LEX_USER *user = setpasswd->get_user();
            random_password_info p{
                std::string(user->user.str, user->user.length),
                std::string(user->host.str, user->host.length),
                setpasswd->get_generated_password(), 1};
            generated_passwords.push_back(p);
          }
        }
        if (generated_passwords.size() > 0) {
          if (send_password_result_set(thd, generated_passwords)) goto error;
        }  // end if generated_passwords
        if (generated_passwords.size() == 0) my_ok(thd);
      } else {
        // We encountered some sort of error, but no message was sent.
        if (!thd->is_error())
          my_error(ER_WRONG_ARGUMENTS, MYF(0), "SET PASSWORD");
        goto error;
      }

      break;
    }

    case SQLCOM_UNLOCK_TABLES:
      /*
        It is critical for mysqldump --single-transaction --source-data that
        UNLOCK TABLES does not implicitly commit a connection which has only
        done FLUSH TABLES WITH READ LOCK + BEGIN. If this assumption becomes
        false, mysqldump will not work.
      */
      if (thd->variables.option_bits & OPTION_TABLE_LOCK) {
        /*
          Can we commit safely? If not, return to avoid releasing
          transactional metadata locks.
        */
        if (trans_check_state(thd)) return -1;
        res = trans_commit_implicit(thd);
        thd->locked_tables_list.unlock_locked_tables(thd);
        thd->mdl_context.release_transactional_locks();
        thd->variables.option_bits &= ~(OPTION_TABLE_LOCK);
      }
      if (thd->global_read_lock.is_acquired())
        thd->global_read_lock.unlock_global_read_lock(thd);
      if (res) goto error;
      my_ok(thd);
      break;
    case SQLCOM_LOCK_TABLES:
      /*
        Can we commit safely? If not, return to avoid releasing
        transactional metadata locks.
      */
      if (trans_check_state(thd)) return -1;
      /* We must end the transaction first, regardless of anything */
      res = trans_commit_implicit(thd);
      thd->locked_tables_list.unlock_locked_tables(thd);
      /* Release transactional metadata locks. */
      thd->mdl_context.release_transactional_locks();
      if (res) goto error;

      /*
        Here we have to pre-open temporary tables for LOCK TABLES.

        CF_PREOPEN_TMP_TABLES is not set for this SQL statement simply
        because LOCK TABLES calls close_thread_tables() as a first thing
        (it's called from unlock_locked_tables() above). So even if
        CF_PREOPEN_TMP_TABLES was set and the tables would be pre-opened
        in a usual way, they would have been closed.
      */

      if (open_temporary_tables(thd, all_tables)) goto error;

      if (lock_tables_precheck(thd, all_tables)) goto error;

      thd->variables.option_bits |= OPTION_TABLE_LOCK;

      res = lock_tables_open_and_lock_tables(thd, all_tables);

      if (res) {
        thd->variables.option_bits &= ~(OPTION_TABLE_LOCK);
      } else {
        my_ok(thd);
      }
      break;

    case SQLCOM_IMPORT:
      res = lex->m_sql_cmd->execute(thd);
      break;
    case SQLCOM_CREATE_DB: {
      const char *alias;
      if (!(alias = thd->strmake(lex->name.str, lex->name.length)) ||
          (check_and_convert_db_name(&lex->name, false) !=
           Ident_name_check::OK))
        break;
      if (check_access(thd, CREATE_ACL, lex->name.str, nullptr, nullptr, true,
                       false))
        break;
      /*
        As mysql_create_db() may modify HA_CREATE_INFO structure passed to
        it, we need to use a copy of LEX::create_info to make execution
        prepared statement- safe.
      */
      HA_CREATE_INFO create_info(*lex->create_info);
      res = mysql_create_db(
          thd, (lower_case_table_names == 2 ? alias : lex->name.str),
          &create_info);
      break;
    }
    case SQLCOM_DROP_DB: {
      if (check_and_convert_db_name(&lex->name, false) != Ident_name_check::OK)
        break;
      if (check_access(thd, DROP_ACL, lex->name.str, nullptr, nullptr, true,
                       false))
        break;
      res = mysql_rm_db(thd, to_lex_cstring(lex->name), lex->drop_if_exists);
      break;
    }
    case SQLCOM_ALTER_DB: {
      if (check_and_convert_db_name(&lex->name, false) != Ident_name_check::OK)
        break;
      if (check_access(thd, ALTER_ACL, lex->name.str, nullptr, nullptr, true,
                       false))
        break;
      /*
        As mysql_alter_db() may modify HA_CREATE_INFO structure passed to
        it, we need to use a copy of LEX::create_info to make execution
        prepared statement- safe.
      */
      HA_CREATE_INFO create_info(*lex->create_info);
      res = mysql_alter_db(thd, lex->name.str, &create_info);
      break;
    }
    case SQLCOM_CREATE_EVENT:
    case SQLCOM_ALTER_EVENT:
      do {
        assert(lex->event_parse_data);
        if (lex->table_or_sp_used()) {
          my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                   "Usage of subqueries or stored "
                   "function calls as part of this statement");
          break;
        }

        // Use the hypergraph optimizer if it's enabled.
        lex->using_hypergraph_optimizer =
            thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);

        res = sp_process_definer(thd);
        if (res) break;

        switch (lex->sql_command) {
          case SQLCOM_CREATE_EVENT: {
            bool if_not_exists =
                (lex->create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS);
            res =
                Events::create_event(thd, lex->event_parse_data, if_not_exists);
            break;
          }
          case SQLCOM_ALTER_EVENT: {
            LEX_CSTRING name_lex_str = NULL_CSTR;
            if (lex->spname) {
              name_lex_str.str = lex->spname->m_name.str;
              name_lex_str.length = lex->spname->m_name.length;
            }

            res =
                Events::update_event(thd, lex->event_parse_data,
                                     lex->spname ? &lex->spname->m_db : nullptr,
                                     lex->spname ? &name_lex_str : nullptr);
            break;
          }
          default:
            assert(0);
        }
        DBUG_PRINT("info", ("DDL error code=%d", res));
        if (!res && !thd->killed) my_ok(thd);

      } while (false);
      /* Don't do it, if we are inside a SP */
      if (!thd->sp_runtime_ctx) {
        sp_head::destroy(lex->sphead);
        lex->sphead = nullptr;
      }
      /* lex->cleanup() is called outside, no need to call it here */
      break;
    case SQLCOM_DROP_EVENT: {
      if (!(res = Events::drop_event(thd, lex->spname->m_db,
                                     to_lex_cstring(lex->spname->m_name),
                                     lex->drop_if_exists)))
        my_ok(thd);
      break;
    }
    case SQLCOM_CREATE_FUNCTION:  // UDF function
    {
      if (check_access(thd, INSERT_ACL, "mysql", nullptr, nullptr, true, false))
        break;
      if (!(res = mysql_create_function(
                thd, &lex->udf,
                lex->create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)))
        my_ok(thd);
      break;
    }
    case SQLCOM_CREATE_USER: {
      if (check_access(thd, INSERT_ACL, "mysql", nullptr, nullptr, true,
                       true) &&
          check_global_access(thd, CREATE_USER_ACL))
        break;
      /* Conditionally writes to binlog */
      HA_CREATE_INFO create_info(*lex->create_info);
      if (!(res = mysql_create_user(
                thd, lex->users_list,
                create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS, false))) {
        // OK or result set was already sent.
      }

      break;
    }
    case SQLCOM_DROP_USER: {
      if (check_access(thd, DELETE_ACL, "mysql", nullptr, nullptr, true,
                       true) &&
          check_global_access(thd, CREATE_USER_ACL))
        break;
      /* Conditionally writes to binlog */
      if (!(res = mysql_drop_user(thd, lex->users_list, lex->drop_if_exists,
                                  false)))
        my_ok(thd);

      break;
    }
    case SQLCOM_RENAME_USER: {
      if (check_access(thd, UPDATE_ACL, "mysql", nullptr, nullptr, true,
                       true) &&
          check_global_access(thd, CREATE_USER_ACL))
        break;
      /* Conditionally writes to binlog */
      if (!(res = mysql_rename_user(thd, lex->users_list))) my_ok(thd);
      break;
    }
    case SQLCOM_REVOKE_ALL: {
      if (check_access(thd, UPDATE_ACL, "mysql", nullptr, nullptr, true,
                       true) &&
          check_global_access(thd, CREATE_USER_ACL))
        break;

      /* Replicate current user as grantor */
      thd->binlog_invoker();

      /* Conditionally writes to binlog */
      if (!(res = mysql_revoke_all(thd, lex->users_list))) my_ok(thd);
      break;
    }
    case SQLCOM_REVOKE:
    case SQLCOM_GRANT: {
      /* GRANT ... AS preliminery checks */
      if (lex->grant_as.grant_as_used) {
        if ((first_table || query_block->db)) {
          my_error(ER_UNSUPPORTED_USE_OF_GRANT_AS, MYF(0));
          goto error;
        }
      }
      /*
        Skip access check if we're granting a proxy
      */
      if (lex->type != TYPE_ENUM_PROXY) {
        /*
          If there are static grants in the GRANT statement or there are no
          dynamic privileges we perform check_access on GRANT_OPTION based on
          static global privilege level and set the DA accordingly.
        */
        if (lex->grant > 0 || lex->dynamic_privileges.elements == 0) {
          /*
            check_access sets DA error message based on GRANT arguments.
          */
          if (check_access(
                  thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
                  first_table ? first_table->db : query_block->db,
                  first_table ? &first_table->grant.privilege : nullptr,
                  first_table ? &first_table->grant.m_internal : nullptr,
                  first_table ? false : true, false)) {
            goto error;
          }
        }
        /*
          ..else we still call check_access to load internal structures, but
          defer checking of global dynamic GRANT_OPTION to mysql_grant. We still
          ignore checks if this was a grant of a proxy.
        */
        else {
          /*
            check_access will load grant.privilege and grant.m_internal with
            values which are used later during column privilege checking. The
            return value isn't interesting as we'll check for dynamic global
            privileges later.
          */
          check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
                       first_table ? first_table->db : query_block->db,
                       first_table ? &first_table->grant.privilege : nullptr,
                       first_table ? &first_table->grant.m_internal : nullptr,
                       first_table ? false : true, true);
        }
      }

      /* Replicate current user as grantor */
      thd->binlog_invoker();

      if (thd->security_context()->user().str)  // If not replication
      {
        LEX_USER *user, *tmp_user;
        bool first_user = true;

        List_iterator<LEX_USER> user_list(lex->users_list);
        while ((tmp_user = user_list++)) {
          if (!(user = get_current_user(thd, tmp_user))) goto error;
          if (specialflag & SPECIAL_NO_RESOLVE &&
              hostname_requires_resolving(user->host.str))
            push_warning(thd, Sql_condition::SL_WARNING,
                         ER_WARN_HOSTNAME_WONT_WORK,
                         ER_THD(thd, ER_WARN_HOSTNAME_WONT_WORK));
          // Are we trying to change a password of another user
          assert(user->host.str != nullptr);

          /*
            GRANT/REVOKE PROXY has the target user as a first entry in the list.
           */
          if (lex->type == TYPE_ENUM_PROXY && first_user) {
            first_user = false;
            if (acl_check_proxy_grant_access(thd, user->host.str,
                                             user->user.str,
                                             lex->grant & GRANT_ACL))
              goto error;
          }
        }
      }
      if (first_table) {
        if (lex->dynamic_privileges.elements > 0) {
          if (thd->lex->grant_if_exists) {
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_ILLEGAL_PRIVILEGE_LEVEL,
                                ER_THD(thd, ER_ILLEGAL_PRIVILEGE_LEVEL),
                                all_tables->table_name);
          } else {
            my_error(ER_ILLEGAL_PRIVILEGE_LEVEL, MYF(0),
                     all_tables->table_name);
            goto error;
          }
        }
        if (lex->type == TYPE_ENUM_PROCEDURE ||
            lex->type == TYPE_ENUM_FUNCTION) {
          uint grants = lex->all_privileges
                            ? (PROC_OP_ACLS) | (lex->grant & GRANT_ACL)
                            : lex->grant;
          if (check_grant_routine(thd, grants | GRANT_ACL, all_tables,
                                  lex->type == TYPE_ENUM_PROCEDURE, false))
            goto error;
          /* Conditionally writes to binlog */
          res = mysql_routine_grant(
              thd, all_tables, lex->type == TYPE_ENUM_PROCEDURE,
              lex->users_list, grants, lex->sql_command == SQLCOM_REVOKE, true);
          if (!res) my_ok(thd);
        } else {
          if (check_grant(thd, (lex->grant | lex->grant_tot_col | GRANT_ACL),
                          all_tables, false, UINT_MAX, false))
            goto error;
          /* Conditionally writes to binlog */
          res =
              mysql_table_grant(thd, all_tables, lex->users_list, lex->columns,
                                lex->grant, lex->sql_command == SQLCOM_REVOKE);
        }
      } else {
        if (lex->columns.elements ||
            (lex->type && lex->type != TYPE_ENUM_PROXY)) {
          my_error(ER_ILLEGAL_GRANT_FOR_TABLE, MYF(0));
          goto error;
        } else {
          /* Dynamic privileges are allowed only for global grants */
          if (query_block->db && lex->dynamic_privileges.elements > 0) {
            String privs;
            bool comma = false;
            for (const LEX_CSTRING &priv : lex->dynamic_privileges) {
              if (comma) privs.append(",");
              privs.append(priv.str, priv.length);
              comma = true;
            }
            if (thd->lex->grant_if_exists) {
              push_warning_printf(
                  thd, Sql_condition::SL_WARNING, ER_ILLEGAL_PRIVILEGE_LEVEL,
                  ER_THD(thd, ER_ILLEGAL_PRIVILEGE_LEVEL), privs.c_ptr());
            } else {
              my_error(ER_ILLEGAL_PRIVILEGE_LEVEL, MYF(0), privs.c_ptr());
              goto error;
            }
          }
          /* Conditionally writes to binlog */
          res = mysql_grant(
              thd, query_block->db, lex->users_list, lex->grant,
              lex->sql_command == SQLCOM_REVOKE, lex->type == TYPE_ENUM_PROXY,
              lex->dynamic_privileges, lex->all_privileges, &lex->grant_as);
        }
      }
      break;
    }
    case SQLCOM_RESET:
      /*
        RESET commands are never written to the binary log, so we have to
        initialize this variable because RESET shares the same code as FLUSH
      */
      lex->no_write_to_binlog = true;
      if ((lex->type & REFRESH_PERSIST) && (lex->option_type == OPT_PERSIST)) {
        Persisted_variables_cache *pv =
            Persisted_variables_cache::get_instance();
        if (pv)
          if (pv->reset_persisted_variables(thd, lex->name.str,
                                            lex->drop_if_exists))
            goto error;
        my_ok(thd);
        break;
      }
      [[fallthrough]];
    case SQLCOM_FLUSH: {
      int write_to_binlog;
      if (is_reload_request_denied(thd, lex->type)) goto error;

      if (first_table && lex->type & REFRESH_READ_LOCK) {
        /* Check table-level privileges. */
        if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                               false, UINT_MAX, false))
          goto error;
        if (flush_tables_with_read_lock(thd, all_tables)) goto error;
        my_ok(thd);
        break;
      } else if (first_table && lex->type & REFRESH_FOR_EXPORT) {
        /* Check table-level privileges. */
        if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                               false, UINT_MAX, false))
          goto error;
        if (flush_tables_for_export(thd, all_tables)) goto error;
        my_ok(thd);
        break;
      }

      /*
        handle_reload_request() will tell us if we are allowed to write to the
        binlog or not.
      */
      if (!handle_reload_request(thd, lex->type, first_table,
                                 &write_to_binlog)) {
        /*
          We WANT to write and we CAN write.
          ! we write after unlocking the table.
        */
        /*
          Presumably, RESET and binlog writing doesn't require synchronization
        */

        if (write_to_binlog > 0)  // we should write
        {
          if (!lex->no_write_to_binlog)
            res = write_bin_log(thd, false, thd->query().str,
                                thd->query().length);
        } else if (write_to_binlog < 0) {
          /*
             We should not write, but rather report error because
             handle_reload_request binlog interactions failed
           */
          res = 1;
        }

        if (!res) my_ok(thd);
      }

      break;
    }
    case SQLCOM_KILL: {
      Item *it = lex->kill_value_list.head();

      if (lex->table_or_sp_used()) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "Usage of subqueries or stored "
                 "function calls as part of this statement");
        goto error;
      }

      if ((!it->fixed && it->fix_fields(lex->thd, &it)) || it->check_cols(1)) {
        my_error(ER_SET_CONSTANTS_ONLY, MYF(0));
        goto error;
      }

      my_thread_id thread_id = static_cast<my_thread_id>(it->val_int());
      if (thd->is_error()) goto error;

      sql_kill(thd, thread_id, lex->type & ONLY_KILL_QUERY);
      break;
    }
    case SQLCOM_SHOW_CREATE_USER: {
      LEX_USER *show_user = get_current_user(thd, lex->grant_user);
      Security_context *sctx = thd->security_context();
      bool are_both_users_same =
          !strcmp(sctx->priv_user().str, show_user->user.str) &&
          !my_strcasecmp(system_charset_info, show_user->host.str,
                         sctx->priv_host().str);
      if (are_both_users_same || !check_access(thd, SELECT_ACL, "mysql",
                                               nullptr, nullptr, true, false))
        res = mysql_show_create_user(thd, show_user, are_both_users_same);
      break;
    }
    case SQLCOM_BEGIN:
      if (trans_begin(thd, lex->start_transaction_opt)) goto error;
      my_ok(thd);
      break;
    case SQLCOM_COMMIT: {
      assert(thd->lock == nullptr ||
             thd->locked_tables_mode == LTM_LOCK_TABLES);
      bool tx_chain =
          (lex->tx_chain == TVL_YES ||
           (thd->variables.completion_type == 1 && lex->tx_chain != TVL_NO));
      bool tx_release =
          (lex->tx_release == TVL_YES ||
           (thd->variables.completion_type == 2 && lex->tx_release != TVL_NO));
      if (trans_commit(thd)) goto error;
      thd->mdl_context.release_transactional_locks();
      /* Begin transaction with the same isolation level. */
      if (tx_chain) {
        if (trans_begin(thd)) goto error;
      } else {
        /* Reset the isolation level and access mode if no chaining
         * transaction.*/
        trans_reset_one_shot_chistics(thd);
      }
      /* Disconnect the current client connection. */
      if (tx_release) thd->killed = THD::KILL_CONNECTION;
      my_ok(thd);
      break;
    }
    case SQLCOM_ROLLBACK: {
      assert(thd->lock == nullptr ||
             thd->locked_tables_mode == LTM_LOCK_TABLES);
      bool tx_chain =
          (lex->tx_chain == TVL_YES ||
           (thd->variables.completion_type == 1 && lex->tx_chain != TVL_NO));
      bool tx_release =
          (lex->tx_release == TVL_YES ||
           (thd->variables.completion_type == 2 && lex->tx_release != TVL_NO));
      if (trans_rollback(thd)) goto error;
      thd->mdl_context.release_transactional_locks();
      /* Begin transaction with the same isolation level. */
      if (tx_chain) {
        if (trans_begin(thd)) goto error;
      } else {
        /* Reset the isolation level and access mode if no chaining
         * transaction.*/
        trans_reset_one_shot_chistics(thd);
      }
      /* Disconnect the current client connection. */
      if (tx_release) thd->killed = THD::KILL_CONNECTION;
      my_ok(thd);
      break;
    }
    case SQLCOM_RELEASE_SAVEPOINT:
      if (trans_release_savepoint(thd, lex->ident)) goto error;
      my_ok(thd);
      break;
    case SQLCOM_ROLLBACK_TO_SAVEPOINT:
      if (trans_rollback_to_savepoint(thd, lex->ident)) goto error;
      my_ok(thd);
      break;
    case SQLCOM_SAVEPOINT:
      if (trans_savepoint(thd, lex->ident)) goto error;
      my_ok(thd);
      break;
    case SQLCOM_CREATE_PROCEDURE:
    case SQLCOM_CREATE_SPFUNCTION: {
      uint namelen;
      char *name;

      assert(lex->sphead != nullptr);
      assert(lex->sphead->m_db.str); /* Must be initialized in the parser */
      /*
        Verify that the database name is allowed, optionally
        lowercase it.
      */
      if (check_and_convert_db_name(&lex->sphead->m_db, false) !=
          Ident_name_check::OK)
        goto error;

      if (check_access(thd, CREATE_PROC_ACL, lex->sphead->m_db.str, nullptr,
                       nullptr, false, false))
        goto error;

      name = lex->sphead->name(&namelen);
      if (lex->sphead->m_type == enum_sp_type::FUNCTION) {
        udf_func *udf = find_udf(name, namelen);
        /*
          Issue a warning if there is an existing loadable function with the
          same name.
        */
        if (udf) {
          push_warning_printf(thd, Sql_condition::SL_NOTE,
                              ER_WARN_SF_UDF_NAME_COLLISION,
                              ER_THD(thd, ER_WARN_SF_UDF_NAME_COLLISION), name);
        }
      }

      if (sp_process_definer(thd)) goto error;

      /*
        Record the CURRENT_USER in binlog. The CURRENT_USER is used on slave to
        grant default privileges when sp_automatic_privileges variable is set.
      */
      thd->binlog_invoker();

      bool sp_already_exists = false;
      if (!(res = sp_create_routine(
                thd, lex->sphead, thd->lex->definer,
                thd->lex->create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS,
                sp_already_exists))) {
        if (!sp_already_exists) {
          /* only add privileges if really necessary */

          Security_context security_context;
          bool restore_backup_context = false;
          Security_context *backup = nullptr;
          /*
            We're going to issue an implicit GRANT statement so we close all
            open tables. We have to keep metadata locks as this ensures that
            this statement is atomic against concurrent FLUSH TABLES WITH READ
            LOCK. Deadlocks which can arise due to fact that this implicit
            statement takes metadata locks should be detected by a deadlock
            detector in MDL subsystem and reported as errors.

            No need to commit/rollback statement transaction, it's not started.

            TODO: Long-term we should either ensure that implicit GRANT
            statement is written into binary log as a separate statement or make
            both creation of routine and implicit GRANT parts of one fully
            atomic statement.
          */
          assert(thd->get_transaction()->is_empty(Transaction_ctx::STMT));
          close_thread_tables(thd);
          /*
            Check if invoker exists on slave, then use invoker privilege to
            insert routine privileges to mysql.procs_priv. If invoker is not
            available then consider using definer.

            Check if the definer exists on slave,
            then use definer privilege to insert routine privileges to
            mysql.procs_priv.

            For current user of SQL thread has GLOBAL_ACL privilege,
            which doesn't any check routine privileges,
            so no routine privilege record  will insert into mysql.procs_priv.
          */

          if (thd->slave_thread) {
            LEX_CSTRING current_user;
            LEX_CSTRING current_host;
            if (thd->has_invoker()) {
              current_host = thd->get_invoker_host();
              current_user = thd->get_invoker_user();
            } else {
              current_host = lex->definer->host;
              current_user = lex->definer->user;
            }
            if (is_acl_user(thd, current_host.str, current_user.str)) {
              security_context.change_security_context(
                  thd, current_user, current_host, thd->lex->sphead->m_db.str,
                  &backup);
              restore_backup_context = true;
            }
          }

          if (sp_automatic_privileges && !opt_noacl &&
              check_routine_access(
                  thd, DEFAULT_CREATE_PROC_ACLS, lex->sphead->m_db.str, name,
                  lex->sql_command == SQLCOM_CREATE_PROCEDURE, true)) {
            if (sp_grant_privileges(
                    thd, lex->sphead->m_db.str, name,
                    lex->sql_command == SQLCOM_CREATE_PROCEDURE))
              push_warning(thd, Sql_condition::SL_WARNING,
                           ER_PROC_AUTO_GRANT_FAIL,
                           ER_THD(thd, ER_PROC_AUTO_GRANT_FAIL));
            thd->clear_error();
          }

          /*
            Restore current user with GLOBAL_ACL privilege of SQL thread
          */
          if (restore_backup_context) {
            assert(thd->slave_thread == 1);
            thd->security_context()->restore_security_context(thd, backup);
          }
        }
        my_ok(thd);
      }
      break; /* break super switch */
    }        /* end case group bracket */

    case SQLCOM_ALTER_PROCEDURE:
    case SQLCOM_ALTER_FUNCTION: {
      if (check_routine_access(thd, ALTER_PROC_ACL, lex->spname->m_db.str,
                               lex->spname->m_name.str,
                               lex->sql_command == SQLCOM_ALTER_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type = (lex->sql_command == SQLCOM_ALTER_PROCEDURE)
                                 ? enum_sp_type::PROCEDURE
                                 : enum_sp_type::FUNCTION;
      /*
        Note that if you implement the capability of ALTER FUNCTION to
        alter the body of the function, this command should be made to
        follow the restrictions that log-bin-trust-function-creators=0
        already puts on CREATE FUNCTION.
      */
      /* Conditionally writes to binlog */
      res = sp_update_routine(thd, sp_type, lex->spname, &lex->sp_chistics);
      if (res || thd->killed) goto error;

      my_ok(thd);
      break;
    }
    case SQLCOM_DROP_PROCEDURE:
    case SQLCOM_DROP_FUNCTION: {
      if (lex->sql_command == SQLCOM_DROP_FUNCTION &&
          !lex->spname->m_explicit_name) {
        /* DROP FUNCTION <non qualified name> */
        udf_func *udf =
            find_udf(lex->spname->m_name.str, lex->spname->m_name.length);
        if (udf) {
          if (check_access(thd, DELETE_ACL, "mysql", nullptr, nullptr, true,
                           false))
            goto error;

          if (!(res = mysql_drop_function(thd, &lex->spname->m_name))) {
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DROP_FAILED, MYF(0), "FUNCTION (UDF)",
                   lex->spname->m_name.str);
          goto error;
        }

        if (lex->spname->m_db.str == nullptr) {
          if (lex->drop_if_exists) {
            push_warning_printf(thd, Sql_condition::SL_NOTE,
                                ER_SP_DOES_NOT_EXIST,
                                ER_THD(thd, ER_SP_DOES_NOT_EXIST),
                                "FUNCTION (UDF)", lex->spname->m_name.str);
            res = false;
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION (UDF)",
                   lex->spname->m_name.str);
          goto error;
        }
        /* Fall thought to test for a stored function */
      }

      const char *db = lex->spname->m_db.str;
      char *name = lex->spname->m_name.str;

      if (check_routine_access(thd, ALTER_PROC_ACL, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type = (lex->sql_command == SQLCOM_DROP_PROCEDURE)
                                 ? enum_sp_type::PROCEDURE
                                 : enum_sp_type::FUNCTION;

      /* Conditionally writes to binlog */
      enum_sp_return_code sp_result =
          sp_drop_routine(thd, sp_type, lex->spname);

      /*
        We're going to issue an implicit REVOKE statement so we close all
        open tables. We have to keep metadata locks as this ensures that
        this statement is atomic against concurrent FLUSH TABLES WITH READ
        LOCK. Deadlocks which can arise due to fact that this implicit
        statement takes metadata locks should be detected by a deadlock
        detector in MDL subsystem and reported as errors.

        No need to commit/rollback statement transaction, it's not started.

        TODO: Long-term we should either ensure that implicit REVOKE statement
              is written into binary log as a separate statement or make both
              dropping of routine and implicit REVOKE parts of one fully atomic
              statement.
      */
      assert(thd->get_transaction()->is_empty(Transaction_ctx::STMT));
      close_thread_tables(thd);

      if (sp_result != SP_DOES_NOT_EXISTS && sp_automatic_privileges &&
          !opt_noacl &&
          sp_revoke_privileges(thd, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE)) {
        push_warning(thd, Sql_condition::SL_WARNING, ER_PROC_AUTO_REVOKE_FAIL,
                     ER_THD(thd, ER_PROC_AUTO_REVOKE_FAIL));
        /* If this happens, an error should have been reported. */
        goto error;
      }

      res = sp_result;
      switch (sp_result) {
        case SP_OK:
          my_ok(thd);
          break;
        case SP_DOES_NOT_EXISTS:
          if (lex->drop_if_exists) {
            res =
                write_bin_log(thd, true, thd->query().str, thd->query().length);
            push_warning_printf(thd, Sql_condition::SL_NOTE,
                                ER_SP_DOES_NOT_EXIST,
                                ER_THD(thd, ER_SP_DOES_NOT_EXIST),
                                SP_COM_STRING(lex), lex->spname->m_qname.str);
            if (!res) my_ok(thd);
            break;
          }
          my_error(ER_SP_DOES_NOT_EXIST, MYF(0), SP_COM_STRING(lex),
                   lex->spname->m_qname.str);
          goto error;
        default:
          my_error(ER_SP_DROP_FAILED, MYF(0), SP_COM_STRING(lex),
                   lex->spname->m_qname.str);
          goto error;
      }
      break;
    }
    case SQLCOM_CREATE_VIEW: {
      /*
        Note: SQLCOM_CREATE_VIEW also handles 'ALTER VIEW' commands
        as specified through the thd->lex->create_view_mode flag.
      */
      res = mysql_create_view(thd, first_table, thd->lex->create_view_mode);
      break;
    }
    case SQLCOM_DROP_VIEW: {
      if (check_table_access(thd, DROP_ACL, all_tables, false, UINT_MAX, false))
        goto error;
      /* Conditionally writes to binlog. */
      res = mysql_drop_view(thd, first_table);
      break;
    }
    case SQLCOM_CREATE_TRIGGER:
    case SQLCOM_DROP_TRIGGER: {
      /* Conditionally writes to binlog. */
      assert(lex->m_sql_cmd != nullptr);
      static_cast<Sql_cmd_ddl_trigger_common *>(lex->m_sql_cmd)
          ->set_table(all_tables);

      res = lex->m_sql_cmd->execute(thd);
      break;
    }
    case SQLCOM_BINLOG_BASE64_EVENT: {
      mysql_client_binlog_statement(thd);
      break;
    }
    case SQLCOM_ANALYZE:
    case SQLCOM_CHECK:
    case SQLCOM_OPTIMIZE:
    case SQLCOM_REPAIR:
    case SQLCOM_TRUNCATE:
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_HA_OPEN:
    case SQLCOM_HA_READ:
    case SQLCOM_HA_CLOSE:
      assert(first_table == all_tables && first_table != nullptr);
      [[fallthrough]];
    case SQLCOM_CREATE_SERVER:
    case SQLCOM_CREATE_RESOURCE_GROUP:
    case SQLCOM_ALTER_SERVER:
    case SQLCOM_ALTER_RESOURCE_GROUP:
    case SQLCOM_DROP_RESOURCE_GROUP:
    case SQLCOM_DROP_SERVER:
    case SQLCOM_SET_RESOURCE_GROUP:
    case SQLCOM_SIGNAL:
    case SQLCOM_RESIGNAL:
    case SQLCOM_GET_DIAGNOSTICS:
    case SQLCOM_CHANGE_REPLICATION_FILTER:
    case SQLCOM_XA_START:
    case SQLCOM_XA_END:
    case SQLCOM_XA_PREPARE:
    case SQLCOM_XA_COMMIT:
    case SQLCOM_XA_ROLLBACK:
    case SQLCOM_XA_RECOVER:
    case SQLCOM_INSTALL_PLUGIN:
    case SQLCOM_UNINSTALL_PLUGIN:
    case SQLCOM_INSTALL_COMPONENT:
    case SQLCOM_UNINSTALL_COMPONENT:
    case SQLCOM_SHUTDOWN:
    case SQLCOM_ALTER_INSTANCE:
    case SQLCOM_SELECT:
    case SQLCOM_DO:
    case SQLCOM_CALL:
    case SQLCOM_CREATE_ROLE:
    case SQLCOM_DROP_ROLE:
    case SQLCOM_SET_ROLE:
    case SQLCOM_GRANT_ROLE:
    case SQLCOM_REVOKE_ROLE:
    case SQLCOM_ALTER_USER_DEFAULT_ROLE:
    case SQLCOM_SHOW_BINLOG_EVENTS:
    case SQLCOM_SHOW_BINLOGS:
    case SQLCOM_SHOW_CHARSETS:
    case SQLCOM_SHOW_COLLATIONS:
    case SQLCOM_SHOW_CREATE_DB:
    case SQLCOM_SHOW_CREATE_EVENT:
    case SQLCOM_SHOW_CREATE_FUNC:
    case SQLCOM_SHOW_CREATE_PROC:
    case SQLCOM_SHOW_CREATE:
    case SQLCOM_SHOW_CREATE_TRIGGER:
    // case SQLCOM_SHOW_CREATE_USER:
    case SQLCOM_SHOW_DATABASES:
    case SQLCOM_SHOW_ENGINE_LOGS:
    case SQLCOM_SHOW_ENGINE_MUTEX:
    case SQLCOM_SHOW_ENGINE_STATUS:
    case SQLCOM_SHOW_ERRORS:
    case SQLCOM_SHOW_EVENTS:
    case SQLCOM_SHOW_FIELDS:
    case SQLCOM_SHOW_FUNC_CODE:
    case SQLCOM_SHOW_GRANTS:
    case SQLCOM_SHOW_KEYS:
    case SQLCOM_SHOW_MASTER_STAT:
    case SQLCOM_SHOW_OPEN_TABLES:
    case SQLCOM_SHOW_PLUGINS:
    case SQLCOM_SHOW_PRIVILEGES:
    case SQLCOM_SHOW_PROC_CODE:
    case SQLCOM_SHOW_PROCESSLIST:
    case SQLCOM_SHOW_PROFILE:
    case SQLCOM_SHOW_PROFILES:
    case SQLCOM_SHOW_RELAYLOG_EVENTS:
    case SQLCOM_SHOW_SLAVE_HOSTS:
    case SQLCOM_SHOW_SLAVE_STAT:
    case SQLCOM_SHOW_STATUS:
    case SQLCOM_SHOW_STORAGE_ENGINES:
    case SQLCOM_SHOW_TABLE_STATUS:
    case SQLCOM_SHOW_TABLES:
    case SQLCOM_SHOW_TRIGGERS:
    case SQLCOM_SHOW_STATUS_PROC:
    case SQLCOM_SHOW_STATUS_FUNC:
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_SHOW_WARNS:
    case SQLCOM_CLONE:
    case SQLCOM_LOCK_INSTANCE:
    case SQLCOM_UNLOCK_INSTANCE:
    case SQLCOM_ALTER_TABLESPACE:
    case SQLCOM_EXPLAIN_OTHER:
    case SQLCOM_RESTART_SERVER:
    case SQLCOM_CREATE_SRS:
    case SQLCOM_DROP_SRS: {
      assert(lex->m_sql_cmd != nullptr);

      res = lex->m_sql_cmd->execute(thd);

      break;
    }
    case SQLCOM_ALTER_USER: {
      LEX_USER *user, *tmp_user;
      bool changing_own_password = false;
      Security_context *sctx = thd->security_context();
      bool own_password_expired = sctx->password_expired();
      bool check_permission = true;
      /* track if it is ALTER USER registration step */
      bool finish_reg = false;
      bool init_reg = false;
      bool unregister = false;
      bool is_self = false;

      List_iterator<LEX_USER> user_list(lex->users_list);
      while ((tmp_user = user_list++)) {
        LEX_MFA *tmp_lex_mfa;
        List_iterator<LEX_MFA> mfa_list_it(tmp_user->mfa_list);
        while ((tmp_lex_mfa = mfa_list_it++)) {
          finish_reg |= tmp_lex_mfa->finish_registration;
          init_reg |= tmp_lex_mfa->init_registration;
          unregister |= tmp_lex_mfa->unregister;
        }
        bool update_password_only = false;
        bool second_password = false;

        /* If it is an empty lex_user update it with current user */
        if (!tmp_user->host.str && !tmp_user->user.str) {
          /* set user information as of the current user */
          assert(sctx->priv_host().str);
          tmp_user->host.str = sctx->priv_host().str;
          tmp_user->host.length = strlen(sctx->priv_host().str);
          assert(sctx->user().str);
          tmp_user->user.str = sctx->user().str;
          tmp_user->user.length = strlen(sctx->user().str);
        }
        if (!(user = get_current_user(thd, tmp_user))) goto error;

        /* copy password expire attributes to individual lex user */
        user->alter_status = thd->lex->alter_password;
        /*
          Only self password change is non-privileged operation. To detect the
          same, we find :
          (1) If it is only password change operation
          (2) If this operation is on self
        */
        if (user->first_factor_auth_info.uses_identified_by_clause &&
            !user->first_factor_auth_info.uses_identified_with_clause &&
            !thd->lex->mqh.specified_limits &&
            !user->alter_status.update_account_locked_column &&
            !user->alter_status.update_password_expired_column &&
            !user->alter_status.expire_after_days &&
            user->alter_status.use_default_password_lifetime &&
            (user->alter_status.update_password_require_current ==
             Lex_acl_attrib_udyn::UNCHANGED) &&
            !user->alter_status.update_password_history &&
            !user->alter_status.update_password_reuse_interval &&
            !user->alter_status.update_failed_login_attempts &&
            !user->alter_status.update_password_lock_time &&
            (thd->lex->ssl_type == SSL_TYPE_NOT_SPECIFIED))
          update_password_only = true;

        is_self = !strcmp(sctx->user().length ? sctx->user().str : "",
                          user->user.str) &&
                  !my_strcasecmp(&my_charset_latin1, user->host.str,
                                 sctx->priv_host().str);
        if (finish_reg || init_reg) {
          /* Registration step is allowed only for connecting users */
          if (!is_self) {
            my_error(ER_INVALID_USER_FOR_REGISTRATION, MYF(0), sctx->user().str,
                     sctx->priv_host().str);
            goto error;
          }
        }
        /*
          if user executes ALTER statement to change password only
          for himself then skip access check - Provided preference to
          retain/discard current password was specified.
        */

        if (user->discard_old_password || user->retain_current_password) {
          second_password = true;
        }
        if ((update_password_only || user->discard_old_password || init_reg ||
             finish_reg || unregister) &&
            is_self) {
          changing_own_password = update_password_only;
          if (second_password) {
            if (check_access(thd, UPDATE_ACL, consts::mysql.c_str(), nullptr,
                             nullptr, true, true) &&
                !sctx->check_access(CREATE_USER_ACL, consts::mysql.c_str()) &&
                !(sctx->has_global_grant(
                          STRING_WITH_LEN("APPLICATION_PASSWORD_ADMIN"))
                      .first)) {
              my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
                       "CREATE USER or APPLICATION_PASSWORD_ADMIN");
              goto error;
            }
          }
          continue;
        } else if (check_permission) {
          if (check_access(thd, UPDATE_ACL, "mysql", nullptr, nullptr, true,
                           true) &&
              check_global_access(thd, CREATE_USER_ACL))
            goto error;

          check_permission = false;
        }

        if (is_self &&
            (user->first_factor_auth_info.uses_identified_by_clause ||
             user->first_factor_auth_info.uses_identified_with_clause ||
             user->first_factor_auth_info.uses_authentication_string_clause)) {
          changing_own_password = true;
          break;
        }

        if (update_password_only &&
            likely((get_server_state() == SERVER_OPERATING)) &&
            !strcmp(sctx->priv_user().str, "")) {
          my_error(ER_PASSWORD_ANONYMOUS_USER, MYF(0));
          goto error;
        }
      }

      if (unlikely(own_password_expired && !changing_own_password)) {
        my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
        goto error;
      }
      /* Conditionally writes to binlog */
      res = mysql_alter_user(thd, lex->users_list, lex->drop_if_exists);
      /*
        Iterate over list of MFA methods, check if all auth plugin methods
        which need registration steps have completed, then turn OFF server
        sandbox mode
      */
      tmp_user = lex->users_list[0];
      if (!res && is_self && finish_reg) {
        if (turn_off_sandbox_mode(thd, tmp_user)) return true;
      }
      break;
    }
    default:
      assert(0); /* Impossible */
      my_ok(thd);
      break;
  }
  goto finish;

error:
  res = true;

finish:
  /* Restore system variables which were changed by SET_VAR hint. */
  if (lex->opt_hints_global && lex->opt_hints_global->sys_var_hint)
    lex->opt_hints_global->sys_var_hint->restore_vars(thd);

  THD_STAGE_INFO(thd, stage_query_end);

  // Check for receiving a recent kill signal
  if (thd->killed) {
    thd->send_kill_message();
    res = thd->is_error();
  }
  if (res) {
    if (thd->get_reprepare_observer() != nullptr &&
        thd->get_reprepare_observer()->is_invalidated() &&
        thd->get_reprepare_observer()->can_retry())
      thd->skip_gtid_rollback = true;
  } else {
    lex->set_exec_started();
  }

  // Cleanup EXPLAIN info
  if (!thd->in_sub_stmt) {
    if (is_explainable_query(lex->sql_command)) {
      DEBUG_SYNC(thd, "before_reset_query_plan");
      /*
        We want EXPLAIN CONNECTION to work until the explained statement ends,
        thus it is only now that we may fully clean up any unit of this
        statement.
      */
      lex->unit->assert_not_fully_clean();
    }
    thd->query_plan.set_query_plan(SQLCOM_END, nullptr, false);
  }

  assert(!thd->in_active_multi_stmt_transaction() ||
         thd->in_multi_stmt_transaction_mode());

  if (!thd->in_sub_stmt) {
    mysql_audit_notify(thd,
                       first_level ? MYSQL_AUDIT_QUERY_STATUS_END
                                   : MYSQL_AUDIT_QUERY_NESTED_STATUS_END,
                       first_level ? "MYSQL_AUDIT_QUERY_STATUS_END"
                                   : "MYSQL_AUDIT_QUERY_NESTED_STATUS_END");

    /* report error issued during command execution */
    if ((thd->is_error() && !early_error_on_rep_command) ||
        (thd->variables.option_bits & OPTION_MASTER_SQL_ERROR))
      trans_rollback_stmt(thd);
    else {
      /* If commit fails, we should be able to reset the OK status. */
      thd->get_stmt_da()->set_overwrite_status(true);
      trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }
    /*
      Reset thd killed flag during cleanup so that commands which are
      dispatched using service session API's start with a clean state.
    */
    if (thd->killed == THD::KILL_QUERY || thd->killed == THD::KILL_TIMEOUT) {
      thd->killed = THD::NOT_KILLED;
      thd->reset_query_for_display();
    }
  }

  lex->cleanup(true);

  /* Free tables */
  THD_STAGE_INFO(thd, stage_closing_tables);
  close_thread_tables(thd);

  // Rollback any item transformations made during optimization and execution
  thd->rollback_item_tree_changes();

#ifndef NDEBUG
  if (lex->sql_command != SQLCOM_SET_OPTION && !thd->in_sub_stmt)
    DEBUG_SYNC(thd, "execute_command_after_close_tables");
#endif

  if (!thd->in_sub_stmt && thd->transaction_rollback_request) {
    /*
      We are not in sub-statement and transaction rollback was requested by
      one of storage engines (e.g. due to deadlock). Rollback transaction in
      all storage engines including binary log.
    */
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  } else if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END)) {
    /* No transaction control allowed in sub-statements. */
    assert(!thd->in_sub_stmt);
    /* If commit fails, we should be able to reset the OK status. */
    thd->get_stmt_da()->set_overwrite_status(true);
    /* Commit the normal transaction if one is active. */
    trans_commit_implicit(thd);
    thd->get_stmt_da()->set_overwrite_status(false);
    thd->mdl_context.release_transactional_locks();
  } else if (!thd->in_sub_stmt && !thd->in_multi_stmt_transaction_mode()) {
    /*
      - If inside a multi-statement transaction,
      defer the release of metadata locks until the current
      transaction is either committed or rolled back. This prevents
      other statements from modifying the table for the entire
      duration of this transaction.  This provides commit ordering
      and guarantees serializability across multiple transactions.
      - If in autocommit mode, or outside a transactional context,
      automatically release metadata locks of the current statement.
    */
    thd->mdl_context.release_transactional_locks();
  } else if (!thd->in_sub_stmt &&
             (thd->lex->sql_command != SQLCOM_CREATE_TABLE ||
              !thd->lex->create_info->m_transactional_ddl)) {
    thd->mdl_context.release_statement_locks();
  }

  // If the client wishes to have transaction state reported, we add whatever
  // is set on THD to our set here.
  {
    TX_TRACKER_GET(tst);

    if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
      tst->add_trx_state_from_thd(thd);

    // We're done. Clear "is DML" flag.
    tst->clear_trx_state(thd, TX_STMT_DML);
  }

#ifdef HAVE_LSAN_DO_RECOVERABLE_LEAK_CHECK
  // Get incremental leak reports, for easier leak hunting.
  // ./mtr --mem --mysqld='-T 4096' --sanitize main.1st
  // Don't waste time calling leak sanitizer during bootstrap.
  if (!opt_initialize && (test_flags & TEST_DO_QUICK_LEAK_CHECK)) {
    int have_leaks = __lsan_do_recoverable_leak_check();
    if (have_leaks > 0) {
      fprintf(stderr, "LSAN found leaks for Query: %*s\n",
              static_cast<int>(thd->query().length), thd->query().str);
      fflush(stderr);
    }
  }
#endif

#if defined(VALGRIND_DO_QUICK_LEAK_CHECK)
  // Get incremental leak reports, for easier leak hunting.
  // ./mtr --mem --mysqld='-T 4096' --valgrind-mysqld main.1st
  // Note that with multiple connections, the report below may be misleading.
  if (test_flags & TEST_DO_QUICK_LEAK_CHECK) {
    static unsigned long total_leaked_bytes = 0;
    unsigned long leaked = 0;
    unsigned long dubious [[maybe_unused]];
    unsigned long reachable [[maybe_unused]];
    unsigned long suppressed [[maybe_unused]];
    /*
      We could possibly use VALGRIND_DO_CHANGED_LEAK_CHECK here,
      but that is a fairly new addition to the Valgrind api.
      Note: we dont want to check 'reachable' until we have done shutdown,
      and that is handled by the final report anyways.
      We print some extra information, to tell mtr to ignore this report.
    */
    LogErr(INFORMATION_LEVEL, ER_VALGRIND_DO_QUICK_LEAK_CHECK);
    VALGRIND_DO_QUICK_LEAK_CHECK;
    VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed);
    if (leaked > total_leaked_bytes) {
      LogErr(ERROR_LEVEL, ER_VALGRIND_COUNT_LEAKS, leaked - total_leaked_bytes,
             static_cast<int>(thd->query().length), thd->query().str);
    }
    total_leaked_bytes = leaked;
  }
#endif

  if (!res && !thd->is_error()) {      // if statement succeeded
    binlog_gtid_end_transaction(thd);  // finalize GTID life-cycle
    DEBUG_SYNC(thd, "persist_new_state_after_statement_succeeded");
  } else if (!gtid_consistency_violation_state &&    // if the consistency state
             thd->has_gtid_consistency_violation) {  // was set by the failing
                                                     // statement
    gtid_state->end_gtid_violating_transaction(thd);  // just roll it back
    DEBUG_SYNC(thd, "restore_previous_state_after_statement_failed");
  }

  thd->skip_gtid_rollback = false;

  return res || thd->is_error();
}

/**
  Do special checking for SHOW statements.

  @param thd              Thread context.
  @param lex              LEX for SHOW statement.
  @param lock             If true, lock metadata for schema objects

  @returns false if check is successful, true if error
*/

bool show_precheck(THD *thd, LEX *lex, bool lock [[maybe_unused]]) {
  assert(lex->sql_command == SQLCOM_SHOW_CREATE_USER);
  Table_ref *const tables = lex->query_tables;
  if (tables != nullptr) {
    if (check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
      return true;
  }
  return false;
}

#define MY_YACC_INIT 1000  // Start with big alloc
#define MY_YACC_MAX 32000  // Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, YYLTYPE **yyls,
                   ulong *yystacksize) {
  Yacc_state *state = &current_thd->m_parser_state->m_yacc;
  ulong old_info = 0;
  assert(state);
  if ((uint)*yystacksize >= MY_YACC_MAX) return true;
  if (!state->yacc_yyvs) old_info = *yystacksize;
  *yystacksize = set_zone((*yystacksize) * 2, MY_YACC_INIT, MY_YACC_MAX);
  if (!(state->yacc_yyvs =
            (uchar *)my_realloc(key_memory_bison_stack, state->yacc_yyvs,
                                *yystacksize * sizeof(**yyvs),
                                MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(state->yacc_yyss =
            (uchar *)my_realloc(key_memory_bison_stack, state->yacc_yyss,
                                *yystacksize * sizeof(**yyss),
                                MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(state->yacc_yyls =
            (uchar *)my_realloc(key_memory_bison_stack, state->yacc_yyls,
                                *yystacksize * sizeof(**yyls),
                                MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return true;
  if (old_info) {
    /*
      Only copy the old stack on the first call to my_yyoverflow(),
      when replacing a static stack (YYINITDEPTH) by a dynamic stack.
      For subsequent calls, my_realloc already did preserve the old stack.
    */
    memcpy(state->yacc_yyss, *yyss, old_info * sizeof(**yyss));
    memcpy(state->yacc_yyvs, *yyvs, old_info * sizeof(**yyvs));
    memcpy(state->yacc_yyls, *yyls, old_info * sizeof(**yyls));
  }
  *yyss = (short *)state->yacc_yyss;
  *yyvs = (YYSTYPE *)state->yacc_yyvs;
  *yyls = (YYLTYPE *)state->yacc_yyls;
  return false;
}

/**
  Reset the part of THD responsible for the state of command
  processing.

  This needs to be called before execution of every statement
  (prepared or conventional).  It is not called by substatements of
  routines.

  @todo Remove mysql_reset_thd_for_next_command and only use the
  member function.

  @todo Call it after we use THD for queries, not before.
*/
void mysql_reset_thd_for_next_command(THD *thd) {
  thd->reset_for_next_command();
}

void THD::reset_for_next_command() {
  // TODO: Why on earth is this here?! We should probably fix this
  // function and move it to the proper file. /Matz
  THD *thd = this;
  DBUG_TRACE;
  assert(!thd->sp_runtime_ctx); /* not for substatements of routines */
  assert(!thd->in_sub_stmt);
  thd->reset_item_list();
  /*
    Those two lines below are theoretically unneeded as
    THD::cleanup_after_query() should take care of this already.
  */
  thd->auto_inc_intervals_in_cur_stmt_for_binlog.clear();
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt = false;

  thd->query_start_usec_used = false;
  thd->m_is_fatal_error = false;
  thd->time_zone_used = false;
  /*
    Clear the status flag that are expected to be cleared at the
    beginning of each SQL statement.
  */
  thd->server_status &= ~SERVER_STATUS_CLEAR_SET;
  /*
    If in autocommit mode and not in a transaction, reset flag
    that identifies if a transaction has done some operations
    that cannot be safely rolled back.

    If the flag is set an warning message is printed out in
    ha_rollback_trans() saying that some tables couldn't be
    rolled back.
  */
  if (!thd->in_multi_stmt_transaction_mode()) {
    thd->get_transaction()->reset_unsafe_rollback_flags(
        Transaction_ctx::SESSION);
  }
  assert(thd->security_context() == &thd->m_main_security_ctx);
  thd->thread_specific_used = false;

  if (opt_bin_log) {
    thd->user_var_events.clear();
    thd->user_var_events_alloc = thd->mem_root;
  }
  thd->clear_error();
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->reset_statement_cond_count();

  thd->rand_used = false;
  thd->m_sent_row_count = thd->m_examined_row_count = 0;

  thd->reset_current_stmt_binlog_format_row();
  thd->binlog_unsafe_warning_flags = 0;
  thd->binlog_need_explicit_defaults_ts = false;

  thd->commit_error = THD::CE_NONE;
  thd->durability_property = HA_REGULAR_DURABILITY;
  thd->set_trans_pos(nullptr, 0);
  thd->derived_tables_processing = false;
  thd->parsing_system_view = false;

  // Need explicit setting, else demand all privileges to a table.
  thd->want_privilege = ~NO_ACCESS;

  thd->reset_skip_readonly_check();
  thd->tx_commit_pending = false;

  DBUG_PRINT("debug", ("is_current_stmt_binlog_format_row(): %d",
                       thd->is_current_stmt_binlog_format_row()));

  /*
    In case we're processing multiple statements we need to checkout a new
    acl access map here as the global acl version might have increased due to
    a grant/revoke or flush.
  */
  thd->security_context()->checkout_access_maps();
#ifndef NDEBUG
  thd->set_tmp_table_seq_id(1);
#endif
}

/*
  Helper function to send the statement_id to the session var
*/
void statement_id_to_session(THD *thd) {
  auto sysvar_tracker =
      thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER);
  if (sysvar_tracker->is_enabled()) {
    LEX_CSTRING cs_statement = {STRING_WITH_LEN("statement_id")};
    sysvar_tracker->mark_as_changed(thd, cs_statement);
  }
}
/*
  When you modify dispatch_sql_command(), you may need to modify
  mysql_test_parse_for_slave() in this same file.
*/

/**
  Parse an SQL command from a text string and pass the resulting AST to the
  query executor.

  @param thd          Current session.
  @param parser_state Parser state.
*/

void dispatch_sql_command(THD *thd, Parser_state *parser_state) {
  DBUG_TRACE;
  DBUG_PRINT("dispatch_sql_command", ("query: '%s'", thd->query().str));
  statement_id_to_session(thd);
  DBUG_EXECUTE_IF("parser_debug", turn_parser_debug_on(););

  mysql_reset_thd_for_next_command(thd);
  // It is possible that rewritten query may not be empty (in case of
  // multiqueries). So reset it.
  thd->reset_rewritten_query();
  lex_start(thd);

  thd->m_parser_state = parser_state;
  invoke_pre_parse_rewrite_plugins(thd);
  thd->m_parser_state = nullptr;

  // we produce digest if it's not explicitly turned off
  // by setting maximum digest length to zero
  if (get_max_digest_length() != 0)
    parser_state->m_input.m_compute_digest = true;

  LEX *lex = thd->lex;
  const char *found_semicolon = nullptr;

  bool err = thd->get_stmt_da()->is_error();
  size_t qlen = 0;

  if (!err) {
    err = parse_sql(thd, parser_state, nullptr);
    if (!err) err = invoke_post_parse_rewrite_plugins(thd, false);

    found_semicolon = parser_state->m_lip.found_semicolon;
    qlen = found_semicolon ? (found_semicolon - thd->query().str)
                           : thd->query().length;
    /*
      We set thd->query_length correctly to not log several queries, when we
      execute only first. We set it to not see the ';' otherwise it would get
      into binlog and Query_log_event::print() would give ';;' output.
    */

    if (!thd->is_error() && found_semicolon && (ulong)(qlen)) {
      thd->set_query(thd->query().str, qlen - 1);
    }
  }

  DEBUG_SYNC_C("sql_parse_before_rewrite");

  if (!err) {
    /*
      Rewrite the query for logging and for the Performance Schema
      statement tables. (Raw logging happened earlier.)

      Sub-routines of mysql_rewrite_query() should try to only rewrite when
      necessary (e.g. not do password obfuscation when query contains no
      password).

      If rewriting does not happen here, thd->m_rewritten_query is still
      empty from being reset in alloc_query().
    */
    if (thd->rewritten_query().length() == 0) mysql_rewrite_query(thd);

    if (thd->rewritten_query().length()) {
      lex->safe_to_cache_query = false;  // see comments below

      thd->set_query_for_display(thd->rewritten_query().ptr(),
                                 thd->rewritten_query().length());
    } else if (thd->slave_thread) {
      /*
        In the slave, we add the information to pfs.events_statements_history,
        but not to pfs.threads, as that is what the test suite expects.
      */
      MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, thd->query().str,
                               thd->query().length);
    } else {
      thd->set_query_for_display(thd->query().str, thd->query().length);
    }

    if (!(opt_general_log_raw || thd->slave_thread)) {
      if (thd->rewritten_query().length())
        query_logger.general_log_write(thd, COM_QUERY,
                                       thd->rewritten_query().ptr(),
                                       thd->rewritten_query().length());
      else {
        query_logger.general_log_write(thd, COM_QUERY, thd->query().str, qlen);
      }
    }
  }

  const bool with_query_attributes = (thd->bind_parameter_values_count > 0);
  MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(thd->m_statement_psi,
                                          with_query_attributes);

  DEBUG_SYNC_C("sql_parse_after_rewrite");

  if (!err) {
    thd->m_statement_psi = MYSQL_REFINE_STATEMENT(
        thd->m_statement_psi, sql_statement_info[thd->lex->sql_command].m_key);

    if (mqh_used && thd->get_user_connect() &&
        check_mqh(thd, lex->sql_command)) {
      if (thd->is_classic_protocol())
        thd->get_protocol_classic()->get_net()->error = NET_ERROR_UNSET;
    } else {
      if (!thd->is_error()) {
        /* Actually execute the query */
        if (found_semicolon) {
          lex->safe_to_cache_query = false;
          thd->server_status |= SERVER_MORE_RESULTS_EXISTS;
        }
        lex->set_trg_event_type_for_tables();

        int error [[maybe_unused]];
        if (unlikely(
                (thd->security_context()->password_expired() ||
                 thd->security_context()->is_in_registration_sandbox_mode()) &&
                lex->sql_command != SQLCOM_SET_PASSWORD &&
                lex->sql_command != SQLCOM_ALTER_USER)) {
          if (thd->security_context()->is_in_registration_sandbox_mode())
            my_error(ER_PLUGIN_REQUIRES_REGISTRATION, MYF(0));
          else
            my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
          error = 1;
        } else {
          resourcegroups::Resource_group *src_res_grp = nullptr;
          resourcegroups::Resource_group *dest_res_grp = nullptr;
          MDL_ticket *ticket = nullptr;
          MDL_ticket *cur_ticket = nullptr;
          auto mgr_ptr = resourcegroups::Resource_group_mgr::instance();
          bool switched = mgr_ptr->switch_resource_group_if_needed(
              thd, &src_res_grp, &dest_res_grp, &ticket, &cur_ticket);

          error = mysql_execute_command(thd, true);

          if (switched)
            mgr_ptr->restore_original_resource_group(thd, src_res_grp,
                                                     dest_res_grp);
          thd->resource_group_ctx()->m_switch_resource_group_str[0] = '\0';
          if (ticket != nullptr)
            mgr_ptr->release_shared_mdl_for_resource_group(thd, ticket);
          if (cur_ticket != nullptr)
            mgr_ptr->release_shared_mdl_for_resource_group(thd, cur_ticket);
        }
      }
    }
  } else {
    /*
      Log the failed raw query in the Performance Schema. This statement did
      not parse, so there is no way to tell if it may contain a password of not.

      The tradeoff is:
        a) If we do log the query, a user typing by accident a broken query
           containing a password will have the password exposed. This is very
           unlikely, and this behavior can be documented. Remediation is to
           use a new password when retyping the corrected query.

        b) If we do not log the query, finding broken queries in the client
           application will be much more difficult. This is much more likely.

      Considering that broken queries can typically be generated by attempts at
      SQL injection, finding the source of the SQL injection is critical, so the
      design choice is to log the query text of broken queries (a).
    */
    thd->set_query_for_display(thd->query().str, thd->query().length);

    /* Instrument this broken statement as "statement/sql/error" */
    thd->m_statement_psi = MYSQL_REFINE_STATEMENT(
        thd->m_statement_psi, sql_statement_info[SQLCOM_END].m_key);

    assert(thd->is_error());
    DBUG_PRINT("info",
               ("Command aborted. Fatal_error: %d", thd->is_fatal_error()));
  }

  THD_STAGE_INFO(thd, stage_freeing_items);
  sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);
  thd->lex->destroy();
  thd->end_statement();
  thd->cleanup_after_query();
  assert(thd->change_list.is_empty());

  DEBUG_SYNC(thd, "query_rewritten");
}

/**
  Usable by the replication SQL thread only: just parse a query to know if it
  can be ignored because of replicate-*-table rules.

  @retval
    0	cannot be ignored
  @retval
    1	can be ignored
*/

bool mysql_test_parse_for_slave(THD *thd) {
  LEX *lex = thd->lex;
  bool ignorable = false;
  sql_digest_state *parent_digest = thd->m_digest;
  PSI_statement_locker *parent_locker = thd->m_statement_psi;
  DBUG_TRACE;

  assert(thd->slave_thread);

  Parser_state parser_state;
  if (parser_state.init(thd, thd->query().str, thd->query().length) == 0) {
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    thd->m_digest = nullptr;
    thd->m_statement_psi = nullptr;
    if (parse_sql(thd, &parser_state, nullptr) == 0) {
      if (all_tables_not_ok(thd, lex->query_block->get_table_list()))
        ignorable = true;
      else if (!check_database_filters(thd, thd->db().str, lex->sql_command))
        ignorable = true;
    }
    thd->m_digest = parent_digest;
    thd->m_statement_psi = parent_locker;
    thd->end_statement();
  }
  thd->cleanup_after_query();
  return ignorable;
}

/**
  Store field definition for create.

  @param thd                       The thread handler.
  @param field_name                The field name.
  @param type                      The type of the field.
  @param length                    The length of the field or NULL.
  @param decimals                  The length of a decimal part or NULL.
  @param type_modifier             Type modifiers & constraint flags of the
                                   field.
  @param default_value             The default value or NULL.
  @param on_update_value           The ON UPDATE expression or NULL.
  @param comment                   The comment.
  @param change                    The old column name (if renaming) or NULL.
  @param interval_list             The list of ENUM/SET values or NULL.
  @param cs                        The character set of the field.
  @param has_explicit_collation    Column has an explicit COLLATE attribute.
  @param uint_geom_type            The GIS type of the field.
  @param gcol_info                 The generated column data or NULL.
  @param default_val_expr          The expression for generating default values,
                                   if there is one, or nullptr.
  @param opt_after                 The name of the field to add after or
                                   the @see first_keyword pointer to insert
                                   first.
  @param srid                      The SRID for this column (only relevant if
                                   this is a geometry column).
  @param col_check_const_spec_list List of column check constraints.
  @param hidden                    Column hidden type.
  @param is_array                  Whether it's a typed array field

  @return
    Return 0 if ok
*/
bool Alter_info::add_field(
    THD *thd, const LEX_STRING *field_name, enum_field_types type,
    const char *length, const char *decimals, uint type_modifier,
    Item *default_value, Item *on_update_value, LEX_CSTRING *comment,
    const char *change, List<String> *interval_list, const CHARSET_INFO *cs,
    bool has_explicit_collation, uint uint_geom_type,
    Value_generator *gcol_info, Value_generator *default_val_expr,
    const char *opt_after, std::optional<gis::srid_t> srid,
    Sql_check_constraint_spec_list *col_check_const_spec_list,
    dd::Column::enum_hidden_type hidden, bool is_array) {
  uint8 datetime_precision = decimals ? atoi(decimals) : 0;
  DBUG_TRACE;
  assert(!is_array || hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SQL);

  LEX_CSTRING field_name_cstr = {field_name->str, field_name->length};

  if (check_string_char_length(field_name_cstr, "", NAME_CHAR_LEN,
                               system_charset_info, true)) {
    my_error(ER_TOO_LONG_IDENT, MYF(0),
             field_name->str); /* purecov: inspected */
    return true;               /* purecov: inspected */
  }
  if (type_modifier & PRI_KEY_FLAG) {
    List<Key_part_spec> key_parts;
    auto key_part_spec =
        new (thd->mem_root) Key_part_spec(field_name_cstr, 0, ORDER_ASC);
    if (key_part_spec == nullptr || key_parts.push_back(key_part_spec))
      return true;
    Key_spec *key = new (thd->mem_root)
        Key_spec(thd->mem_root, KEYTYPE_PRIMARY, NULL_CSTR,
                 &default_key_create_info, false, true, key_parts);
    if (key == nullptr || key_list.push_back(key)) return true;
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG)) {
    List<Key_part_spec> key_parts;
    auto key_part_spec =
        new (thd->mem_root) Key_part_spec(field_name_cstr, 0, ORDER_ASC);
    if (key_part_spec == nullptr || key_parts.push_back(key_part_spec))
      return true;
    Key_spec *key = new (thd->mem_root)
        Key_spec(thd->mem_root, KEYTYPE_UNIQUE, NULL_CSTR,
                 &default_key_create_info, false, true, key_parts);
    if (key == nullptr || key_list.push_back(key)) return true;
  }

  if (default_value) {
    /*
      Default value should be literal => basic constants =>
      no need fix_fields()

      We allow only CURRENT_TIMESTAMP as function default for the TIMESTAMP or
      DATETIME types. In addition, TRUE and FALSE are allowed for bool types.
    */
    if (default_value->type() == Item::FUNC_ITEM) {
      Item_func *func = down_cast<Item_func *>(default_value);
      if (func->basic_const_item()) {
        if (func->result_type() != INT_RESULT) {
          my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
          return true;
        }
        assert(dynamic_cast<Item_func_true *>(func) ||
               dynamic_cast<Item_func_false *>(func));
        default_value = new Item_int(func->val_int());
        if (default_value == nullptr) return true;
      } else if (func->functype() != Item_func::NOW_FUNC ||
                 !real_type_with_now_as_default(type) ||
                 default_value->decimals != datetime_precision) {
        my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
        return true;
      }
    } else if (default_value->type() == Item::NULL_ITEM) {
      default_value = nullptr;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
          NOT_NULL_FLAG) {
        my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
        return true;
      }
    } else if (type_modifier & AUTO_INCREMENT_FLAG) {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      return true;
    }
  }

  // 1) Reject combinations of DEFAULT <value> and DEFAULT (<expression>).
  // 2) Reject combinations of DEFAULT (<expression>) and AUTO_INCREMENT.
  // (Combinations of DEFAULT <value> and AUTO_INCREMENT are rejected above.)
  if ((default_val_expr && default_value) ||
      (default_val_expr && (type_modifier & AUTO_INCREMENT_FLAG))) {
    my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
    return true;
  }

  if (on_update_value && (!real_type_with_now_on_update(type) ||
                          on_update_value->decimals != datetime_precision)) {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    return true;
  }

  // If the SRID is specified on a non-geometric column, return an error
  if (type != MYSQL_TYPE_GEOMETRY && srid.has_value()) {
    my_error(ER_WRONG_USAGE, MYF(0), "SRID", "non-geometry column");
    return true;
  }

  Create_field *new_field = new (thd->mem_root) Create_field();
  if ((new_field == nullptr) ||
      new_field->init(thd, field_name->str, type, length, decimals,
                      type_modifier, default_value, on_update_value, comment,
                      change, interval_list, cs, has_explicit_collation,
                      uint_geom_type, gcol_info, default_val_expr, srid, hidden,
                      is_array))
    return true;

  for (const auto &a : cf_appliers) {
    if (a(new_field, this)) return true;
  }

  create_list.push_back(new_field);
  if (opt_after != nullptr) {
    flags |= Alter_info::ALTER_COLUMN_ORDER;
    new_field->after = opt_after;
  }

  if (col_check_const_spec_list) {
    /*
      Set column name, required for column check constraint validation in
      Sql_check_constraint_spec::pre_validate().
    */
    for (auto &cc_spec : *col_check_const_spec_list) {
      cc_spec->column_name = *field_name;
    }
    /*
      Move column check constraint specifications to table check constraints
      specifications list.
    */
    std::move(col_check_const_spec_list->begin(),
              col_check_const_spec_list->end(),
              std::back_inserter(check_constraint_spec_list));
  }

  return false;
}

/**
  save order by and tables in own lists.
*/

void add_to_list(SQL_I_List<ORDER> &list, ORDER *order) {
  DBUG_TRACE;
  order->used_alias = false;
  order->used = 0;
  list.link_in_list(order, &order->next);
}

/**
  Produces a PT_subquery object from a subquery's text.
  @param thd      Thread handler
  @param text     Subquery's text
  @param text_length  Length of 'text'
  @param text_offset Offset in bytes of 'text' in the original statement
  @param[out] node Produced PT_subquery object

  @returns true if error
 */
static bool reparse_common_table_expr(THD *thd, const char *text,
                                      size_t text_length, uint text_offset,
                                      PT_subquery **node) {
  Common_table_expr_parser_state parser_state;
  parser_state.init(thd, text, text_length);

  Parser_state *old = thd->m_parser_state;
  thd->m_parser_state = &parser_state;

  /*
    Re-parsing a CTE creates Item_param-s and Item_sp_local-s which are
    special, as they do not exist in the original query: thus they should not
    exist from the points of view of logging.
    This is achieved like this:
    - for SP local vars: their pos_in_query is set to 0
    - for PS parameters: they are not added to LEX::param_list and thus not to
    Prepared_statement::param_array.
    They still need a value, which they get like this:
    - for SP local vars: through the ordinary look-up of SP local
    variables' values by name of the variable.
    - for PS parameters: first the first-parsed, 'non-special' Item-params,
    which are in param_array, get their value bound from user-supplied data,
    then they propagate their value to their 'special' clones (@see
    Item_param::m_clones).
  */
  parser_state.m_lip.stmt_prepare_mode = old->m_lip.stmt_prepare_mode;
  parser_state.m_lip.multi_statements = false;  // A safety measure.
  parser_state.m_lip.m_digest = nullptr;

  // This is saved and restored by caller:
  thd->lex->reparse_common_table_expr_at = text_offset;

  /*
    As this function is called during parsing only, it can and should use the
    current Query_arena, character_set_client, etc.
    It intentionally uses THD::sql_parser() directly without the parse_sql()
    wrapper: because it's building a node of the statement currently being
    parsed at the upper call site.
  */
  bool mysql_parse_status = thd->sql_parser();
  thd->m_parser_state = old;
  if (mysql_parse_status) return true; /* purecov: inspected */

  *node = parser_state.result;
  return false;
}

bool PT_common_table_expr::make_subquery_node(THD *thd, PT_subquery **node) {
  if (m_postparse.references.size() >= 2) {
    // m_subq_node was already attached elsewhere, make new node:
    return reparse_common_table_expr(thd, m_subq_text.str, m_subq_text.length,
                                     m_subq_text_offset, node);
  }
  *node = m_subq_node;
  return false;
}

/**
   Tries to match an identifier to the CTEs in scope; if matched, it
   modifies *table_name, *tl', and the matched with-list-element.

   @param          thd      Thread handler
   @param[out]     table_name Identifier
   @param[in,out]  tl       Table_ref for the identifier
   @param          pc       Current parsing context, if available
   @param[out]     found    Is set to true if found.

   @returns true if error (OOM).
*/
bool Query_block::find_common_table_expr(THD *thd, Table_ident *table_name,
                                         Table_ref *tl, Parse_context *pc,
                                         bool *found) {
  *found = false;
  if (!pc) return false;

  PT_with_clause *wc;
  PT_common_table_expr *cte = nullptr;
  Query_block *select = this;
  Query_expression *unit;
  do {
    assert(select->first_execution);
    unit = select->master_query_expression();
    if (!(wc = unit->m_with_clause)) continue;
    if (wc->lookup(tl, &cte)) return true;
    /*
      If no match in the WITH clause of 'select', maybe this is a subquery, so
      look up in the outer query's WITH clause:
    */
  } while (cte == nullptr && (select = select->outer_query_block()));

  if (cte == nullptr) return false;
  *found = true;

  const auto save_reparse_cte = thd->lex->reparse_common_table_expr_at;
  PT_subquery *node;
  if (tl->is_recursive_reference()) {
    /*
      To pass the first steps of resolution, a recursive reference is here
      made to be a dummy derived table; after the temporary table is created
      based on the non-recursive members' types, the recursive reference is
      made to be a reference to the tmp table.
    */
    LEX_CSTRING dummy_subq = {STRING_WITH_LEN("(select 0)")};
    if (reparse_common_table_expr(thd, dummy_subq.str, dummy_subq.length, 0,
                                  &node))
      return true; /* purecov: inspected */
  } else if (cte->make_subquery_node(thd, &node))
    return true; /* purecov: inspected */
  // We imitate derived tables as much as possible.
  assert(parsing_place == CTX_NONE && linkage != GLOBAL_OPTIONS_TYPE);
  parsing_place = CTX_DERIVED;
  node->m_is_derived_table = true;
  auto wc_save = wc->enter_parsing_definition(tl);

  /*
    The outer context for the CTE is the current context of the query block
    that immediately contains the query expression that contains the CTE
    definition, which is the same as the outer context of the query block
    belonging to that query expression. Unless the CTE is contained in
    the outermost query expression, in which case there is no outer context.
  */
  thd->lex->push_context(select->outer_query_block() != nullptr
                             ? select->context.outer_context
                             : nullptr);
  assert(thd->lex->will_contextualize);
  if (node->contextualize(pc)) return true;

  thd->lex->pop_context();

  wc->leave_parsing_definition(wc_save);
  parsing_place = CTX_NONE;
  /*
    Prepared statement's parameters and SP local variables are spotted as
    'made during re-parsing' by node->contextualize(), which is why we
    ran that call _before_ restoring lex->reparse_common_table_expr_at.
  */
  thd->lex->reparse_common_table_expr_at = save_reparse_cte;
  tl->is_alias = true;
  Query_expression *node_query_expression =
      node->value()->master_query_expression();
  *table_name = Table_ident(node_query_expression);
  assert(table_name->is_derived_table());
  tl->db = table_name->db.str;
  tl->db_length = table_name->db.length;
  return false;
}

bool PT_with_clause::lookup(Table_ref *tl, PT_common_table_expr **found) {
  *found = nullptr;
  assert(tl->query_block != nullptr);
  /*
    If right_bound!=NULL, it means we are currently parsing the
    definition of CTE 'right_bound' and this definition contains
    'tl'.
  */
  const Common_table_expr *right_bound =
      m_most_inner_in_parsing ? m_most_inner_in_parsing->common_table_expr()
                              : nullptr;
  bool in_self = false;
  for (auto el : m_list->elements()) {
    // Search for a CTE named like 'tl', in this list, from left to right.
    if (el->is(right_bound)) {
      /*
        We meet right_bound.
        If not RECURSIVE:
        we must stop the search in this WITH clause;
        indeed right_bound must not reference itself or any CTE defined after it
        in the WITH list (forward references are forbidden, preventing any
        cycle).
        If RECURSIVE:
        If right_bound matches 'tl', it is a recursive reference.
      */
      if (!m_recursive) break;  // Prevent forward reference.
      in_self = true;           // Accept a recursive reference.
    }
    bool match;
    if (el->match_table_ref(tl, in_self, &match)) return true;
    if (!match) {
      if (in_self) break;  // Prevent forward reference.
      continue;
    }
    if (in_self && tl->query_block->outer_query_block() !=
                       m_most_inner_in_parsing->query_block) {
      /*
        SQL2011 says a recursive CTE cannot contain a subquery
        referencing the CTE, except if this subquery is a derived table
        like:
        WITH RECURSIVE qn AS (non-rec-SELECT UNION ALL
        SELECT * FROM (SELECT * FROM qn) AS dt)
        However, we don't allow this, as:
        - it simplifies detection and substitution correct recursive
        references (they're all on "level 0" of the UNION)
        - it's not limiting the user so much (in most cases, he can just
        merge his DT up manually, as the DT cannot contain aggregation).
        - Oracle bans it:
        with qn (a) as (
        select 123 from dual
        union all
        select 1+a from (select * from qn) where a<130) select * from qn
        ORA-32042: recursive WITH clause must reference itself directly in one
        of the UNION ALL branches.

        The above if() works because, when we parse such example query, we
        first resolve the 'qn' reference in the top query, making it a derived
        table:

        select * from (
           select 123 from dual
           union all
           select 1+a from (select * from qn) where a<130) qn(a);
                                                           ^most_inner_in_parsing
        Then we contextualize that derived table (containing the union);
        when we contextualize the recursive query block of the union, the
        inner 'qn' is recognized as a recursive reference, and its
        query_block->outer_query_block() is _not_ the query_block of
        most_inner_in_parsing, which indicates that the inner 'qn' is placed
        too deep.
      */
      my_error(ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE, MYF(0),
               el->name().str);
      return true;
    }
    *found = el;
    break;
  }
  return false;
}

bool PT_common_table_expr::match_table_ref(Table_ref *tl, bool in_self,
                                           bool *found) {
  *found = false;
  if (tl->table_name_length == m_name.length &&
      /*
        memcmp() is fine even if lower_case_table_names==1, as CTE names
        have been lowercased in the ctor.
      */
      !memcmp(tl->table_name, m_name.str, m_name.length)) {
    *found = true;
    // 'tl' is a reference to CTE 'el'.
    if (in_self) {
      m_postparse.recursive = true;
      if (tl->set_recursive_reference()) {
        my_error(ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE, MYF(0),
                 name().str);
        return true;
      }
    } else {
      if (m_postparse.references.push_back(tl))
        return true; /* purecov: inspected */
      if (m_column_names.size()) tl->set_derived_column_names(&m_column_names);
    }
    tl->set_common_table_expr(&m_postparse);
  }
  return false;
}

/**
  Add a table to list of used tables.

  @param thd      Current session.
  @param table_name	Table to add
  @param alias		alias for table (or null if no alias)
  @param table_options	A set of the following bits:
                         - TL_OPTION_UPDATING : Table will be updated
                         - TL_OPTION_FORCE_INDEX : Force usage of index
                         - TL_OPTION_ALIAS : an alias in multi table DELETE
  @param lock_type	How table should be locked
  @param mdl_type       Type of metadata lock to acquire on the table.
  @param index_hints_arg a list of index hints(FORCE/USE/IGNORE INDEX).
  @param partition_names List to carry partition names from PARTITION (...)
  clause in statement
  @param option         Used by cache index
  @param pc             Current parsing context, if available.

  @return Pointer to Table_ref element added to the total table list
  @retval
      0		Error
*/

Table_ref *Query_block::add_table_to_list(
    THD *thd, Table_ident *table_name, const char *alias, ulong table_options,
    thr_lock_type lock_type, enum_mdl_type mdl_type,
    List<Index_hint> *index_hints_arg, List<String> *partition_names,
    LEX_STRING *option, Parse_context *pc) {
  Table_ref *previous_table_ref =
      nullptr; /* The table preceding the current one. */
  LEX *lex = thd->lex;
  DBUG_TRACE;

  assert(table_name != nullptr);
  // A derived table has no table name, only an alias.
  if (!(table_options & TL_OPTION_ALIAS) && !table_name->is_derived_table()) {
    Ident_name_check ident_check_status =
        check_table_name(table_name->table.str, table_name->table.length);
    if (ident_check_status == Ident_name_check::WRONG) {
      my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name->table.str);
      return nullptr;
    } else if (ident_check_status == Ident_name_check::TOO_LONG) {
      my_error(ER_TOO_LONG_IDENT, MYF(0), table_name->table.str);
      return nullptr;
    }
  }
  LEX_STRING db = to_lex_string(table_name->db);
  if (!table_name->is_derived_table() && !table_name->is_table_function() &&
      table_name->db.str &&
      (check_and_convert_db_name(&db, false) != Ident_name_check::OK))
    return nullptr;

  const char *alias_str = alias ? alias : table_name->table.str;
  if (!alias) /* Alias is case sensitive */
  {
    if (table_name->sel) {
      my_error(ER_DERIVED_MUST_HAVE_ALIAS, MYF(0));
      return nullptr;
    }
    if (!(alias_str =
              (char *)thd->memdup(alias_str, table_name->table.length + 1)))
      return nullptr;
  }

  Table_ref *ptr = new (thd->mem_root) Table_ref;
  if (ptr == nullptr) return nullptr; /* purecov: inspected */

  if (lower_case_table_names && table_name->table.length)
    table_name->table.length = my_casedn_str(
        files_charset_info, const_cast<char *>(table_name->table.str));

  ptr->query_block = this;
  ptr->table_name = table_name->table.str;
  ptr->table_name_length = table_name->table.length;
  ptr->alias = alias_str;
  ptr->is_alias = alias != nullptr;
  ptr->table_function = table_name->table_function;
  if (table_name->table_function) {
    table_func_count++;
    ptr->derived_key_list.clear();
  }

  if (table_name->db.str) {
    ptr->is_fqtn = true;
    ptr->db = table_name->db.str;
    ptr->db_length = table_name->db.length;
  } else {
    // Check if the unqualified name could refer to a CTE. Don't do this for the
    // alias list of a multi-table DELETE statement (TL_OPTION_ALIAS), since
    // those are only references into the FROM list, and any CTEs referenced by
    // the aliases will be resolved when we later resolve the FROM list.
    bool found_cte = false;
    if ((table_options & TL_OPTION_ALIAS) == 0) {
      if (find_common_table_expr(thd, table_name, ptr, pc, &found_cte))
        return nullptr;
    }
    if (!found_cte && lex->copy_db_to(&ptr->db, &ptr->db_length))
      return nullptr;
  }

  ptr->set_tableno(0);
  ptr->set_lock({lock_type, THR_DEFAULT});
  ptr->updating = table_options & TL_OPTION_UPDATING;
  ptr->ignore_leaves = table_options & TL_OPTION_IGNORE_LEAVES;
  ptr->set_derived_query_expression(table_name->sel);

  if (!ptr->is_derived() && !ptr->is_table_function() &&
      is_infoschema_db(ptr->db, ptr->db_length)) {
    dd::info_schema::convert_table_name_case(
        const_cast<char *>(ptr->db), const_cast<char *>(ptr->table_name));

    bool hidden_system_view = false;
    ptr->is_system_view = dd::get_dictionary()->is_system_view_name(
        ptr->db, ptr->table_name, &hidden_system_view);

    ST_SCHEMA_TABLE *schema_table;
    if (ptr->updating &&
        /* Special cases which are processed by commands itself */
        lex->sql_command != SQLCOM_CHECK &&
        lex->sql_command != SQLCOM_CHECKSUM &&
        !(lex->sql_command == SQLCOM_CREATE_VIEW && ptr->is_system_view)) {
      my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
               thd->security_context()->priv_user().str,
               thd->security_context()->priv_host().str,
               INFORMATION_SCHEMA_NAME.str);
      return nullptr;
    }
    if (ptr->is_system_view) {
      if (thd->lex->sql_command != SQLCOM_CREATE_VIEW) {
        /*
          Stop users from using hidden system views, unless
          it is used by SHOW commands.
        */
        if (thd->lex->query_block && hidden_system_view &&
            !(thd->lex->query_block->active_options() &
              OPTION_SELECT_FOR_SHOW)) {
          my_error(ER_NO_SYSTEM_VIEW_ACCESS, MYF(0), ptr->table_name);
          return nullptr;
        }

        /*
          Stop users from accessing I_S.FILES if they do not have
          PROCESS privilege.
        */
        if (!strcmp(ptr->table_name, "FILES") &&
            check_global_access(thd, PROCESS_ACL))
          return nullptr;
      }
    } else {
      schema_table = find_schema_table(thd, ptr->table_name);
      /*
        Report an error
          if hidden schema table name is used in the statement other than
          SHOW statement OR
          if unknown schema table is used in the statement other than
          SHOW CREATE VIEW statement.
        Invalid view warning is reported for SHOW CREATE VIEW statement in
        the table open stage.
      */
      if ((!schema_table &&
           !(thd->query_plan.get_command() == SQLCOM_SHOW_CREATE &&
             thd->query_plan.get_lex()->only_view)) ||
          (schema_table && schema_table->hidden &&
           (sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) == 0)) {
        my_error(ER_UNKNOWN_TABLE, MYF(0), ptr->table_name,
                 INFORMATION_SCHEMA_NAME.str);
        return nullptr;
      }

      if (schema_table) {
        ptr->schema_table = schema_table;
      }
    }
  }

  ptr->cacheable_table = true;
  ptr->index_hints = index_hints_arg;
  ptr->option = option ? option->str : nullptr;
  /* check that used name is unique */
  if (lock_type != TL_IGNORE) {
    Table_ref *first_table = get_table_list();
    if (lex->sql_command == SQLCOM_CREATE_VIEW)
      first_table = first_table ? first_table->next_local : nullptr;
    for (Table_ref *tables = first_table; tables; tables = tables->next_local) {
      if (!my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
          !strcmp(ptr->db, tables->db)) {
        my_error(ER_NONUNIQ_TABLE, MYF(0), alias_str); /* purecov: tested */
        return nullptr;                                /* purecov: tested */
      }
    }
  }
  /* Store the table reference preceding the current one. */
  if (m_table_list.elements > 0) {
    /*
      table_list.next points to the last inserted Table_ref->next_local'
      element
      We don't use the offsetof() macro here to avoid warnings from gcc
    */
    previous_table_ref =
        (Table_ref *)((char *)m_table_list.next -
                      ((char *)&(ptr->next_local) - (char *)ptr));
    /*
      Set next_name_resolution_table of the previous table reference to point
      to the current table reference. In effect the list
      Table_ref::next_name_resolution_table coincides with
      Table_ref::next_local. Later this may be changed in
      store_top_level_join_columns() for NATURAL/USING joins.
    */
    previous_table_ref->next_name_resolution_table = ptr;
  }

  /*
    Link the current table reference in a local list (list for current select).
    Notice that as a side effect here we set the next_local field of the
    previous table reference to 'ptr'. Here we also add one element to the
    list 'table_list'.
  */
  m_table_list.link_in_list(ptr, &ptr->next_local);
  ptr->next_name_resolution_table = nullptr;
  ptr->partition_names = partition_names;
  /* Link table in global list (all used tables) */
  lex->add_to_query_tables(ptr);

  // Pure table aliases do not need to be locked:
  if (!(table_options & TL_OPTION_ALIAS)) {
    MDL_REQUEST_INIT(&ptr->mdl_request, MDL_key::TABLE, ptr->db,
                     ptr->table_name, mdl_type, MDL_TRANSACTION);
  }
  if (table_name->is_derived_table()) {
    ptr->derived_key_list.clear();
    derived_table_count++;
  }

  // Check access to DD tables. We must allow CHECK and ALTER TABLE
  // for the DDSE tables, since this is expected by the upgrade
  // client. We must also allow DDL access for the initialize thread,
  // since this thread is creating the I_S views.
  // Note that at this point, the mdl request for CREATE TABLE is still
  // MDL_SHARED, so we must explicitly check for SQLCOM_CREATE_TABLE.
  const dd::Dictionary *dictionary = dd::get_dictionary();
  if (dictionary &&
      !dictionary->is_dd_table_access_allowed(
          thd->is_dd_system_thread() || thd->is_initialize_system_thread() ||
              thd->is_server_upgrade_thread(),
          (ptr->mdl_request.is_ddl_or_lock_tables_lock_request() ||
           (lex->sql_command == SQLCOM_CREATE_TABLE &&
            ptr == lex->query_tables)) &&
              lex->sql_command != SQLCOM_CHECK &&
              lex->sql_command != SQLCOM_ALTER_TABLE,
          ptr->db, ptr->db_length, ptr->table_name)) {
    // We must allow creation of the system views even for non-system
    // threads since this is expected by the mysql_upgrade utility.
    if (!(lex->sql_command == SQLCOM_CREATE_VIEW &&
          dd::get_dictionary()->is_system_view_name(
              lex->query_tables->db, lex->query_tables->table_name))) {
      my_error(ER_NO_SYSTEM_TABLE_ACCESS, MYF(0),
               ER_THD_NONCONST(thd, dictionary->table_type_error_code(
                                        ptr->db, ptr->table_name)),
               ptr->db, ptr->table_name);
      // Take error handler into account to see if we should return.
      if (thd->is_error()) return nullptr;
    }
  }

  return ptr;
}

/**
  Initialize a new table list for a nested join.

    The function initializes a structure of the Table_ref type
    for a nested join. It sets up its nested join list as empty.
    The created structure is added to the front of the current
    join list in the Query_block object. Then the function
    changes the current nest level for joins to refer to the newly
    created empty list after having saved the info on the old level
    in the initialized structure.

  @param thd         current thread

  @retval
    0   if success
  @retval
    1   otherwise
*/

bool Query_block::init_nested_join(THD *thd) {
  DBUG_TRACE;

  Table_ref *const ptr = Table_ref::new_nested_join(
      thd->mem_root, "(nested_join)", embedding, m_current_table_nest, this);
  if (ptr == nullptr) return true;

  m_current_table_nest->push_front(ptr);
  embedding = ptr;
  m_current_table_nest = &ptr->nested_join->m_tables;

  return false;
}

/**
  End a nested join table list.

    The function returns to the previous join nest level.
    If the current level contains only one member, the function
    moves it one level up, eliminating the nest.

  @return
    - Pointer to Table_ref element added to the total table list, if
  success
    - 0, otherwise
*/

Table_ref *Query_block::end_nested_join() {
  Table_ref *ptr;
  NESTED_JOIN *nested_join;
  DBUG_TRACE;

  assert(embedding);
  ptr = embedding;
  m_current_table_nest = ptr->join_list;
  embedding = ptr->embedding;
  nested_join = ptr->nested_join;
  if (nested_join->m_tables.size() == 1) {
    Table_ref *embedded = nested_join->m_tables.front();
    m_current_table_nest->pop_front();
    embedded->join_list = m_current_table_nest;
    embedded->embedding = embedding;
    m_current_table_nest->push_front(embedded);
    ptr = embedded;
  } else if (nested_join->m_tables.empty()) {
    m_current_table_nest->pop_front();
    ptr = nullptr;  // return value
  }
  return ptr;
}

/**
  Plumbing for nest_last_join, q.v.
*/
Table_ref *nest_join(THD *thd, Query_block *select, Table_ref *embedding,
                     mem_root_deque<Table_ref *> *jlist, size_t table_cnt,
                     const char *legend) {
  DBUG_TRACE;

  Table_ref *const ptr = Table_ref::new_nested_join(thd->mem_root, legend,
                                                    embedding, jlist, select);
  if (ptr == nullptr) return nullptr;

  mem_root_deque<Table_ref *> *const embedded_list =
      &ptr->nested_join->m_tables;

  for (uint i = 0; i < table_cnt; i++) {
    Table_ref *table = jlist->front();
    jlist->pop_front();
    table->join_list = embedded_list;
    table->embedding = ptr;
    embedded_list->push_back(table);
    if (table->natural_join) ptr->is_natural_join = true;
  }
  jlist->push_front(ptr);

  return ptr;
}

/**
  Nest last join operations.

  The function nest last table_cnt join operations as if they were
  the components of a cross join operation.

  @param thd         current thread
  @param table_cnt   2 for regular joins: t1 JOIN t2.
                     N for the MySQL join-like extension: (t1, t2, ... tN).

  @return Pointer to Table_ref element created for the new nested join
  @retval
    0  Error
*/

Table_ref *Query_block::nest_last_join(THD *thd, size_t table_cnt) {
  return nest_join(thd, this, embedding, m_current_table_nest, table_cnt,
                   "(nest_last_join)");
}

/**
  Add a table to the current join list.

    The function puts a table in front of the current join list
    of Query_block object.
    Thus, joined tables are put into this list in the reverse order
    (the most outer join operation follows first).

  @param table       The table to add.

  @returns false if success, true if error (OOM).
*/

bool Query_block::add_joined_table(Table_ref *table) {
  DBUG_TRACE;
  m_current_table_nest->push_front(table);
  table->join_list = m_current_table_nest;
  table->embedding = embedding;
  return false;
}

void Query_block::set_lock_for_table(const Lock_descriptor &descriptor,
                                     Table_ref *table) {
  thr_lock_type lock_type = descriptor.type;
  bool for_update = lock_type >= TL_READ_NO_INSERT;
  enum_mdl_type mdl_type = mdl_type_for_dml(lock_type);
  DBUG_TRACE;
  DBUG_PRINT("enter", ("lock_type: %d  for_update: %d", lock_type, for_update));
  table->set_lock(descriptor);
  table->updating = for_update;
  table->mdl_request.set_type(mdl_type);
}

/**
  Set lock for all tables in current query block.

  @param lock_type Lock to set for tables.

  @note
    If the lock is a write lock, then tables->updating is set to true.
    This is to get tables_ok to know that the table is being updated by the
    query.
    Sets the type of metadata lock to request according to lock_type.
*/
void Query_block::set_lock_for_tables(thr_lock_type lock_type) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("lock_type: %d  for_update: %d", lock_type,
                       lock_type >= TL_READ_NO_INSERT));
  for (Table_ref *table = m_table_list.first; table; table = table->next_local)
    set_lock_for_table({lock_type, THR_WAIT}, table);
}

/**
  Push a new name resolution context for a JOIN ... ON clause to the
  context stack of a query block.

    Create a new name resolution context for a JOIN ... ON clause,
    set the first and last leaves of the list of table references
    to be used for name resolution, and push the newly created
    context to the stack of contexts of the query.

  @param pc        current parse context
  @param left_op   left  operand of the JOIN
  @param right_op  right operand of the JOIN

  @retval
    false  if all is OK
  @retval
    true   if a memory allocation error occurred
*/

bool push_new_name_resolution_context(Parse_context *pc, Table_ref *left_op,
                                      Table_ref *right_op) {
  THD *thd = pc->thd;
  Name_resolution_context *on_context;
  if (!(on_context = new (thd->mem_root) Name_resolution_context)) return true;
  on_context->init();
  on_context->first_name_resolution_table =
      left_op->first_leaf_for_name_resolution();
  on_context->last_name_resolution_table =
      right_op->last_leaf_for_name_resolution();
  on_context->query_block = pc->select;
  // Other tables in FROM clause of this JOIN are not visible:
  on_context->outer_context = thd->lex->current_context()->outer_context;
  on_context->next_context = pc->select->first_context;
  pc->select->first_context = on_context;

  return thd->lex->push_context(on_context);
}

/**
  Add an ON condition to the second operand of a JOIN ... ON.

    Add an ON condition to the right operand of a JOIN ... ON clause.

  @param b     the second operand of a JOIN ... ON
  @param expr  the condition to be added to the ON clause
*/

void add_join_on(Table_ref *b, Item *expr) {
  if (expr) {
    b->set_join_cond_optim((Item *)1);  // m_join_cond_optim is not ready
    if (!b->join_cond())
      b->set_join_cond(expr);
    else {
      /*
        If called from the parser, this happens if you have both a
        right and left join. If called later, it happens if we add more
        than one condition to the ON clause.
      */
      b->set_join_cond(new Item_cond_and(b->join_cond(), expr));
    }
    b->join_cond()->apply_is_true();
  }
}

const CHARSET_INFO *get_bin_collation(const CHARSET_INFO *cs) {
  const CHARSET_INFO *ret =
      get_charset_by_csname(cs->csname, MY_CS_BINSORT, MYF(0));
  if (ret) return ret;

  char tmp[65];
  strmake(strmake(tmp, cs->csname, sizeof(tmp) - 4), STRING_WITH_LEN("_bin"));
  my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
  return nullptr;
}

/**
  kill on thread.

  @param thd			Thread class
  @param id			Thread id
  @param only_kill_query        Should it kill the query or the connection

  @note
    This is written such that we have a short lock on LOCK_thd_list
*/

static uint kill_one_thread(THD *thd, my_thread_id id, bool only_kill_query) {
  uint error = ER_NO_SUCH_THREAD;
  Find_thd_with_id find_thd_with_id(id);

  DBUG_TRACE;
  DBUG_PRINT("enter", ("id=%u only_kill=%d", id, only_kill_query));
  DEBUG_SYNC(thd, "kill_thd_begin");
  THD_ptr tmp = Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
  Security_context *sctx = thd->security_context();
  if (tmp) {
    /*
      If we're SUPER, we can KILL anything, including system-threads.
      No further checks.

      KILLer: thd->m_security_ctx->user could in theory be NULL while
      we're still in "unauthenticated" state. This is a theoretical
      case (the code suggests this could happen, so we play it safe).

      KILLee: tmp->m_security_ctx->user will be NULL for system threads.
      We need to check so Jane Random User doesn't crash the server
      when trying to kill a) system threads or b) unauthenticated users'
      threads (Bug#43748).

      If user of both killer and killee are non-NULL, proceed with
      slayage if both are string-equal.
    */

    if (sctx->check_access(SUPER_ACL) ||
        sctx->has_global_grant(STRING_WITH_LEN("CONNECTION_ADMIN")).first ||
        sctx->user_matches(tmp->security_context())) {
      /*
        Process the kill:
        if thread is not already undergoing any kill connection.
        Killer must have SYSTEM_USER privilege iff killee has the same privilege
        privilege
      */
      if (tmp->killed != THD::KILL_CONNECTION) {
        if (tmp->is_system_user() && !thd->is_system_user()) {
          error = ER_KILL_DENIED_ERROR;
        } else {
          tmp->awake(only_kill_query ? THD::KILL_QUERY : THD::KILL_CONNECTION);
          error = 0;
        }
      } else
        error = 0;
    } else
      error = ER_KILL_DENIED_ERROR;
  }
  DEBUG_SYNC(thd, "kill_thd_end");
  DBUG_PRINT("exit", ("%d", error));
  return error;
}

/*
  kills a thread and sends response

  SYNOPSIS
    sql_kill()
    thd			Thread class
    id			Thread id
    only_kill_query     Should it kill the query or the connection
*/

static void sql_kill(THD *thd, my_thread_id id, bool only_kill_query) {
  uint error;
  if (!(error = kill_one_thread(thd, id, only_kill_query))) {
    if (!thd->killed) my_ok(thd);
  } else
    my_error(error, MYF(0), id);
}

/**
  This class implements callback function used by killall_non_super_threads
  to kill all threads that do not have either SYSTEM_VARIABLES_ADMIN +
  CONNECTION_ADMIN privileges or legacy SUPER privilege
*/

class Kill_non_super_conn : public Do_THD_Impl {
 private:
  /* THD of connected client. */
  THD *m_client_thd;
  bool m_is_client_regular_user;

 public:
  Kill_non_super_conn(THD *thd) : m_client_thd(thd) {
    assert(m_client_thd->security_context()->check_access(SUPER_ACL) ||
           (m_client_thd->is_connection_admin() &&
            m_client_thd->security_context()
                ->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
                .first));
    m_is_client_regular_user = !m_client_thd->is_system_user();
  }

  void operator()(THD *thd_to_kill) override {
    mysql_mutex_lock(&thd_to_kill->LOCK_thd_data);

    /* Kill only if non-privileged thread and non slave thread.
       If an account has not yet been assigned to the security context of the
       thread we cannot tell if the account is super user or not. In this case
       we cannot kill that thread. In offline mode, after the account is
       assigned to this thread and it turns out it is not privileged user
       thread, the authentication for this thread will fail and the thread will
       be terminated.
       Additionally, client with SYSTEM_VARIABLES_ADMIN but not SYSTEM_USER
       privilege is not allowed to kill threads having SYSTEM_USER,
       but not CONNECTION_ADMIN privilege.
    */
    const bool has_higher_privilege =
        m_is_client_regular_user && thd_to_kill->is_system_user();
    if (!thd_to_kill->is_connection_admin() &&
        thd_to_kill->killed != THD::KILL_CONNECTION &&
        !thd_to_kill->slave_thread && !has_higher_privilege)
      thd_to_kill->awake(THD::KILL_CONNECTION);

    mysql_mutex_unlock(&thd_to_kill->LOCK_thd_data);
  }
};

/*
  kills all the threads that do not have the
  SUPER privilege.

  SYNOPSIS
    killall_non_super_threads()
    thd                 Thread class
*/

void killall_non_super_threads(THD *thd) {
  Kill_non_super_conn kill_non_super_conn(thd);
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  thd_manager->do_for_all_thd(&kill_non_super_conn);
}

/**
  prepares the index and data directory path.

  @param thd                    Thread handle
  @param data_file_name         Pathname for data directory
  @param index_file_name        Pathname for index directory
  @param table_name             Table name to be appended to the pathname
  specified

  @return false                 success
  @return true                  An error occurred
*/

bool prepare_index_and_data_dir_path(THD *thd, const char **data_file_name,
                                     const char **index_file_name,
                                     const char *table_name) {
  int ret_val;
  const char *file_name;
  const char *directory_type;

  /*
    If a data directory path is passed, check if the path exists and append
    table_name to it.
  */
  if (data_file_name &&
      (ret_val = append_file_to_dir(thd, data_file_name, table_name))) {
    file_name = *data_file_name;
    directory_type = "DATA DIRECTORY";
    goto err;
  }

  /*
    If an index directory path is passed, check if the path exists and append
    table_name to it.
  */
  if (index_file_name &&
      (ret_val = append_file_to_dir(thd, index_file_name, table_name))) {
    file_name = *index_file_name;
    directory_type = "INDEX DIRECTORY";
    goto err;
  }

  return false;
err:
  if (ret_val == ER_PATH_LENGTH)
    my_error(ER_PATH_LENGTH, MYF(0), directory_type);
  if (ret_val == ER_WRONG_VALUE)
    my_error(ER_WRONG_VALUE, MYF(0), "path", file_name);
  return true;
}

/** If pointer is not a null pointer, append filename to it. */

int append_file_to_dir(THD *thd, const char **filename_ptr,
                       const char *table_name) {
  char tbbuff[FN_REFLEN];
  char buff[FN_REFLEN];
  char *ptr;
  char *end;

  if (!*filename_ptr) return 0;  // nothing to do

  /* Convert tablename to filename charset so that "/" gets converted
  appropriately */
  size_t tab_len = tablename_to_filename(table_name, tbbuff, sizeof(tbbuff));

  /* Check that the filename is not too long and it's a hard path */
  if (strlen(*filename_ptr) + tab_len >= FN_REFLEN - 1) return ER_PATH_LENGTH;

  if (!test_if_hard_path(*filename_ptr)) return ER_WRONG_VALUE;

  /* Fix is using unix filename format on dos */
  my_stpcpy(buff, *filename_ptr);
  end = convert_dirname(buff, *filename_ptr, NullS);

  ptr = (char *)thd->alloc((size_t)(end - buff) + tab_len + 1);
  if (ptr == nullptr) return ER_OUTOFMEMORY;  // End of memory
  *filename_ptr = ptr;
  strxmov(ptr, buff, tbbuff, NullS);
  return 0;
}

Comp_creator *comp_eq_creator(bool invert) {
  return invert ? (Comp_creator *)&ne_creator : (Comp_creator *)&eq_creator;
}

Comp_creator *comp_equal_creator(bool invert [[maybe_unused]]) {
  assert(!invert);  // Function never called with true.
  return &equal_creator;
}

Comp_creator *comp_ge_creator(bool invert) {
  return invert ? (Comp_creator *)&lt_creator : (Comp_creator *)&ge_creator;
}

Comp_creator *comp_gt_creator(bool invert) {
  return invert ? (Comp_creator *)&le_creator : (Comp_creator *)&gt_creator;
}

Comp_creator *comp_le_creator(bool invert) {
  return invert ? (Comp_creator *)&gt_creator : (Comp_creator *)&le_creator;
}

Comp_creator *comp_lt_creator(bool invert) {
  return invert ? (Comp_creator *)&ge_creator : (Comp_creator *)&lt_creator;
}

Comp_creator *comp_ne_creator(bool invert) {
  return invert ? (Comp_creator *)&eq_creator : (Comp_creator *)&ne_creator;
}

/**
  Construct ALL/ANY/SOME subquery Item.

  @param left_expr   pointer to left expression
  @param cmp         compare function creator
  @param all         true if we create ALL subquery
  @param query_block  pointer on parsed subquery structure

  @return
    constructed Item (or 0 if out of memory)
*/
Item *all_any_subquery_creator(Item *left_expr,
                               chooser_compare_func_creator cmp, bool all,
                               Query_block *query_block) {
  if ((cmp == &comp_eq_creator) && !all)  //  = ANY <=> IN
    return new Item_in_subselect(left_expr, query_block);
  if ((cmp == &comp_ne_creator) && all)  // <> ALL <=> NOT IN
  {
    Item *i = new Item_in_subselect(left_expr, query_block);
    if (i == nullptr) return nullptr;
    Item *neg_i = i->truth_transformer(nullptr, Item::BOOL_NEGATED);
    if (neg_i != nullptr) return neg_i;
    return new Item_func_not(i);
  }
  Item_allany_subselect *it =
      new Item_allany_subselect(left_expr, cmp, query_block, all);
  if (all) return it->upper_item = new Item_func_not_all(it); /* ALL */

  return it->upper_item = new Item_func_nop_all(it); /* ANY/SOME */
}

/**
   Set proper open mode and table type for element representing target table
   of CREATE TABLE statement, also adjust statement table list if necessary.
*/

void create_table_set_open_action_and_adjust_tables(LEX *lex) {
  Table_ref *create_table = lex->query_tables;

  if (lex->create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_table->open_type = OT_TEMPORARY_ONLY;
  else
    create_table->open_type = OT_BASE_ONLY;

  if (lex->query_block->fields.empty()) {
    /*
      Avoid opening and locking target table for ordinary CREATE TABLE
      or CREATE TABLE LIKE for write (unlike in CREATE ... SELECT we
      won't do any insertions in it anyway). Not doing this causes
      problems when running CREATE TABLE IF NOT EXISTS for already
      existing log table.
    */
    create_table->set_lock({TL_READ, THR_DEFAULT});
  }
}

/**
  Set the specified definer to the default value, which is the
  current user in the thread.

  @param[in]  thd       thread handler
  @param[out] definer   definer
*/

void get_default_definer(THD *thd, LEX_USER *definer) {
  const Security_context *sctx = thd->security_context();

  definer->user.str = sctx->priv_user().str;
  definer->user.length = strlen(definer->user.str);

  definer->host.str = sctx->priv_host().str;
  definer->host.length = strlen(definer->host.str);

  definer->first_factor_auth_info.plugin = EMPTY_CSTR;
  definer->first_factor_auth_info.auth = NULL_CSTR;
  definer->current_auth = NULL_CSTR;
  definer->first_factor_auth_info.uses_identified_with_clause = false;
  definer->first_factor_auth_info.uses_identified_by_clause = false;
  definer->first_factor_auth_info.uses_authentication_string_clause = false;
  definer->uses_replace_clause = false;
  definer->retain_current_password = false;
  definer->discard_old_password = false;
  definer->alter_status.update_password_expired_column = false;
  definer->alter_status.use_default_password_lifetime = true;
  definer->alter_status.expire_after_days = 0;
  definer->alter_status.update_account_locked_column = false;
  definer->alter_status.account_locked = false;
  definer->alter_status.update_password_require_current =
      Lex_acl_attrib_udyn::DEFAULT;
  definer->first_factor_auth_info.has_password_generator = false;
  definer->alter_status.failed_login_attempts = 0;
  definer->alter_status.password_lock_time = 0;
  definer->alter_status.update_failed_login_attempts = false;
  definer->alter_status.update_password_lock_time = false;
}

/**
  Create default definer for the specified THD.

  @param[in] thd         thread handler

  @return
    - On success, return a valid pointer to the created and initialized
    LEX_USER, which contains definer information.
    - On error, return 0.
*/

LEX_USER *create_default_definer(THD *thd) {
  LEX_USER *definer;

  if (!(definer = (LEX_USER *)LEX_USER::alloc(thd))) return nullptr;

  thd->get_definer(definer);

  return definer;
}

/**
  Returns information about user or current user.

  @param[in] thd          thread handler
  @param[in] user         user

  @return
    - On success, return a valid pointer to initialized
    LEX_USER, which contains user information.
    - On error, return 0.
*/

LEX_USER *get_current_user(THD *thd, LEX_USER *user) {
  if (!user || !user->user.str)  // current_user
  {
    LEX_USER *default_definer = create_default_definer(thd);
    if (default_definer) {
      /*
        Inherit parser semantics from the statement in which the user parameter
        was used.
        This is needed because a LEX_USER is both used as a component in an
        AST and as a specifier for a particular user in the
        ACL subsystem.
      */
      default_definer->first_factor_auth_info
          .uses_authentication_string_clause =
          user->first_factor_auth_info.uses_authentication_string_clause;
      default_definer->first_factor_auth_info.uses_identified_by_clause =
          user->first_factor_auth_info.uses_identified_by_clause;
      default_definer->first_factor_auth_info.uses_identified_with_clause =
          user->first_factor_auth_info.uses_identified_with_clause;
      default_definer->uses_replace_clause = user->uses_replace_clause;
      default_definer->current_auth.str = user->current_auth.str;
      default_definer->current_auth.length = user->current_auth.length;
      default_definer->retain_current_password = user->retain_current_password;
      default_definer->discard_old_password = user->discard_old_password;
      default_definer->first_factor_auth_info.plugin =
          user->first_factor_auth_info.plugin;
      default_definer->first_factor_auth_info.auth =
          user->first_factor_auth_info.auth;
      default_definer->alter_status = user->alter_status;
      default_definer->first_factor_auth_info.has_password_generator =
          user->first_factor_auth_info.has_password_generator;
      return default_definer;
    }
  }

  return user;
}

/**
  Check that byte length of a string does not exceed some limit.

  @param str         string to be checked
  @param err_msg     error message to be displayed if the string is too long
  @param max_byte_length  max length

  @retval
    false   the passed string is not longer than max_length
  @retval
    true    the passed string is longer than max_length

  NOTE
    The function is not used in existing code but can be useful later?
*/

static bool check_string_byte_length(const LEX_CSTRING &str,
                                     const char *err_msg,
                                     size_t max_byte_length) {
  if (str.length <= max_byte_length) return false;

  my_error(ER_WRONG_STRING_LENGTH, MYF(0), str.str, err_msg, max_byte_length);

  return true;
}

/*
  Check that char length of a string does not exceed some limit.

  SYNOPSIS
  check_string_char_length()
      str              string to be checked
      err_msg          error message to be displayed if the string is too long
      max_char_length  max length in symbols
      cs               string charset

  RETURN
    false   the passed string is not longer than max_char_length
    true    the passed string is longer than max_char_length
*/

bool check_string_char_length(const LEX_CSTRING &str, const char *err_msg,
                              size_t max_char_length, const CHARSET_INFO *cs,
                              bool no_error) {
  int well_formed_error;
  size_t res = cs->cset->well_formed_len(cs, str.str, str.str + str.length,
                                         max_char_length, &well_formed_error);

  if (!well_formed_error && str.length == res) return false;

  if (!no_error) {
    ErrConvString err(str.str, str.length, cs);
    my_error(ER_WRONG_STRING_LENGTH, MYF(0), err.ptr(), err_msg,
             max_char_length);
  }
  return true;
}

/*
  Check if path does not contain mysql data home directory
  SYNOPSIS
    test_if_data_home_dir()
    dir                     directory
    conv_home_dir           converted data home directory
    home_dir_len            converted data home directory length

  RETURN VALUES
    0	ok
    1	error
*/
int test_if_data_home_dir(const char *dir) {
  char path[FN_REFLEN];
  size_t dir_len;
  DBUG_TRACE;

  if (!dir) return 0;

  (void)fn_format(path, dir, "", "",
                  (MY_RETURN_REAL_PATH | MY_RESOLVE_SYMLINKS));
  dir_len = strlen(path);
  if (mysql_unpacked_real_data_home_len <= dir_len) {
    if (dir_len > mysql_unpacked_real_data_home_len &&
        path[mysql_unpacked_real_data_home_len] != FN_LIBCHAR)
      return 0;

    if (lower_case_file_system) {
      if (!my_strnncoll(default_charset_info, (const uchar *)path,
                        mysql_unpacked_real_data_home_len,
                        (const uchar *)mysql_unpacked_real_data_home,
                        mysql_unpacked_real_data_home_len))
        return 1;
    } else if (!memcmp(path, mysql_unpacked_real_data_home,
                       mysql_unpacked_real_data_home_len))
      return 1;
  }
  return 0;
}

/**
  Check that host name string is valid.

  @param[in] str string to be checked

  @return             Operation status
    @retval  false    host name is ok
    @retval  true     host name string is longer than max_length or
                      has invalid symbols
*/

bool check_host_name(const LEX_CSTRING &str) {
  const char *name = str.str;
  const char *end = str.str + str.length;
  if (check_string_byte_length(str, ER_THD(current_thd, ER_HOSTNAME),
                               HOSTNAME_LENGTH))
    return true;

  while (name != end) {
    if (*name == '@') {
      my_printf_error(ER_UNKNOWN_ERROR,
                      "Malformed hostname (illegal symbol: '%c')", MYF(0),
                      *name);
      return true;
    }
    name++;
  }
  return false;
}

class Parser_oom_handler : public Internal_error_handler {
 public:
  Parser_oom_handler() : m_has_errors(false), m_is_mem_error(false) {}
  bool handle_condition(THD *thd, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *) override {
    if (*level == Sql_condition::SL_ERROR) {
      m_has_errors = true;
      /* Out of memory error is reported only once. Return as handled */
      if (m_is_mem_error &&
          (sql_errno == EE_CAPACITY_EXCEEDED || sql_errno == EE_OUTOFMEMORY))
        return true;
      if (sql_errno == EE_CAPACITY_EXCEEDED || sql_errno == EE_OUTOFMEMORY) {
        m_is_mem_error = true;
        if (sql_errno == EE_CAPACITY_EXCEEDED)
          my_error(ER_CAPACITY_EXCEEDED, MYF(0),
                   static_cast<ulonglong>(thd->variables.parser_max_mem_size),
                   "parser_max_mem_size",
                   ER_THD(thd, ER_CAPACITY_EXCEEDED_IN_PARSER));
        else
          my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
        return true;
      }
    }
    return false;
  }

 private:
  bool m_has_errors;
  bool m_is_mem_error;
};

/**
  Transform an SQL statement into an AST that is ready for resolving, using the
  supplied parser state and object creation context.

  This is a wrapper() for THD::sql_parser() and should generally be used for AST
  construction.

  The function may optionally generate a query digest, invoke this function as
  follows:


  @verbatim
    THD *thd = ...;
    const char *query_text = ...;
    uint query_length = ...;
    Object_creation_ctx *ctx = ...;
    bool rc;

    Parser_state parser_state;
    if (parser_state.init(thd, query_text, query_length)
    {
      ... handle error
    }

    parser_state.m_input.m_has_digest= true;
    parser_state.m_input.m_compute_digest= true;

    rc= parse_sql(the, &parser_state, ctx);
    if (! rc)
    {
      unsigned char md5[MD5_HASH_SIZE];
      char digest_text[1024];
      bool truncated;
      const sql_digest_storage *digest= & thd->m_digest->m_digest_storage;

      compute_digest_md5(digest, & md5[0]);
      compute_digest_text(digest, & digest_text[0], sizeof(digest_text), &
  truncated);
    }
  @endverbatim

  @param thd Thread context.
  @param parser_state Parser state.
  @param creation_ctx Object creation context.

  @return Error status.
    @retval false on success.
    @retval true on parsing error.
*/

bool parse_sql(THD *thd, Parser_state *parser_state,
               Object_creation_ctx *creation_ctx) {
  DBUG_TRACE;
  bool ret_value;
  assert(thd->m_parser_state == nullptr);
  // TODO fix to allow parsing gcol exprs after main query.
  //  assert(thd->lex->m_sql_cmd == NULL);

  /* Backup creation context. */

  Object_creation_ctx *backup_ctx = nullptr;

  if (creation_ctx) backup_ctx = creation_ctx->set_n_backup(thd);

  /* Set parser state. */

  thd->m_parser_state = parser_state;

  parser_state->m_digest_psi = nullptr;
  parser_state->m_lip.m_digest = nullptr;

  /*
    Partial parsers (GRAMMAR_SELECTOR_*) are not supposed to compute digests.
  */
  assert(!parser_state->m_lip.is_partial_parser() ||
         !parser_state->m_input.m_has_digest);

  /*
    Only consider statements that are supposed to have a digest,
    like top level queries.
  */
  if (parser_state->m_input.m_has_digest) {
    /*
      For these statements,
      see if the digest computation is required.
    */
    if (thd->m_digest != nullptr) {
      /* Start Digest */
      parser_state->m_digest_psi = MYSQL_DIGEST_START(thd->m_statement_psi);

      if (parser_state->m_input.m_compute_digest ||
          (parser_state->m_digest_psi != nullptr)) {
        /*
          If either:
          - the caller wants to compute a digest
          - the performance schema wants to compute a digest
          set the digest listener in the lexer.
        */
        parser_state->m_lip.m_digest = thd->m_digest;
        parser_state->m_lip.m_digest->m_digest_storage.m_charset_number =
            thd->charset()->number;
      }
    }
  }

  /* Parse the query. */

  /*
    Use a temporary DA while parsing. We don't know until after parsing
    whether the current command is a diagnostic statement, in which case
    we'll need to have the previous DA around to answer questions about it.
  */
  Diagnostics_area *parser_da = thd->get_parser_da();
  Diagnostics_area *da = thd->get_stmt_da();

  Parser_oom_handler poomh;
  // Note that we may be called recursively here, on INFORMATION_SCHEMA queries.

  thd->mem_root->set_max_capacity(thd->variables.parser_max_mem_size);
  thd->mem_root->set_error_for_capacity_exceeded(true);
  thd->push_internal_handler(&poomh);

  thd->push_diagnostics_area(parser_da, false);

  bool mysql_parse_status = thd->sql_parser();

  thd->pop_internal_handler();
  thd->mem_root->set_max_capacity(0);
  thd->mem_root->set_error_for_capacity_exceeded(false);
  /*
    Unwind diagnostics area.

    If any issues occurred during parsing, they will become
    the sole conditions for the current statement.

    Otherwise, if we have a diagnostic statement on our hands,
    we'll preserve the previous diagnostics area here so we
    can answer questions about it.  This specifically means
    that repeatedly asking about a DA won't clear it.

    Otherwise, it's a regular command with no issues during
    parsing, so we'll just clear the DA in preparation for
    the processing of this command.
  */

  if (parser_da->current_statement_cond_count() != 0) {
    /*
      Error/warning during parsing: top DA should contain parse error(s)!  Any
      pre-existing conditions will be replaced. The exception is diagnostics
      statements, in which case we wish to keep the errors so they can be sent
      to the client.
    */
    if (thd->lex->sql_command != SQLCOM_SHOW_WARNS &&
        thd->lex->sql_command != SQLCOM_GET_DIAGNOSTICS)
      da->reset_condition_info(thd);

    /*
      We need to put any errors in the DA as well as the condition list.
    */
    if (parser_da->is_error() && !da->is_error()) {
      da->set_error_status(parser_da->mysql_errno(), parser_da->message_text(),
                           parser_da->returned_sqlstate());
    }

    da->copy_sql_conditions_from_da(thd, parser_da);

    parser_da->reset_diagnostics_area();
    parser_da->reset_condition_info(thd);

    /*
      Do not clear the condition list when starting execution as it
      now contains not the results of the previous executions, but
      a non-zero number of errors/warnings thrown during parsing!
    */
    thd->lex->keep_diagnostics = DA_KEEP_PARSE_ERROR;
  }

  thd->pop_diagnostics_area();

  /*
    Check that if THD::sql_parser() failed either thd->is_error() is set, or an
    internal error handler is set.

    The assert will not catch a situation where parsing fails without an
    error reported if an error handler exists. The problem is that the
    error handler might have intercepted the error, so thd->is_error() is
    not set. However, there is no way to be 100% sure here (the error
    handler might be for other errors than parsing one).
  */

  assert(!mysql_parse_status || (mysql_parse_status && thd->is_error()) ||
         (mysql_parse_status && thd->get_internal_handler()));

  /* Reset parser state. */

  thd->m_parser_state = nullptr;

  /* Restore creation context. */

  if (creation_ctx) creation_ctx->restore_env(thd, backup_ctx);

  /* That's it. */

  ret_value = mysql_parse_status || thd->is_fatal_error();

  if ((ret_value == 0) && (parser_state->m_digest_psi != nullptr)) {
    /*
      On parsing success, record the digest in the performance schema.
    */
    assert(thd->m_digest != nullptr);
    MYSQL_DIGEST_END(parser_state->m_digest_psi,
                     &thd->m_digest->m_digest_storage);
  }

  return ret_value;
}

/**
  @} (end of group Runtime_Environment)
*/

/**
  Check and merge "[ CHARACTER SET charset ] [ COLLATE collation ]" clause

  @param [in]  charset    Character set pointer or NULL.
  @param [in]  collation  Collation pointer or NULL.
  @param [out] to         Resulting character set/collation/NULL on success,
                          untouched on failure.

  Check if collation "collation" is applicable to character set "charset".

  If "collation" is NULL (e.g. when COLLATE clause is not specified),
  then simply "charset" is returned in "to".
  And vice versa, if "charset" is NULL, "collation" is returned in "to".

  @returns false on success,
   otherwise returns true and pushes an error message on the error stack
*/

bool merge_charset_and_collation(const CHARSET_INFO *charset,
                                 const CHARSET_INFO *collation,
                                 const CHARSET_INFO **to) {
  if (charset != nullptr && collation != nullptr &&
      !my_charset_same(charset, collation)) {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), collation->m_coll_name,
             charset->csname);
    return true;
  }

  *to = collation != nullptr ? collation : charset;
  return false;
}

bool merge_sp_var_charset_and_collation(const CHARSET_INFO *charset,
                                        const CHARSET_INFO *collation,
                                        const CHARSET_INFO **to) {
  if (charset == nullptr && collation != nullptr) {
    my_error(
        ER_NOT_SUPPORTED_YET, MYF(0),
        "COLLATE with no CHARACTER SET in SP parameters, RETURNS, DECLARE");
    return true;
  }
  return merge_charset_and_collation(charset, collation, to);
}

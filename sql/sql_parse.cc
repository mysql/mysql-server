/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

#define MYSQL_LEX 1
#include "my_global.h"
#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_parse.h"        // sql_kill, *_precheck, *_prepare
#include "lock.h"             // try_transactional_lock,
                              // check_transactional_lock,
                              // set_handler_table_locks,
                              // lock_global_read_lock,
                              // make_global_read_lock_block_commit
#include "sql_base.h"         // find_temporary_table
#include "sql_cache.h"        // QUERY_CACHE_FLAGS_SIZE, query_cache_*
#include "sql_show.h"         // mysqld_list_*, mysqld_show_*,
                              // calc_sum_of_all_status
#include "mysqld.h"
#include "sql_locale.h"                         // my_locale_en_US
#include "log.h"                                // flush_error_log
#include "sql_view.h"         // mysql_create_view, mysql_drop_view
#include "sql_delete.h"       // mysql_delete
#include "sql_insert.h"       // mysql_insert
#include "sql_update.h"       // mysql_update, mysql_multi_update
#include "sql_partition.h"    // struct partition_info
#include "sql_db.h"           // mysql_change_db, mysql_create_db,
                              // mysql_rm_db, mysql_upgrade_db,
                              // mysql_alter_db,
                              // check_db_dir_existence,
                              // my_dbopt_cleanup
#include "sql_table.h"        // mysql_create_like_table,
                              // mysql_create_table,
                              // mysql_alter_table,
                              // mysql_backup_table,
                              // mysql_restore_table
#include "sql_reload.h"       // reload_acl_and_cache
#include "sql_admin.h"        // mysql_assign_to_keycache
#include "sql_connect.h"      // check_user,
                              // decrease_user_connections,
                              // thd_init_client_charset, check_mqh,
                              // reset_mqh
#include "sql_rename.h"       // mysql_rename_table
#include "sql_tablespace.h"   // mysql_alter_tablespace
#include "hostname.h"         // hostname_cache_refresh
#include "sql_acl.h"          // *_ACL, check_grant, is_acl_user,
                              // has_any_table_level_privileges,
                              // mysql_drop_user, mysql_rename_user,
                              // check_grant_routine,
                              // mysql_routine_grant,
                              // mysql_show_grants,
                              // sp_grant_privileges, ...
#include "sql_test.h"         // mysql_print_status
#include "sql_select.h"       // handle_select, mysql_select,
#include "sql_load.h"         // mysql_load
#include "sql_servers.h"      // create_servers, alter_servers,
                              // drop_servers, servers_reload
#include "sql_handler.h"      // mysql_ha_open, mysql_ha_close,
                              // mysql_ha_read
#include "sql_binlog.h"       // mysql_client_binlog_statement
#include "sql_do.h"           // mysql_do
#include "sql_help.h"         // mysqld_help
#include "rpl_constants.h"    // Incident, INCIDENT_LOST_EVENTS
#include "log_event.h"
#include "rpl_slave.h"
#include "rpl_master.h"
#include "rpl_filter.h"
#include <m_ctype.h>
#include <myisam.h>
#include <my_dir.h>
#include "rpl_handler.h"

#include "sp_head.h"
#include "sp.h"
#include "sp_cache.h"
#include "events.h"
#include "sql_trigger.h"
#include "transaction.h"
#include "sql_audit.h"
#include "sql_prepare.h"
#include "debug_sync.h"
#include "probes_mysql.h"
#include "set_var.h"
#include "opt_trace.h"
#include "mysql/psi/mysql_statement.h"
#include "sql_bootstrap.h"
#include "opt_explain.h"
#include "sql_rewrite.h"
#include "global_threads.h"
#include "sql_analyse.h"
#include "table_cache.h" // table_cache_manager

#include <algorithm>
using std::max;
using std::min;

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

/* Used in error handling only */
#define SP_COM_STRING(LP) \
  ((LP)->sql_command == SQLCOM_CREATE_SPFUNCTION || \
   (LP)->sql_command == SQLCOM_ALTER_FUNCTION || \
   (LP)->sql_command == SQLCOM_SHOW_CREATE_FUNC || \
   (LP)->sql_command == SQLCOM_DROP_FUNCTION ? \
   "FUNCTION" : "PROCEDURE")

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables);
static bool check_show_access(THD *thd, TABLE_LIST *table);
static void sql_kill(THD *thd, ulong id, bool only_kill_query);
static bool lock_tables_precheck(THD *thd, TABLE_LIST *tables);

const char *any_db="*any*";	// Special symbol for check_access

const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("Init DB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("Field List") },
  { C_STRING_WITH_LEN("Create DB") },
  { C_STRING_WITH_LEN("Drop DB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Statistics") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Debug") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("Delayed insert") },
  { C_STRING_WITH_LEN("Change user") },
  { C_STRING_WITH_LEN("Binlog Dump") },
  { C_STRING_WITH_LEN("Table Dump") },
  { C_STRING_WITH_LEN("Connect Out") },
  { C_STRING_WITH_LEN("Register Slave") },
  { C_STRING_WITH_LEN("Prepare") },
  { C_STRING_WITH_LEN("Execute") },
  { C_STRING_WITH_LEN("Long Data") },
  { C_STRING_WITH_LEN("Close stmt") },
  { C_STRING_WITH_LEN("Reset stmt") },
  { C_STRING_WITH_LEN("Set option") },
  { C_STRING_WITH_LEN("Fetch") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Binlog Dump GTID") },
  { C_STRING_WITH_LEN("Error") }  // Last command number
};

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED", "ROLLBACK ONLY"
};


Log_throttle log_throttle_qni(&opt_log_throttle_queries_not_using_indexes,
                              &LOCK_log_throttle_qni,
                              Log_throttle::LOG_THROTTLE_WINDOW_SIZE,
                              slow_log_print,
                              "throttle: %10lu 'index "
                              "not used' warning(s) suppressed.");


#ifdef HAVE_REPLICATION
/**
  Returns true if all tables should be ignored.
*/
inline bool all_tables_not_ok(THD *thd, TABLE_LIST *tables)
{
  return rpl_filter->is_on() && tables && !thd->sp_runtime_ctx &&
         !rpl_filter->tables_ok(thd->db, tables);
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
  
*/
inline bool db_stmt_db_ok(THD *thd, char* db)
{
  DBUG_ENTER("db_stmt_db_ok");

  if (!thd->slave_thread)
    DBUG_RETURN(TRUE);

  /*
    No filters exist in ignore/do_db ? Then, just check
    wild_do_table filtering. Otherwise, check the do_db
    rules.
  */
  bool db_ok= (rpl_filter->get_do_db()->is_empty() &&
               rpl_filter->get_ignore_db()->is_empty()) ?
              rpl_filter->db_ok_with_wild_table(db) :
              rpl_filter->db_ok(db);

  DBUG_RETURN(db_ok);
}
#endif


static bool some_non_temp_table_to_be_updated(THD *thd, TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    DBUG_ASSERT(table->db && table->table_name);
    if (table->updating && !find_temporary_table(thd, table))
      return 1;
  }
  return 0;
}


/*
  Implicitly commit a active transaction if statement requires so.

  @param thd    Thread handle.
  @param mask   Bitmask used for the SQL command match.

*/
bool stmt_causes_implicit_commit(const THD *thd, uint mask)
{
  const LEX *lex= thd->lex;
  bool skip= FALSE;
  DBUG_ENTER("stmt_causes_implicit_commit");

  if (!(sql_command_flags[lex->sql_command] & mask))
    DBUG_RETURN(FALSE);

  switch (lex->sql_command) {
  case SQLCOM_DROP_TABLE:
    skip= lex->drop_temporary;
    break;
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_CREATE_TABLE:
    /* If CREATE TABLE of non-temporary table, do implicit commit */
    skip= (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);
    break;
  case SQLCOM_SET_OPTION:
    skip= lex->autocommit ? FALSE : TRUE;
    break;
  default:
    break;
  }

  DBUG_RETURN(!skip);
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

uint sql_command_flags[SQLCOM_END+1];
uint server_command_flags[COM_END+1];

void init_update_queries(void)
{
  /* Initialize the server command flags array. */
  memset(server_command_flags, 0, sizeof(server_command_flags));

  server_command_flags[COM_STATISTICS]= CF_SKIP_QUESTIONS;
  server_command_flags[COM_PING]=       CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_PREPARE]= CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_CLOSE]=   CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_RESET]=   CF_SKIP_QUESTIONS;

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
  sql_command_flags[SQLCOM_CREATE_TABLE]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS |
                                            CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_INDEX]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE]=    CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE]=       CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_LOAD]=           CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_DB]=      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_DB]=        CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB_UPGRADE]= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB]=       CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW]=    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW]=      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER]= CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_UPDATE]=	    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_UPDATE_MULTI]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  // This is INSERT VALUES(...), can be VALUES(stored_func()) so we trace it
  sql_command_flags[SQLCOM_INSERT]=	    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_INSERT_SELECT]=  CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE]=         CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE_MULTI]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE]=        CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE_SELECT]= CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_SELECT]=         CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  // (1) so that subquery is traced when doing "SET @var = (subquery)"
  /*
    @todo SQLCOM_SET_OPTION should have CF_CAN_GENERATE_ROW_EVENTS
    set, because it may invoke a stored function that generates row
    events. /Sven
  */
  sql_command_flags[SQLCOM_SET_OPTION]=     CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE; // (1)
  // (1) so that subquery is traced when doing "DO @var := (subquery)"
  sql_command_flags[SQLCOM_DO]=             CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE; // (1)

  sql_command_flags[SQLCOM_SHOW_STATUS_PROC]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_STATUS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_DATABASES]=   CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_TRIGGERS]=    CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_EVENTS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_OPEN_TABLES]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_PLUGINS]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FIELDS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_KEYS]=        CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_VARIABLES]=   CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_CHARSETS]=    CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS]=  CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_BINLOGS]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STORAGE_ENGINES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PRIVILEGES]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_WARNS]=       CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ERRORS]=      CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_MUTEX]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_LOGS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_GRANTS]=      CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT_NONBLOCKING]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_PROC]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_FUNC]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_TRIGGER]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_PROC_CODE]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FUNC_CODE]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_EVENT]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILES]=    CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILE]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_BINLOG_BASE64_EVENT]= CF_STATUS_COMMAND |
                                                 CF_CAN_GENERATE_ROW_EVENTS;

   sql_command_flags[SQLCOM_SHOW_TABLES]=       (CF_STATUS_COMMAND |
                                                 CF_SHOW_TABLE_COMMAND |
                                                 CF_REEXECUTION_FRAGILE);
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS]= (CF_STATUS_COMMAND |
                                                CF_SHOW_TABLE_COMMAND |
                                                CF_REEXECUTION_FRAGILE);

  sql_command_flags[SQLCOM_CREATE_USER]=       CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_RENAME_USER]=       CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_USER]=         CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_USER]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_GRANT]=             CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_REVOKE]=            CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_REVOKE_ALL]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_OPTIMIZE]=          CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_FUNCTION]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE]=  CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION]= CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN]=    CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN]=  CF_CHANGES_DATA;

  /* Does not change the contents of the Diagnostics Area. */
  sql_command_flags[SQLCOM_GET_DIAGNOSTICS]= CF_DIAGNOSTIC_STMT;

  /*
    (1): without it, in "CALL some_proc((subq))", subquery would not be
    traced.
  */
  sql_command_flags[SQLCOM_CALL]=      CF_REEXECUTION_FRAGILE |
                                       CF_CAN_GENERATE_ROW_EVENTS |
                                       CF_OPTIMIZER_TRACE; // (1)
  sql_command_flags[SQLCOM_EXECUTE]=   CF_CAN_GENERATE_ROW_EVENTS;

  /*
    The following admin table operations are allowed
    on log tables.
  */
  sql_command_flags[SQLCOM_REPAIR]=    CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE]|= CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ANALYZE]=   CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHECK]=     CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_CREATE_USER]|=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_USER]|=         CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER]|=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER]|=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE]|=            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL]|=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_GRANT]|=             CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]=       CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_FLUSH]=              CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RESET]=              CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER]=      CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER]=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER]=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHANGE_MASTER]=      CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_START]=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_STOP]=         CF_AUTO_COMMIT_TRANS;

  /*
    The following statements can deal with temporary tables,
    so temporary tables should be pre-opened for those statements to
    simplify privilege checking.

    There are other statements that deal with temporary tables and open
    them, but which are not listed here. The thing is that the order of
    pre-opening temporary tables for those statements is somewhat custom.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_TABLE]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=     CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_TRUNCATE]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_LOAD]|=            CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_INDEX]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE_MULTI]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_INSERT_SELECT]|=   CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE_MULTI]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPLACE_SELECT]|=  CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SELECT]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SET_OPTION]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DO]|=              CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CALL]|=            CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECKSUM]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ANALYZE]|=         CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECK]|=           CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_OPTIMIZE]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPAIR]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]|= CF_PREOPEN_TMP_TABLES;

  /*
    DDL statements that should start with closing opened handlers.

    We use this flag only for statements for which open HANDLERs
    have to be closed before emporary tables are pre-opened.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_TABLE]|=      CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=     CF_HA_CLOSE;
  sql_command_flags[SQLCOM_TRUNCATE]|=        CF_HA_CLOSE;
  sql_command_flags[SQLCOM_REPAIR]|=          CF_HA_CLOSE;
  sql_command_flags[SQLCOM_OPTIMIZE]|=        CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ANALYZE]|=         CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CHECK]|=           CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_INDEX]|=      CF_HA_CLOSE;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]|=  CF_HA_CLOSE;

  /*
    Mark statements that always are disallowed in read-only
    transactions. Note that according to the SQL standard,
    even temporary table DDL should be disallowed.
  */
  sql_command_flags[SQLCOM_CREATE_TABLE]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_DB]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_DB]|=          CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB_UPGRADE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_USER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_USER]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER]|=    CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_FUNCTION]|=  CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION]|=CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION]|=    CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE]|=  CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REPAIR]|=           CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_GRANT]|=            CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE]|=           CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN]|= CF_DISALLOW_IN_RO_TRANS;
}

bool sqlcom_can_generate_row_events(const THD *thd)
{
  return (sql_command_flags[thd->lex->sql_command] &
          CF_CAN_GENERATE_ROW_EVENTS);
}
 
bool is_update_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CHANGES_DATA) != 0;
}


bool is_explainable_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CAN_BE_EXPLAINED) != 0;
}

/**
  Check if a sql command is allowed to write to log tables.
  @param command The SQL command
  @return true if writing is allowed
*/
bool is_log_table_write_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_WRITE_LOGS_COMMAND) != 0;
}

void execute_init_command(THD *thd, LEX_STRING *init_command,
                          mysql_rwlock_t *var_lock)
{
  Vio* save_vio;
  ulong save_client_capabilities;

  mysql_rwlock_rdlock(var_lock);
  if (!init_command->length)
  {
    mysql_rwlock_unlock(var_lock);
    return;
  }

  /*
    copy the value under a lock, and release the lock.
    init_command has to be executed without a lock held,
    as it may try to change itself
  */
  size_t len= init_command->length;
  char *buf= thd->strmake(init_command->str, len);
  mysql_rwlock_unlock(var_lock);

#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
  thd->profiling.set_query_source(buf, len);
#endif

  THD_STAGE_INFO(thd, stage_execution_of_init_command);
  save_client_capabilities= thd->client_capabilities;
  thd->client_capabilities|= CLIENT_MULTI_QUERIES;
  /*
    We don't need return result of execution to client side.
    To forbid this we should set thd->net.vio to 0.
  */
  save_vio= thd->net.vio;
  thd->net.vio= 0;
  dispatch_command(COM_QUERY, thd, buf, len);
  thd->client_capabilities= save_client_capabilities;
  thd->net.vio= save_vio;

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif
}

static char *fgets_fn(char *buffer, size_t size, fgets_input_t input, int *error)
{
  MYSQL_FILE *in= static_cast<MYSQL_FILE*> (input);
  char *line= mysql_file_fgets(buffer, size, in);
  if (error)
    *error= (line == NULL) ? ferror(in->m_file) : 0;
  return line;
}

static void handle_bootstrap_impl(THD *thd)
{
  MYSQL_FILE *file= bootstrap_file;
  char buffer[MAX_BOOTSTRAP_QUERY_SIZE];
  char *query;
  int length;
  int rc;
  int error= 0;

  DBUG_ENTER("handle_bootstrap");

#ifndef EMBEDDED_LIBRARY
  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd;
#endif /* EMBEDDED_LIBRARY */

  thd->security_ctx->user= (char*) my_strdup("boot", MYF(MY_WME));
  thd->security_ctx->priv_user[0]= thd->security_ctx->priv_host[0]=0;
  /*
    Make the "client" handle multiple results. This is necessary
    to enable stored procedures with SELECTs and Dynamic SQL
    in init-file.
  */
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;

  thd->init_for_queries();

  buffer[0]= '\0';

  for ( ; ; )
  {
    rc= read_bootstrap_query(buffer, &length, file, fgets_fn, &error);

    if (rc == READ_BOOTSTRAP_EOF)
      break;
    /*
      Check for bootstrap file errors. SQL syntax errors will be
      caught below.
    */
    if (rc != READ_BOOTSTRAP_SUCCESS)
    {
      /*
        mysql_parse() may have set a successful error status for the previous
        query. We must clear the error status to report the bootstrap error.
      */
      thd->get_stmt_da()->reset_diagnostics_area();

      /* Get the nearest query text for reference. */
      char *err_ptr= buffer + (length <= MAX_BOOTSTRAP_ERROR_LEN ?
                                        0 : (length - MAX_BOOTSTRAP_ERROR_LEN));
      switch (rc)
      {
      case READ_BOOTSTRAP_ERROR:
        my_printf_error(ER_UNKNOWN_ERROR, "Bootstrap file error, return code (%d). "
                        "Nearest query: '%s'", MYF(0), error, err_ptr);
        break;

      case READ_BOOTSTRAP_QUERY_SIZE:
        my_printf_error(ER_UNKNOWN_ERROR, "Boostrap file error. Query size "
                        "exceeded %d bytes near '%s'.", MYF(0),
                        MAX_BOOTSTRAP_LINE_SIZE, err_ptr);
        break;

      default:
        DBUG_ASSERT(false);
        break;
      }

      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    query= (char *) thd->memdup_w_gap(buffer, length + 1,
                                      thd->db_length + 1 +
                                      QUERY_CACHE_FLAGS_SIZE);
    size_t db_len= 0;
    memcpy(query + length + 1, (char *) &db_len, sizeof(size_t));
    thd->set_query_and_id(query, length, thd->charset(), next_query_id());
    DBUG_PRINT("query",("%-.4096s",thd->query()));
#if defined(ENABLED_PROFILING)
    thd->profiling.start_new_query();
    thd->profiling.set_query_source(thd->query(), length);
#endif

    /*
      We don't need to obtain LOCK_thread_count here because in bootstrap
      mode we have only one thread.
    */
    thd->set_time();
    Parser_state parser_state;
    if (parser_state.init(thd, thd->query(), length))
    {
      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    mysql_parse(thd, thd->query(), length, &parser_state);

    bootstrap_error= thd->is_error();
    thd->protocol->end_statement();

#if defined(ENABLED_PROFILING)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    free_root(&thd->transaction.mem_root,MYF(MY_KEEP_PREALLOC));
  }

  DBUG_VOID_RETURN;
}


/**
  Execute commands from bootstrap_file.

  Used when creating the initial grant tables.
*/

pthread_handler_t handle_bootstrap(void *arg)
{
  THD *thd=(THD*) arg;

  mysql_thread_set_psi_id(thd->thread_id);

  do_handle_bootstrap(thd);
  return 0;
}

void do_handle_bootstrap(THD *thd)
{
  bool thd_added= false;
  /* The following must be called before DBUG_ENTER */
  thd->thread_stack= (char*) &thd;
  if (my_thread_init() || thd->store_globals())
  {
#ifndef EMBEDDED_LIBRARY
    close_connection(thd, ER_OUT_OF_RESOURCES);
#endif
    thd->fatal_error();
    goto end;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  thd_added= true;
  add_global_thread(thd);
  mysql_mutex_unlock(&LOCK_thread_count);

  handle_bootstrap_impl(thd);

end:
  net_end(&thd->net);
  thd->release_resources();

  if (thd_added)
  {
    mysql_mutex_lock(&LOCK_thread_count);
    remove_global_thread(thd);
    mysql_mutex_unlock(&LOCK_thread_count);
  }
  /*
    For safety we delete the thd before signalling that bootstrap is done,
    since the server will be taken down immediately.
  */
  delete thd;

  mysql_mutex_lock(&LOCK_thread_count);
  in_bootstrap= FALSE;
  mysql_cond_broadcast(&COND_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);

#ifndef EMBEDDED_LIBRARY
  my_thread_end();
  pthread_exit(0);
#endif

  return;
}


/* This works because items are allocated with sql_alloc() */

void free_items(Item *item)
{
  Item *next;
  DBUG_ENTER("free_items");
  for (; item ; item=next)
  {
    next=item->next;
    item->delete_self();
  }
  DBUG_VOID_RETURN;
}

/**
   This works because items are allocated with sql_alloc().
   @note The function also handles null pointers (empty list).
*/
void cleanup_items(Item *item)
{
  DBUG_ENTER("cleanup_items");  
  for (; item ; item=item->next)
    item->cleanup();
  DBUG_VOID_RETURN;
}

#ifndef EMBEDDED_LIBRARY

/**
  Read one command from connection and execute it (query or simple command).
  This function is called in loop from thread function.

  For profiling to work, it must never be called recursively.

  @retval
    0  success
  @retval
    1  request of thread shutdown (see dispatch_command() description)
*/

bool do_command(THD *thd)
{
  bool return_value;
  char *packet= 0;
  ulong packet_length;
  NET *net= &thd->net;
  enum enum_server_command command;

  DBUG_ENTER("do_command");

  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  thd->lex->current_select= 0;

  /*
    This thread will do a blocking read from the client which
    will be interrupted when the next command is received from
    the client, the connection is closed or "net_wait_timeout"
    number of seconds has passed.
  */
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  /*
    XXX: this code is here only to clear possible errors of init_connect. 
    Consider moving to init_connect() instead.
  */
  thd->clear_error();				// Clear error message
  thd->get_stmt_da()->reset_diagnostics_area();

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
  thd->m_server_idle= true;
  packet_length= my_net_read(net);
  thd->m_server_idle= false;

  if (packet_length == packet_error)
  {
    DBUG_PRINT("info",("Got error %d reading command from socket %s",
		       net->error,
		       vio_description(net->vio)));

    /* Instrument this broken statement as "statement/com/error" */
    thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                 com_statement_info[COM_END].m_key);

    /* Check if we can continue without closing the connection */

    /* The error must be set. */
    DBUG_ASSERT(thd->is_error());
    thd->protocol->end_statement();

    /* Mark the statement completed. */
    MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
    thd->m_statement_psi= NULL;

    if (net->error != 3)
    {
      return_value= TRUE;                       // We have to close it.
      goto out;
    }

    net->error= 0;
    return_value= FALSE;
    goto out;
  }

  packet= (char*) net->read_pos;
  /*
    'packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, my_net_read returns zero.
    If packet_length is not zero, my_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in my_net_read:
    it sets packet[packet_length]= 0, but only for non-zero packets.
  */
  if (packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    packet[0]= (uchar) COM_SLEEP;
    packet_length= 1;
  }
  /* Do not rely on my_net_read, extra safety against programming errors. */
  packet[packet_length]= '\0';                  /* safety */

  command= (enum enum_server_command) (uchar) packet[0];

  if (command >= COM_END)
    command= COM_END;				// Wrong command

  DBUG_PRINT("info",("Command on %s = %d (%s)",
                     vio_description(net->vio), command,
                     command_name[command].str));

  /* Restore read timeout value */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);

  DBUG_ASSERT(packet_length);

  return_value= dispatch_command(command, thd, packet+1, (uint) (packet_length-1));

out:
  /* The statement instrumentation must be closed in all cases. */
  DBUG_ASSERT(thd->m_statement_psi == NULL);
  DBUG_RETURN(return_value);
}
#endif  /* EMBEDDED_LIBRARY */

/**
  @brief Determine if an attempt to update a non-temporary table while the
    read-only option was enabled has been made.

  This is a helper function to mysql_execute_command.

  @note SQLCOM_UPDATE_MULTI is an exception and delt with elsewhere.

  @see mysql_execute_command
  @returns Status code
    @retval TRUE The statement should be denied.
    @retval FALSE The statement isn't updating any relevant tables.
*/

static my_bool deny_updates_if_read_only_option(THD *thd,
                                                TABLE_LIST *all_tables)
{
  DBUG_ENTER("deny_updates_if_read_only_option");

  if (!opt_readonly)
    DBUG_RETURN(FALSE);

  LEX *lex= thd->lex;

  const my_bool user_is_super=
    ((ulong)(thd->security_ctx->master_access & SUPER_ACL) ==
     (ulong)SUPER_ACL);

  if (user_is_super)
    DBUG_RETURN(FALSE);

  if (!(sql_command_flags[lex->sql_command] & CF_CHANGES_DATA))
    DBUG_RETURN(FALSE);

  /* Multi update is an exception and is dealt with later. */
  if (lex->sql_command == SQLCOM_UPDATE_MULTI)
    DBUG_RETURN(FALSE);

  const my_bool create_temp_tables= 
    (lex->sql_command == SQLCOM_CREATE_TABLE) &&
    (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);

   const my_bool create_real_tables=
     (lex->sql_command == SQLCOM_CREATE_TABLE) &&
     !(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);

  const my_bool drop_temp_tables= 
    (lex->sql_command == SQLCOM_DROP_TABLE) &&
    lex->drop_temporary;

  const my_bool update_real_tables=
    ((create_real_tables ||
      some_non_temp_table_to_be_updated(thd, all_tables)) &&
     !(create_temp_tables || drop_temp_tables));

  const my_bool create_or_drop_databases=
    (lex->sql_command == SQLCOM_CREATE_DB) ||
    (lex->sql_command == SQLCOM_DROP_DB);

  if (update_real_tables || create_or_drop_databases)
  {
      /*
        An attempt was made to modify one or more non-temporary tables.
      */
      DBUG_RETURN(TRUE);
  }


  /* Assuming that only temporary tables are modified. */
  DBUG_RETURN(FALSE);
}

/**
  Perform one connection-level (COM_XXXX) command.

  @param command         type of command to perform
  @param thd             connection handle
  @param packet          data for the command, packet is always null-terminated
  @param packet_length   length of packet + 1 (to show that data is
                         null-terminated) except for COM_SLEEP, where it
                         can be zero.

  @todo
    set thd->lex->sql_command to SQLCOM_END here.
  @todo
    The following has to be changed to an 8 byte integer

  @retval
    0   ok
  @retval
    1   request of thread shutdown, i. e. if command is
        COM_QUIT/COM_SHUTDOWN
*/
bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length)
{
  NET *net= &thd->net;
  bool error= 0;
  DBUG_ENTER("dispatch_command");
  DBUG_PRINT("info",("packet: '%*.s'; command: %d", packet_length, packet, command));

  /* SHOW PROFILE instrumentation, begin */
#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
#endif

  /* DTRACE instrumentation, begin */
  MYSQL_COMMAND_START(thd->thread_id, command,
                      &thd->security_ctx->priv_user[0],
                      (char *) thd->security_ctx->host_or_ip);

  /* Performance Schema Interface instrumentation, begin */
  thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                               com_statement_info[command].m_key);

  thd->set_command(command);
  /*
    Commands which always take a long time are logged into
    the slow log only if opt_log_slow_admin_statements is set.
  */
  thd->enable_slow_log= TRUE;
  thd->lex->sql_command= SQLCOM_END; /* to avoid confusing VIEW detectors */
  thd->set_time();
  if (!thd->is_valid_time())
  {
    /*
     If the time has got past 2038 we need to shut this server down
     We do this by making sure every command is a shutdown and we 
     have enough privileges to shut the server down

     TODO: remove this when we have full 64 bit my_time_t support
    */
    thd->security_ctx->master_access|= SHUTDOWN_ACL;
    command= COM_SHUTDOWN;
  }
  thd->set_query_id(next_query_id());
  inc_thread_running();

  if (!(server_command_flags[command] & CF_SKIP_QUESTIONS))
    statistic_increment_rwlock(thd->status_var.questions, &LOCK_status);

  /**
    Clear the set of flags that are expected to be cleared at the
    beginning of each command.
  */
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;

  /**
    Enforce password expiration for all RPC commands, except the
    following:

    COM_QUERY does a more fine-grained check later.
    COM_STMT_CLOSE and COM_STMT_SEND_LONG_DATA don't return anything.
    COM_PING only discloses information that the server is running,
       and that's available through other means.
    COM_QUIT should work even for expired statements.
  */
  if (unlikely(thd->security_ctx->password_expired &&
               command != COM_QUERY &&
               command != COM_STMT_CLOSE &&
               command != COM_STMT_SEND_LONG_DATA &&
               command != COM_PING &&
               command != COM_QUIT))
  {
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    goto done;
  }

  switch (command) {
  case COM_INIT_DB:
  {
    LEX_STRING tmp;
    status_var_increment(thd->status_var.com_stat[SQLCOM_CHANGE_DB]);
    thd->convert_string(&tmp, system_charset_info,
			packet, packet_length, thd->charset());
    if (!mysql_change_db(thd, &tmp, FALSE))
    {
      general_log_write(thd, command, thd->db, thd->db_length);
      my_ok(thd);
    }
    break;
  }
#ifdef HAVE_REPLICATION
  case COM_REGISTER_SLAVE:
  {
    if (!register_slave(thd, (uchar*)packet, packet_length))
      my_ok(thd);
    break;
  }
#endif
  case COM_CHANGE_USER:
  {
    int auth_rc;
    status_var_increment(thd->status_var.com_other);

    thd->change_user();
    thd->clear_error();                         // if errors from rollback

    /* acl_authenticate() takes the data from net->read_pos */
    net->read_pos= (uchar*)packet;

    uint save_db_length= thd->db_length;
    char *save_db= thd->db;
    USER_CONN *save_user_connect=
      const_cast<USER_CONN*>(thd->get_user_connect());
    Security_context save_security_ctx= *thd->security_ctx;
    const CHARSET_INFO *save_character_set_client=
      thd->variables.character_set_client;
    const CHARSET_INFO *save_collation_connection=
      thd->variables.collation_connection;
    const CHARSET_INFO *save_character_set_results=
      thd->variables.character_set_results;

    auth_rc= acl_authenticate(thd, packet_length);
    MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER(thd);
    if (auth_rc)
    {
      my_free(thd->security_ctx->user);
      *thd->security_ctx= save_security_ctx;
      thd->set_user_connect(save_user_connect);
      thd->reset_db (save_db, save_db_length);
      thd->variables.character_set_client= save_character_set_client;
      thd->variables.collation_connection= save_collation_connection;
      thd->variables.character_set_results= save_character_set_results;
      thd->update_charset();
    }
    else
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* we've authenticated new user */
      if (save_user_connect)
	decrease_user_connections(save_user_connect);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
      my_free(save_db);
      my_free(save_security_ctx.user);
    }
    break;
  }
  case COM_STMT_EXECUTE:
  {
    mysqld_stmt_execute(thd, packet, packet_length);
    break;
  }
  case COM_STMT_FETCH:
  {
    mysqld_stmt_fetch(thd, packet, packet_length);
    break;
  }
  case COM_STMT_SEND_LONG_DATA:
  {
    mysql_stmt_get_longdata(thd, packet, packet_length);
    break;
  }
  case COM_STMT_PREPARE:
  {
    mysqld_stmt_prepare(thd, packet, packet_length);
    break;
  }
  case COM_STMT_CLOSE:
  {
    mysqld_stmt_close(thd, packet);
    break;
  }
  case COM_STMT_RESET:
  {
    mysqld_stmt_reset(thd, packet);
    break;
  }
  case COM_QUERY:
  {
    if (alloc_query(thd, packet, packet_length))
      break;					// fatal error is set
    MYSQL_QUERY_START(thd->query(), thd->thread_id,
                      (char *) (thd->db ? thd->db : ""),
                      &thd->security_ctx->priv_user[0],
                      (char *) thd->security_ctx->host_or_ip);
    char *packet_end= thd->query() + thd->query_length();

    if (opt_log_raw)
      general_log_write(thd, command, thd->query(), thd->query_length());

    DBUG_PRINT("query",("%-.4096s",thd->query()));

#if defined(ENABLED_PROFILING)
    thd->profiling.set_query_source(thd->query(), thd->query_length());
#endif

    MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, thd->query(), thd->query_length());

    Parser_state parser_state;
    if (parser_state.init(thd, thd->query(), thd->query_length()))
      break;

    mysql_parse(thd, thd->query(), thd->query_length(), &parser_state);

    while (!thd->killed && (parser_state.m_lip.found_semicolon != NULL) &&
           ! thd->is_error())
    {
      /*
        Multiple queries exits, execute them individually
      */
      char *beginning_of_next_stmt= (char*) parser_state.m_lip.found_semicolon;

      /* Finalize server status flags after executing a statement. */
      thd->update_server_status();
      thd->protocol->end_statement();
      query_cache_end_of_result(thd);
      ulong length= (ulong)(packet_end - beginning_of_next_stmt);

      log_slow_statement(thd);

      /* Remove garbage at start of query */
      while (length > 0 && my_isspace(thd->charset(), *beginning_of_next_stmt))
      {
        beginning_of_next_stmt++;
        length--;
      }

/* PSI end */
      MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
      thd->m_statement_psi= NULL;

/* DTRACE end */
      if (MYSQL_QUERY_DONE_ENABLED())
      {
        MYSQL_QUERY_DONE(thd->is_error());
      }

/* SHOW PROFILE end */
#if defined(ENABLED_PROFILING)
      thd->profiling.finish_current_query();
#endif

/* SHOW PROFILE begin */
#if defined(ENABLED_PROFILING)
      thd->profiling.start_new_query("continuing");
      thd->profiling.set_query_source(beginning_of_next_stmt, length);
#endif

/* DTRACE begin */
      MYSQL_QUERY_START(beginning_of_next_stmt, thd->thread_id,
                        (char *) (thd->db ? thd->db : ""),
                        &thd->security_ctx->priv_user[0],
                        (char *) thd->security_ctx->host_or_ip);

/* PSI begin */
      thd->m_statement_psi= MYSQL_START_STATEMENT(&thd->m_statement_state,
                                                  com_statement_info[command].m_key,
                                                  thd->db, thd->db_length,
                                                  thd->charset());
      THD_STAGE_INFO(thd, stage_init);
      MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, beginning_of_next_stmt, length);

      thd->set_query_and_id(beginning_of_next_stmt, length,
                            thd->charset(), next_query_id());
      /*
        Count each statement from the client.
      */
      statistic_increment_rwlock(thd->status_var.questions, &LOCK_status);
      thd->set_time(); /* Reset the query start time. */
      parser_state.reset(beginning_of_next_stmt, length);
      /* TODO: set thd->lex->sql_command to SQLCOM_END here */
      mysql_parse(thd, beginning_of_next_stmt, length, &parser_state);
    }

    DBUG_PRINT("info",("query ready"));
    break;
  }
  case COM_FIELD_LIST:				// This isn't actually needed
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0));	/* purecov: inspected */
    break;
#else
  {
    char *fields, *packet_end= packet + packet_length, *arg_end;
    /* Locked closure of all tables */
    TABLE_LIST table_list;
    LEX_STRING table_name;
    LEX_STRING db;
    /*
      SHOW statements should not add the used tables to the list of tables
      used in a transaction.
    */
    MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();

    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_FIELDS]);
    if (thd->copy_db_to(&db.str, &db.length))
      break;
    /*
      We have name + wildcard in packet, separated by endzero
    */
    arg_end= strend(packet);
    uint arg_length= arg_end - packet;

    /* Check given table name length. */
    if (arg_length >= packet_length || arg_length > NAME_LEN)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    thd->convert_string(&table_name, system_charset_info,
			packet, arg_length, thd->charset());
    enum_ident_name_check ident_check_status=
      check_table_name(table_name.str, table_name.length, FALSE);
    if (ident_check_status == IDENT_NAME_WRONG)
    {
      /* this is OK due to convert_string() null-terminating the string */
      my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name.str);
      break;
    }
    else if (ident_check_status == IDENT_NAME_TOO_LONG)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), table_name.str);
      break;
    }
    packet= arg_end + 1;
    mysql_reset_thd_for_next_command(thd);
    lex_start(thd);
    /* Must be before we init the table list. */
    if (lower_case_table_names)
      table_name.length= my_casedn_str(files_charset_info, table_name.str);
    table_list.init_one_table(db.str, db.length, table_name.str,
                              table_name.length, table_name.str, TL_READ);
    /*
      Init TABLE_LIST members necessary when the undelrying
      table is view.
    */
    table_list.select_lex= &(thd->lex->select_lex);
    thd->lex->
      select_lex.table_list.link_in_list(&table_list,
                                         &table_list.next_local);
    thd->lex->add_to_query_tables(&table_list);

    if (is_infoschema_db(table_list.db, table_list.db_length))
    {
      ST_SCHEMA_TABLE *schema_table= find_schema_table(thd, table_list.alias);
      if (schema_table)
        table_list.schema_table= schema_table;
    }

    uint query_length= (uint) (packet_end - packet); // Don't count end \0
    if (!(fields= (char *) thd->memdup(packet, query_length + 1)))
      break;
    thd->set_query(fields, query_length);
    general_log_print(thd, command, "%s %s", table_list.table_name, fields);

    if (open_temporary_tables(thd, &table_list))
      break;

    if (check_table_access(thd, SELECT_ACL, &table_list,
                           TRUE, UINT_MAX, FALSE))
      break;
    /*
      Turn on an optimization relevant if the underlying table
      is a view: do not fill derived tables.
    */
    thd->lex->sql_command= SQLCOM_SHOW_FIELDS;

    // See comment in opt_trace_disable_if_no_security_context_access()
    Opt_trace_start ots(thd, &table_list, thd->lex->sql_command, NULL,
                        NULL, 0, NULL, NULL);

    mysqld_list_fields(thd,&table_list,fields);

    thd->lex->unit.cleanup();
    /* No need to rollback statement transaction, it's not started. */
    DBUG_ASSERT(thd->transaction.stmt.is_empty());
    close_thread_tables(thd);
    thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

    thd->cleanup_after_query();
    break;
  }
#endif
  case COM_QUIT:
    /* We don't calculate statistics for this command */
    general_log_print(thd, command, NullS);
    net->error=0;				// Don't give 'abort' message
    thd->get_stmt_da()->disable_status();              // Don't send anything back
    error=TRUE;					// End server
    break;
#ifndef EMBEDDED_LIBRARY
  case COM_BINLOG_DUMP_GTID:
    error= com_binlog_dump_gtid(thd, packet, packet_length);
    break;
  case COM_BINLOG_DUMP:
    error= com_binlog_dump(thd, packet, packet_length);
    break;
#endif
  case COM_REFRESH:
  {
    int not_used;

    /*
      Initialize thd->lex since it's used in many base functions, such as
      open_tables(). Otherwise, it remains unitialized and may cause crash
      during execution of COM_REFRESH.
    */
    lex_start(thd);
    
    status_var_increment(thd->status_var.com_stat[SQLCOM_FLUSH]);
    ulong options= (ulong) (uchar) packet[0];
    if (trans_commit_implicit(thd))
      break;
    thd->mdl_context.release_transactional_locks();
    if (check_global_access(thd,RELOAD_ACL))
      break;
    general_log_print(thd, command, NullS);
#ifndef DBUG_OFF
    bool debug_simulate= FALSE;
    DBUG_EXECUTE_IF("simulate_detached_thread_refresh", debug_simulate= TRUE;);
    if (debug_simulate)
    {
      /*
        Simulate a reload without a attached thread session.
        Provides a environment similar to that of when the
        server receives a SIGHUP signal and reloads caches
        and flushes tables.
      */
      bool res;
      my_pthread_setspecific_ptr(THR_THD, NULL);
      res= reload_acl_and_cache(NULL, options | REFRESH_FAST,
                                NULL, &not_used);
      my_pthread_setspecific_ptr(THR_THD, thd);
      if (res)
        break;
    }
    else
#endif
    if (reload_acl_and_cache(thd, options, (TABLE_LIST*) 0, &not_used))
      break;
    if (trans_commit_implicit(thd))
      break;
    close_thread_tables(thd);
    thd->mdl_context.release_transactional_locks();
    my_ok(thd);
    break;
  }
#ifndef EMBEDDED_LIBRARY
  case COM_SHUTDOWN:
  {
    status_var_increment(thd->status_var.com_other);
    if (check_global_access(thd,SHUTDOWN_ACL))
      break; /* purecov: inspected */
    /*
      If the client is < 4.1.3, it is going to send us no argument; then
      packet_length is 0, packet[0] is the end 0 of the packet. Note that
      SHUTDOWN_DEFAULT is 0. If client is >= 4.1.3, the shutdown level is in
      packet[0].
    */
    enum mysql_enum_shutdown_level level;
    if (!thd->is_valid_time())
      level= SHUTDOWN_DEFAULT;
    else
      level= (enum mysql_enum_shutdown_level) (uchar) packet[0];
    if (level == SHUTDOWN_DEFAULT)
      level= SHUTDOWN_WAIT_ALL_BUFFERS; // soon default will be configurable
    else if (level != SHUTDOWN_WAIT_ALL_BUFFERS)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "this shutdown level");
      break;
    }
    DBUG_PRINT("quit",("Got shutdown command for level %u", level));
    general_log_print(thd, command, NullS);
    my_eof(thd);
    kill_mysql();
    error=TRUE;
    break;
  }
#endif
  case COM_STATISTICS:
  {
    STATUS_VAR current_global_status_var;
    ulong uptime;
    uint length __attribute__((unused));
    ulonglong queries_per_second1000;
    char buff[250];
    uint buff_len= sizeof(buff);

    general_log_print(thd, command, NullS);
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_STATUS]);
    calc_sum_of_all_status(&current_global_status_var);
    if (!(uptime= (ulong) (thd->start_time.tv_sec - server_start_time)))
      queries_per_second1000= 0;
    else
      queries_per_second1000= thd->query_id * LL(1000) / uptime;

    length= my_snprintf(buff, buff_len - 1,
                        "Uptime: %lu  Threads: %d  Questions: %lu  "
                        "Slow queries: %llu  Opens: %llu  Flush tables: %lu  "
                        "Open tables: %u  Queries per second avg: %u.%03u",
                        uptime,
                        (int) get_thread_count(), (ulong) thd->query_id,
                        current_global_status_var.long_query_count,
                        current_global_status_var.opened_tables,
                        refresh_version,
                        table_cache_manager.cached_tables(),
                        (uint) (queries_per_second1000 / 1000),
                        (uint) (queries_per_second1000 % 1000));
#ifdef EMBEDDED_LIBRARY
    /* Store the buffer in permanent memory */
    my_ok(thd, 0, 0, buff);
#else
    (void) my_net_write(net, (uchar*) buff, length);
    (void) net_flush(net);
    thd->get_stmt_da()->disable_status();
#endif
    break;
  }
  case COM_PING:
    status_var_increment(thd->status_var.com_other);
    my_ok(thd);				// Tell client we are alive
    break;
  case COM_PROCESS_INFO:
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_PROCESSLIST]);
    if (!thd->security_ctx->priv_user[0] &&
        check_global_access(thd, PROCESS_ACL))
      break;
    general_log_print(thd, command, NullS);
    mysqld_list_processes(thd,
			  thd->security_ctx->master_access & PROCESS_ACL ? 
			  NullS : thd->security_ctx->priv_user, 0);
    break;
  case COM_PROCESS_KILL:
  {
    if (thread_id & (~0xfffffffful))
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "thread_id", "mysql_kill()");
    else
    {
      status_var_increment(thd->status_var.com_stat[SQLCOM_KILL]);
      ulong id=(ulong) uint4korr(packet);
      sql_kill(thd,id,false);
    }
    break;
  }
  case COM_SET_OPTION:
  {
    status_var_increment(thd->status_var.com_stat[SQLCOM_SET_OPTION]);
    uint opt_command= uint2korr(packet);

    switch (opt_command) {
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_ON:
      thd->client_capabilities|= CLIENT_MULTI_STATEMENTS;
      my_eof(thd);
      break;
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_OFF:
      thd->client_capabilities&= ~CLIENT_MULTI_STATEMENTS;
      my_eof(thd);
      break;
    default:
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    break;
  }
  case COM_DEBUG:
    status_var_increment(thd->status_var.com_other);
    if (check_global_access(thd, SUPER_ACL))
      break;					/* purecov: inspected */
    mysql_print_status();
    general_log_print(thd, command, NullS);
    my_eof(thd);
    break;
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_TIME:				// Impossible from client
  case COM_DELAYED_INSERT: // INSERT DELAYED has been removed.
  case COM_END:
  default:
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    break;
  }

done:
  DBUG_ASSERT(thd->derived_tables == NULL &&
              (thd->open_tables == NULL ||
               (thd->locked_tables_mode == LTM_LOCK_TABLES)));

  /* Finalize server status flags after executing a command. */
  thd->update_server_status();
  if (thd->killed)
    thd->send_kill_message();
  thd->protocol->end_statement();
  query_cache_end_of_result(thd);

  if (!thd->is_error() && !thd->killed_errno())
    mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_RESULT, 0, 0);

  mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_STATUS,
                      thd->get_stmt_da()->is_error() ?
                      thd->get_stmt_da()->mysql_errno() : 0,
                      command_name[command].str);

  log_slow_statement(thd);

  THD_STAGE_INFO(thd, stage_cleaning_up);

  thd->reset_query();
  thd->set_command(COM_SLEEP);

  /* Performance Schema Interface instrumentation, end */
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  thd->m_statement_psi= NULL;

  dec_thread_running();
  thd->packet.shrink(thd->variables.net_buffer_length);	// Reclaim some memory
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));

  /* DTRACE instrumentation, end */
  if (MYSQL_QUERY_DONE_ENABLED() || MYSQL_COMMAND_DONE_ENABLED())
  {
    int res __attribute__((unused));
    res= (int) thd->is_error();
    if (command == COM_QUERY)
    {
      MYSQL_QUERY_DONE(res);
    }
    MYSQL_COMMAND_DONE(res);
  }

  /* SHOW PROFILE instrumentation, end */
#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif

  DBUG_RETURN(error);
}


/**
  Check whether we need to write the current statement (or its rewritten
  version if it exists) to the slow query log.
  As a side-effect, a digest of suppressed statements may be written.

  @param thd          thread handle

  @retval
    true              statement needs to be logged
  @retval
    false             statement does not need to be logged
*/

bool log_slow_applicable(THD *thd)
{
  DBUG_ENTER("log_slow_applicable");

  /*
    The following should never be true with our current code base,
    but better to keep this here so we don't accidently try to log a
    statement in a trigger or stored function
  */
  if (unlikely(thd->in_sub_stmt))
    DBUG_RETURN(false);                         // Don't set time for sub stmt

  /*
    Do not log administrative statements unless the appropriate option is
    set.
  */
  if (thd->enable_slow_log)
  {
    bool warn_no_index= ((thd->server_status &
                          (SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
                         opt_log_queries_not_using_indexes &&
                         !(sql_command_flags[thd->lex->sql_command] &
                           CF_STATUS_COMMAND));
    bool log_this_query=  ((thd->server_status & SERVER_QUERY_WAS_SLOW) ||
                           warn_no_index) &&
                          (thd->get_examined_row_count() >=
                           thd->variables.min_examined_row_limit);
    bool suppress_logging= log_throttle_qni.log(thd, warn_no_index);

    if (!suppress_logging && log_this_query)
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


/**
  Unconditionally the current statement (or its rewritten version if it
  exists) to the slow query log.

  @param thd              thread handle
*/

void log_slow_do(THD *thd)
{
  DBUG_ENTER("log_slow_do");

  THD_STAGE_INFO(thd, stage_logging_slow_query);
  thd->status_var.long_query_count++;

  if (thd->rewritten_query.length())
    slow_log_print(thd,
                   thd->rewritten_query.c_ptr_safe(),
                   thd->rewritten_query.length());
  else
    slow_log_print(thd, thd->query(), thd->query_length());

  DBUG_VOID_RETURN;
}


/**
  Check whether we need to write the current statement to the slow query
  log. If so, do so. This is a wrapper for the two functions above;
  most callers should use this wrapper.  Only use the above functions
  directly if you have expensive rewriting that you only need to do if
  the query actually needs to be logged (e.g. SP variables / NAME_CONST
  substitution when executing a PROCEDURE).
  A digest of suppressed statements may be logged instead of the current
  statement.

  @param thd              thread handle
*/

void log_slow_statement(THD *thd)
{
  DBUG_ENTER("log_slow_statement");

  if (log_slow_applicable(thd))
    log_slow_do(thd);

  DBUG_VOID_RETURN;
}


/**
  Create a TABLE_LIST object for an INFORMATION_SCHEMA table.

    This function is used in the parser to convert a SHOW or DESCRIBE
    table_name command to a SELECT from INFORMATION_SCHEMA.
    It prepares a SELECT_LEX and a TABLE_LIST object to represent the
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
                         enum enum_schema_tables schema_table_idx)
{
  SELECT_LEX *schema_select_lex= NULL;
  DBUG_ENTER("prepare_schema_table");

  switch (schema_table_idx) {
  case SCH_SCHEMATA:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0));   /* purecov: inspected */
    DBUG_RETURN(1);
#else
    break;
#endif

  case SCH_TABLE_NAMES:
  case SCH_TABLES:
  case SCH_VIEWS:
  case SCH_TRIGGERS:
  case SCH_EVENTS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0)); /* purecov: inspected */
    DBUG_RETURN(1);
#else
    {
      LEX_STRING db;
      size_t dummy;
      if (lex->select_lex.db == NULL &&
          lex->copy_db_to(&lex->select_lex.db, &dummy))
      {
        DBUG_RETURN(1);
      }
      schema_select_lex= new SELECT_LEX();
      db.str= schema_select_lex->db= lex->select_lex.db;
      schema_select_lex->table_list.first= NULL;
      db.length= strlen(db.str);

      if (check_and_convert_db_name(&db, FALSE) != IDENT_NAME_OK)
        DBUG_RETURN(1);
      break;
    }
#endif
  case SCH_COLUMNS:
  case SCH_STATISTICS:
  {
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0)); /* purecov: inspected */
    DBUG_RETURN(1);
#else
    DBUG_ASSERT(table_ident);
    TABLE_LIST **query_tables_last= lex->query_tables_last;
    schema_select_lex= new SELECT_LEX();
    /* 'parent_lex' is used in init_query() so it must be before it. */
    schema_select_lex->parent_lex= lex;
    schema_select_lex->init_query();
    if (!schema_select_lex->add_table_to_list(thd, table_ident, 0, 0, TL_READ,
                                              MDL_SHARED_READ))
      DBUG_RETURN(1);
    lex->query_tables_last= query_tables_last;
    break;
  }
#endif
  case SCH_PROFILES:
    /* 
      Mark this current profiling record to be discarded.  We don't
      wish to have SHOW commands show up in profiling.
    */
#if defined(ENABLED_PROFILING)
    thd->profiling.discard_current_query();
#endif
    break;
  case SCH_OPTIMIZER_TRACE:
  case SCH_OPEN_TABLES:
  case SCH_VARIABLES:
  case SCH_STATUS:
  case SCH_PROCEDURES:
  case SCH_CHARSETS:
  case SCH_ENGINES:
  case SCH_COLLATIONS:
  case SCH_COLLATION_CHARACTER_SET_APPLICABILITY:
  case SCH_USER_PRIVILEGES:
  case SCH_SCHEMA_PRIVILEGES:
  case SCH_TABLE_PRIVILEGES:
  case SCH_COLUMN_PRIVILEGES:
  case SCH_TABLE_CONSTRAINTS:
  case SCH_KEY_COLUMN_USAGE:
  default:
    break;
  }
  
  SELECT_LEX *select_lex= lex->current_select;
  if (make_schema_select(thd, select_lex, schema_table_idx))
  {
    DBUG_RETURN(1);
  }
  TABLE_LIST *table_list= select_lex->table_list.first;
  table_list->schema_select_lex= schema_select_lex;
  table_list->schema_table_reformed= 1;
  DBUG_RETURN(0);
}


/**
  Read query from packet and store in thd->query.
  Used in COM_QUERY and COM_STMT_PREPARE.

    Sets the following THD variables:
  - query
  - query_length

  @retval
    FALSE ok
  @retval
    TRUE  error;  In this case thd->fatal_error is set
*/

bool alloc_query(THD *thd, const char *packet, uint packet_length)
{
  char *query;
  /* Remove garbage at start and end of query */
  while (packet_length > 0 && my_isspace(thd->charset(), packet[0]))
  {
    packet++;
    packet_length--;
  }
  const char *pos= packet + packet_length;     // Point at end null
  while (packet_length > 0 &&
	 (pos[-1] == ';' || my_isspace(thd->charset() ,pos[-1])))
  {
    pos--;
    packet_length--;
  }
  /* We must allocate some extra memory for query cache 

    The query buffer layout is:
       buffer :==
            <statement>   The input statement(s)
            '\0'          Terminating null char  (1 byte)
            <length>      Length of following current database name (size_t)
            <db_name>     Name of current database
            <flags>       Flags struct
  */
  if (! (query= (char*) thd->memdup_w_gap(packet,
                                          packet_length,
                                          1 + sizeof(size_t) + thd->db_length +
                                          QUERY_CACHE_FLAGS_SIZE)))
      return TRUE;
  query[packet_length]= '\0';
  /*
    Space to hold the name of the current database is allocated.  We
    also store this length, in case current database is changed during
    execution.  We might need to reallocate the 'query' buffer
  */
  char *len_pos = (query + packet_length + 1);
  memcpy(len_pos, (char *) &thd->db_length, sizeof(size_t));
    
  thd->set_query(query, packet_length);
  thd->rewritten_query.free();                 // free here lest PS break

  /* Reclaim some memory */
  thd->packet.shrink(thd->variables.net_buffer_length);
  thd->convert_buffer.shrink(thd->variables.net_buffer_length);

  return FALSE;
}

static void reset_one_shot_variables(THD *thd) 
{
  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_database=
    global_system_variables.collation_database;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();
  thd->variables.time_zone=
    global_system_variables.time_zone;
  thd->variables.lc_time_names= &my_locale_en_US;
  thd->one_shot_set= 0;
}


static
bool sp_process_definer(THD *thd)
{
  DBUG_ENTER("sp_process_definer");

  LEX *lex= thd->lex;

  /*
    If the definer is not specified, this means that CREATE-statement missed
    DEFINER-clause. DEFINER-clause can be missed in two cases:

      - The user submitted a statement w/o the clause. This is a normal
        case, we should assign CURRENT_USER as definer.

      - Our slave received an updated from the master, that does not
        replicate definer for stored rountines. We should also assign
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

  if (!lex->definer)
  {
    Query_arena original_arena;
    Query_arena *ps_arena= thd->activate_stmt_arena_if_needed(&original_arena);

    lex->definer= create_default_definer(thd);

    if (ps_arena)
      thd->restore_active_arena(ps_arena, &original_arena);

    /* Error has been already reported. */
    if (lex->definer == NULL)
      DBUG_RETURN(TRUE);

    if (thd->slave_thread && lex->sphead)
      lex->sphead->m_chistics->suid= SP_IS_NOT_SUID;
  }
  else
  {
    /*
      If the specified definer differs from the current user, we
      should check that the current user has SUPER privilege (in order
      to create a stored routine under another user one must have
      SUPER privilege).
    */
    if ((strcmp(lex->definer->user.str, thd->security_ctx->priv_user) ||
         my_strcasecmp(system_charset_info, lex->definer->host.str,
                       thd->security_ctx->priv_host)) &&
        check_global_access(thd, SUPER_ACL))
    {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
      DBUG_RETURN(TRUE);
    }
  }

  /* Check that the specified definer exists. Emit a warning if not. */

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!is_acl_user(lex->definer->host.str, lex->definer->user.str))
  {
    push_warning_printf(thd,
                        Sql_condition::SL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  DBUG_RETURN(FALSE);
}


/**
  Auxiliary call that opens and locks tables for LOCK TABLES statement
  and initializes the list of locked tables.

  @param thd     Thread context.
  @param tables  List of tables to be locked.

  @return FALSE in case of success, TRUE in case of error.
*/

static bool lock_tables_open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  Lock_tables_prelocking_strategy lock_tables_prelocking_strategy;
  uint counter;
  TABLE_LIST *table;

  thd->in_lock_tables= 1;

  if (open_tables(thd, &tables, &counter, 0, &lock_tables_prelocking_strategy))
    goto err;

  /*
    We allow to change temporary tables even if they were locked for read
    by LOCK TABLES. To avoid a discrepancy between lock acquired at LOCK
    TABLES time and by the statement which is later executed under LOCK TABLES
    we ensure that for temporary tables we always request a write lock (such
    discrepancy can cause problems for the storage engine).
    We don't set TABLE_LIST::lock_type in this case as this might result in
    extra warnings from THD::decide_logging_format() even though binary logging
    is totally irrelevant for LOCK TABLES.
  */
  for (table= tables; table; table= table->next_global)
    if (!table->placeholder() && table->table->s->tmp_table)
      table->table->reginfo.lock_type= TL_WRITE;

  if (lock_tables(thd, tables, counter, 0) ||
      thd->locked_tables_list.init_locked_tables(thd))
    goto err;

  thd->in_lock_tables= 0;

  return FALSE;

err:
  thd->in_lock_tables= 0;

  trans_rollback_stmt(thd);
  /*
    Need to end the current transaction, so the storage engine (InnoDB)
    can free its locks if LOCK TABLES locked some tables before finding
    that it can't lock a table in its list
  */
  trans_commit_implicit(thd);
  /* Close tables and release metadata locks. */
  close_thread_tables(thd);
  DBUG_ASSERT(!thd->locked_tables_mode);
  thd->mdl_context.release_transactional_locks();
  return TRUE;
}


/**
  Execute command saved in thd and lex->sql_command.

  @param thd                       Thread handle

  @todo
    - Invalidate the table in the query cache if something changed
    after unlocking when changes become visible.
    @todo: this is workaround. right way will be move invalidating in
    the unlock procedure.
    - TODO: use check_change_password()

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/

int
mysql_execute_command(THD *thd)
{
  int res= FALSE;
  int  up_result= 0;
  LEX  *lex= thd->lex;
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= &lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *first_table= select_lex->table_list.first;
  /* list of all tables in query */
  TABLE_LIST *all_tables;
  /* most outer SELECT_LEX_UNIT of query */
  SELECT_LEX_UNIT *unit= &lex->unit;
#ifdef HAVE_REPLICATION
  /* have table map for update for multi-update statement (BUG#37051) */
  bool have_table_map_for_update= FALSE;
#endif
  DBUG_ENTER("mysql_execute_command");
  DBUG_ASSERT(!lex->describe || is_explainable_query(lex->sql_command));

#ifdef WITH_PARTITION_STORAGE_ENGINE
  thd->work_part_info= 0;
#endif

  DBUG_ASSERT(thd->transaction.stmt.is_empty() || thd->in_sub_stmt);
  /*
    In many cases first table of main SELECT_LEX have special meaning =>
    check that it is first table in global list and relink it first in 
    queries_tables list if it is necessary (we need such relinking only
    for queries with subqueries in select list, in this case tables of
    subqueries will go to global list first)

    all_tables will differ from first_table only if most upper SELECT_LEX
    do not contain tables.

    Because of above in place where should be at least one table in most
    outer SELECT_LEX we have following check:
    DBUG_ASSERT(first_table == all_tables);
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
  */
  lex->first_lists_tables_same();
  /* should be assigned after making first tables same */
  all_tables= lex->query_tables;
  /* set context for commands which do not use setup_tables */
  select_lex->
    context.resolve_in_table_list_only(select_lex->
                                       table_list.first);

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
  */
  if ((sql_command_flags[lex->sql_command] & CF_DIAGNOSTIC_STMT) == 0)
  {
    if (all_tables)
      thd->get_stmt_da()->opt_reset_condition_info(thd->query_id);
  }

#ifdef HAVE_REPLICATION
  if (unlikely(thd->slave_thread))
  {
    if (lex->sql_command == SQLCOM_DROP_TRIGGER)
    {
      /*
        When dropping a trigger, we need to load its table name
        before checking slave filter rules.
      */
      add_table_for_trigger(thd, thd->lex->spname, 1, &all_tables);
      
      if (!all_tables)
      {
        /*
          If table name cannot be loaded,
          it means the trigger does not exists possibly because
          CREATE TRIGGER was previously skipped for this trigger
          according to slave filtering rules.
          Returning success without producing any errors in this case.
        */
        DBUG_RETURN(0);
      }
      
      // force searching in slave.cc:tables_ok() 
      all_tables->updating= 1;
    }

    /*
      For fix of BUG#37051, the master stores the table map for update
      in the Query_log_event, and the value is assigned to
      thd->variables.table_map_for_update before executing the update
      query.

      If thd->variables.table_map_for_update is set, then we are
      replicating from a new master, we can use this value to apply
      filter rules without opening all the tables. However If
      thd->variables.table_map_for_update is not set, then we are
      replicating from an old master, so we just skip this and
      continue with the old method. And of course, the bug would still
      exist for old masters.
    */
    if (lex->sql_command == SQLCOM_UPDATE_MULTI &&
        thd->table_map_for_update)
    {
      have_table_map_for_update= TRUE;
      table_map table_map_for_update= thd->table_map_for_update;
      uint nr= 0;
      TABLE_LIST *table;
      for (table=all_tables; table; table=table->next_global, nr++)
      {
        if (table_map_for_update & ((table_map)1 << nr))
          table->updating= TRUE;
        else
          table->updating= FALSE;
      }

      if (all_tables_not_ok(thd, all_tables))
      {
        /* we warn the slave SQL thread */
        my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
        if (thd->one_shot_set)
          reset_one_shot_variables(thd);
        DBUG_RETURN(0);
      }
      
      for (table=all_tables; table; table=table->next_global)
        table->updating= TRUE;
    }
    
    /*
      Check if statment should be skipped because of slave filtering
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
	!(lex->sql_command == SQLCOM_DROP_TABLE &&
          lex->drop_temporary && lex->drop_if_exists) &&
        all_tables_not_ok(thd, all_tables))
    {
      /* we warn the slave SQL thread */
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      if (thd->one_shot_set)
      {
        /*
          It's ok to check thd->one_shot_set here:

          The charsets in a MySQL 5.0 slave can change by both a binlogged
          SET ONE_SHOT statement and the event-internal charset setting, 
          and these two ways to change charsets do not seems to work
          together.

          At least there seems to be problems in the rli cache for
          charsets if we are using ONE_SHOT.  Note that this is normally no
          problem because either the >= 5.0 slave reads a 4.1 binlog (with
          ONE_SHOT) *or* or 5.0 binlog (without ONE_SHOT) but never both."
        */
        reset_one_shot_variables(thd);
      }
      DBUG_RETURN(0);
    }
    /* 
       Execute deferred events first
    */
    if (slave_execute_deferred_events(thd))
      DBUG_RETURN(-1);
  }
  else
  {
#endif /* HAVE_REPLICATION */
    /*
      When option readonly is set deny operations which change non-temporary
      tables. Except for the replication thread and the 'super' users.
    */
    if (deny_updates_if_read_only_option(thd, all_tables))
    {
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      DBUG_RETURN(-1);
    }
#ifdef HAVE_REPLICATION
  } /* endif unlikely slave */
#endif

  status_var_increment(thd->status_var.com_stat[lex->sql_command]);

  Opt_trace_start ots(thd, all_tables, lex->sql_command, &lex->var_list,
                      thd->query(), thd->query_length(), NULL,
                      thd->variables.character_set_client);

  Opt_trace_object trace_command(&thd->opt_trace);
  Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

  DBUG_ASSERT(thd->transaction.stmt.cannot_safely_rollback() == FALSE);

  switch (gtid_pre_statement_checks(thd))
  {
  case GTID_STATEMENT_EXECUTE:
    break;
  case GTID_STATEMENT_CANCEL:
    DBUG_RETURN(-1);
  case GTID_STATEMENT_SKIP:
    my_ok(thd);
    DBUG_RETURN(0);
  }

  /*
    End a active transaction so that this command will have it's
    own transaction and will also sync the binary log. If a DDL is
    not run in it's own transaction it may simply never appear on
    the slave in case the outside transaction rolls back.
  */
  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN))
  {
    /*
      Note that this should never happen inside of stored functions
      or triggers as all such statements prohibited there.
    */
    DBUG_ASSERT(! thd->in_sub_stmt);
    /* Commit or rollback the statement transaction. */
    thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
    /* Commit the normal transaction if one is active. */
    if (trans_commit_implicit(thd))
      goto error;
    /* Release metadata locks acquired in this transaction. */
    thd->mdl_context.release_transactional_locks();
  }

#ifndef DBUG_OFF
  if (lex->sql_command != SQLCOM_SET_OPTION)
    DEBUG_SYNC(thd,"before_execute_sql_command");
#endif

  /*
    Check if we are in a read-only transaction and we're trying to
    execute a statement which should always be disallowed in such cases.

    Note that this check is done after any implicit commits.
  */
  if (thd->tx_read_only &&
      (sql_command_flags[lex->sql_command] & CF_DISALLOW_IN_RO_TRANS))
  {
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
    Pre-open temporary tables to simplify privilege checking
    for statements which need this.
  */
  if (sql_command_flags[lex->sql_command] & CF_PREOPEN_TMP_TABLES)
  {
    if (open_temporary_tables(thd, all_tables))
      goto error;
  }

  switch (lex->sql_command) {

  case SQLCOM_SHOW_STATUS:
  {
    system_status_var old_status_var= thd->status_var;
    thd->initial_status_var= &old_status_var;

    if (!(res= select_precheck(thd, lex, all_tables, first_table)))
      res= execute_sqlcom_select(thd, all_tables);

    /* Don't log SHOW STATUS commands to slow query log */
    thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED);
    /*
      restore status variables, as we don't want 'show status' to cause
      changes
    */
    mysql_rwlock_wrlock(&LOCK_status);
    add_diff_to_status(&global_status_var, &thd->status_var,
                       &old_status_var);
    thd->status_var= old_status_var;
    mysql_rwlock_unlock(&LOCK_status);
    break;
  }
  case SQLCOM_SHOW_EVENTS:
#ifndef HAVE_EVENT_SCHEDULER
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "embedded server");
    break;
#endif
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STATUS_FUNC:
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TRIGGERS:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_PLUGINS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_PROFILE:
  case SQLCOM_SELECT:
  {
    thd->status_var.last_query_cost= 0.0;
    thd->status_var.last_query_partial_plans= 0;

    if ((res= select_precheck(thd, lex, all_tables, first_table)))
      break;

    res= execute_sqlcom_select(thd, all_tables);
    break;
  }
case SQLCOM_PREPARE:
  {
    mysql_sql_stmt_prepare(thd);
    break;
  }
  case SQLCOM_EXECUTE:
  {
    mysql_sql_stmt_execute(thd);
    break;
  }
  case SQLCOM_DEALLOCATE_PREPARE:
  {
    mysql_sql_stmt_close(thd);
    break;
  }
  case SQLCOM_DO:
    if (check_table_access(thd, SELECT_ACL, all_tables, FALSE, UINT_MAX, FALSE)
        || open_and_lock_tables(thd, all_tables, TRUE, 0))
      goto error;

    res= mysql_do(thd, *lex->insert_list);
    break;

  case SQLCOM_EMPTY_QUERY:
    my_ok(thd);
    break;

  case SQLCOM_HELP:
    res= mysqld_help(thd,lex->help_arg);
    break;

#ifndef EMBEDDED_LIBRARY
  case SQLCOM_PURGE:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    /* PURGE MASTER LOGS TO 'file' */
    res = purge_master_logs(thd, lex->to_log);
    break;
  }
  case SQLCOM_PURGE_BEFORE:
  {
    Item *it;

    if (check_global_access(thd, SUPER_ACL))
      goto error;
    /* PURGE MASTER LOGS BEFORE 'data' */
    it= (Item *)lex->value_list.head();
    if ((!it->fixed && it->fix_fields(lex->thd, &it)) ||
        it->check_cols(1))
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "PURGE LOGS BEFORE");
      goto error;
    }
    it= new Item_func_unix_timestamp(it);
    /*
      it is OK only emulate fix_fieds, because we need only
      value of constant
    */
    it->quick_fix_field();
    time_t purge_time= static_cast<time_t>(it->val_int());
    if (thd->is_error())
      goto error;
    res = purge_master_logs_before_date(thd, purge_time);
    break;
  }
#endif
  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      ((1L << (uint) Sql_condition::SL_NOTE) |
			       (1L << (uint) Sql_condition::SL_WARNING) |
			       (1L << (uint) Sql_condition::SL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      (1L << (uint) Sql_condition::SL_ERROR));
    break;
  }
  case SQLCOM_SHOW_PROFILES:
  {
#if defined(ENABLED_PROFILING)
    thd->profiling.discard_current_query();
    res= thd->profiling.show_profiles();
    if (res)
      goto error;
#else
    my_error(ER_FEATURE_DISABLED, MYF(0), "SHOW PROFILES", "enable-profiling");
    goto error;
#endif
    break;
  }

#ifdef HAVE_REPLICATION
  case SQLCOM_SHOW_SLAVE_HOSTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res= show_slave_hosts(thd);
    break;
  }
  case SQLCOM_SHOW_RELAYLOG_EVENTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = mysql_show_relaylog_events(thd);
    break;
  }
  case SQLCOM_SHOW_BINLOG_EVENTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = mysql_show_binlog_events(thd);
    break;
  }
#endif

  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege,
                     &first_table->grant.m_internal,
                     0, 0))
      goto error;
    res= mysql_assign_to_keycache(thd, first_table, &lex->ident);
    break;
  }
  case SQLCOM_PRELOAD_KEYS:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege,
                     &first_table->grant.m_internal,
                     0, 0))
      goto error;
    res = mysql_preload_keys(thd, first_table);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_CHANGE_MASTER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= change_master(thd, active_mi);
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    mysql_mutex_lock(&LOCK_active_mi);
    res= show_slave_status(thd, active_mi);
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT_NONBLOCKING:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    DBUG_EXECUTE_IF("simulate_hold_show_slave_status_nonblocking",
                    my_sleep(10000000););
    res= show_slave_status(thd, active_mi);
    break;
  }
  case SQLCOM_SHOW_MASTER_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    res = show_master_status(thd);
    break;
  }

#endif /* HAVE_REPLICATION */
  case SQLCOM_SHOW_ENGINE_STATUS:
    {
      if (check_global_access(thd, PROCESS_ACL))
        goto error;
      res = ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_STATUS);
      break;
    }
  case SQLCOM_SHOW_ENGINE_MUTEX:
    {
      if (check_global_access(thd, PROCESS_ACL))
        goto error;
      res = ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_MUTEX);
      break;
    }
  case SQLCOM_CREATE_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    bool link_to_local;
    TABLE_LIST *create_table= first_table;
    TABLE_LIST *select_tables= lex->create_last_non_select_table->next_global;

    /*
      Code below (especially in mysql_create_table() and select_create
      methods) may modify HA_CREATE_INFO structure in LEX, so we have to
      use a copy of this structure to make execution prepared statement-
      safe. A shallow copy is enough as this code won't modify any memory
      referenced from this structure.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    /*
      We need to copy alter_info for the same reasons of re-execution
      safety, only in case of Alter_info we have to do (almost) a deep
      copy.
    */
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error)
    {
      /* If out of memory when creating a copy of alter_info. */
      res= 1;
      goto end_with_restore_list;
    }

    if ((res= create_table_precheck(thd, select_tables, create_table)))
      goto end_with_restore_list;

    /* Might have been updated in create_table_precheck */
    create_info.alias= create_table->alias;

    /* Fix names if symlinked or relocated tables */
    if (append_file_to_dir(thd, &create_info.data_file_name,
			   create_table->table_name) ||
	append_file_to_dir(thd, &create_info.index_file_name,
			   create_table->table_name))
      goto end_with_restore_list;

    /*
      If no engine type was given, work out the default now
      rather than at parse-time.
    */
    if (!(create_info.used_fields & HA_CREATE_USED_ENGINE))
      create_info.db_type= create_info.options & HA_LEX_CREATE_TMP_TABLE ?
              ha_default_temp_handlerton(thd) : ha_default_handlerton(thd);
    /*
      If we are using SET CHARSET without DEFAULT, add an implicit
      DEFAULT to not confuse old users. (This may change).
    */
    if ((create_info.used_fields &
	 (HA_CREATE_USED_DEFAULT_CHARSET | HA_CREATE_USED_CHARSET)) ==
	HA_CREATE_USED_CHARSET)
    {
      create_info.used_fields&= ~HA_CREATE_USED_CHARSET;
      create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
      create_info.default_table_charset= create_info.table_charset;
      create_info.table_charset= 0;
    }

#ifdef WITH_PARTITION_STORAGE_ENGINE
    {
      partition_info *part_info= thd->lex->part_info;
      if (part_info && !(part_info= thd->lex->part_info->get_clone()))
      {
        res= -1;
        goto end_with_restore_list;
      }
      thd->work_part_info= part_info;
    }
#endif

    if (select_lex->item_list.elements)		// With select
    {
      select_result *result;

      /*
        CREATE TABLE...IGNORE/REPLACE SELECT... can be unsafe, unless
        ORDER BY PRIMARY KEY clause is used in SELECT statement. We therefore
        use row based logging if mixed or row based logging is available.
        TODO: Check if the order of the output of the select statement is
        deterministic. Waiting for BUG#42415
      */
      if(lex->ignore)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_IGNORE_SELECT);
      
      if(lex->duplicates == DUP_REPLACE)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_REPLACE_SELECT);

      /*
        If:
        a) we inside an SP and there was NAME_CONST substitution,
        b) binlogging is on (STMT mode),
        c) we log the SP as separate statements
        raise a warning, as it may cause problems
        (see 'NAME_CONST issues' in 'Binary Logging of Stored Programs')
       */
      if (thd->query_name_consts && 
          mysql_bin_log.is_open() &&
          thd->variables.binlog_format == BINLOG_FORMAT_STMT &&
          !mysql_bin_log.is_query_in_union(thd, thd->query_id))
      {
        List_iterator_fast<Item> it(select_lex->item_list);
        Item *item;
        uint splocal_refs= 0;
        /* Count SP local vars in the top-level SELECT list */
        while ((item= it++))
        {
          if (item->is_splocal())
            splocal_refs++;
        }
        /*
          If it differs from number of NAME_CONST substitution applied,
          we may have a SOME_FUNC(NAME_CONST()) in the SELECT list,
          that may cause a problem with binary log (see BUG#35383),
          raise a warning. 
        */
        if (splocal_refs != thd->query_name_consts)
          push_warning(thd, 
                       Sql_condition::SL_WARNING,
                       ER_UNKNOWN_ERROR,
"Invoked routine ran a statement that may cause problems with "
"binary log, see 'NAME_CONST issues' in 'Binary Logging of Stored Programs' "
"section of the manual.");
      }
      
      select_lex->options|= SELECT_NO_UNLOCK;
      unit->set_limit(select_lex);

      /*
        Disable non-empty MERGE tables with CREATE...SELECT. Too
        complicated. See Bug #26379. Empty MERGE tables are read-only
        and don't allow CREATE...SELECT anyway.
      */
      if (create_info.used_fields & HA_CREATE_USED_UNION)
      {
        my_error(ER_WRONG_OBJECT, MYF(0), create_table->db,
                 create_table->table_name, "BASE TABLE");
        res= 1;
        goto end_with_restore_list;
      }

      if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
      {
        /* The table already exists */
        if (create_table->table || create_table->view)
        {
          if (create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS)
          {
            push_warning_printf(thd, Sql_condition::SL_NOTE,
                                ER_TABLE_EXISTS_ERROR,
                                ER(ER_TABLE_EXISTS_ERROR),
                                create_info.alias);
            my_ok(thd);
          }
          else
          {
            my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_info.alias);
            res= 1;
          }
          goto end_with_restore_list;
        }

        /*
          Remove target table from main select and name resolution
          context. This can't be done earlier as it will break view merging in
          statements like "CREATE TABLE IF NOT EXISTS existing_view SELECT".
        */
        lex->unlink_first_table(&link_to_local);

        /* Updating any other table is prohibited in CTS statement */
        for (TABLE_LIST *table= lex->query_tables; table;
             table= table->next_global)
          if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
          {
            res= 1;
            my_error(ER_CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, MYF(0),
                     table->table_name, create_info.alias);
            goto end_with_restore_list;
          }

        /*
          select_create is currently not re-execution friendly and
          needs to be created for every execution of a PS/SP.
        */
        if ((result= new select_create(create_table,
                                       &create_info,
                                       &alter_info,
                                       select_lex->item_list,
                                       lex->duplicates,
                                       lex->ignore,
                                       select_tables)))
        {
          /*
            CREATE from SELECT give its SELECT_LEX for SELECT,
            and item_list belong to SELECT
          */
          res= handle_select(thd, result, 0);
          delete result;
        }

        lex->link_first_table_back(create_table, link_to_local);
      }
    }
    else
    {
      /* regular create */
      if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
      {
        /* CREATE TABLE ... LIKE ... */
        res= mysql_create_like_table(thd, create_table, select_tables,
                                     &create_info);
      }
      else
      {
        /* Regular CREATE TABLE */
        res= mysql_create_table(thd, create_table,
                                &create_info, &alter_info);
      }
      if (!res)
        my_ok(thd);
    }

end_with_restore_list:
    break;
  }
  case SQLCOM_CREATE_INDEX:
    /* Fall through */
  case SQLCOM_DROP_INDEX:
  /*
    CREATE INDEX and DROP INDEX are implemented by calling ALTER
    TABLE with proper arguments.

    In the future ALTER TABLE will notice that the request is to
    only add indexes and create these one by one for the existing
    table without having to do a full rebuild.
  */
  {
    /* Prepare stack copies to be re-execution safe */
    HA_CREATE_INFO create_info;
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error) /* out of memory creating a copy of alter_info */
      goto error;

    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_one_table_access(thd, INDEX_ACL, all_tables))
      goto error; /* purecov: inspected */
    /*
      Currently CREATE INDEX or DROP INDEX cause a full table rebuild
      and thus classify as slow administrative statements just like
      ALTER TABLE.
    */
    thd->enable_slow_log= opt_log_slow_admin_statements;

    memset(&create_info, 0, sizeof(create_info));
    create_info.db_type= 0;
    create_info.row_type= ROW_TYPE_NOT_USED;
    create_info.default_table_charset= thd->variables.collation_database;

    res= mysql_alter_table(thd, first_table->db, first_table->table_name,
                           &create_info, first_table, &alter_info,
                           0, (ORDER*) 0, 0);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_SLAVE_START:
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= start_slave(thd, active_mi, 1 /* net report*/);
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SLAVE_STOP:
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
  if (thd->locked_tables_mode ||
      thd->in_active_multi_stmt_transaction() || thd->global_read_lock.is_acquired())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    goto error;
  }
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= stop_slave(thd, active_mi, 1 /* net report*/);
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif /* HAVE_REPLICATION */

  case SQLCOM_RENAME_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *table;
    for (table= first_table; table; table= table->next_local->next_local)
    {
      if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
                       &table->grant.privilege,
                       &table->grant.m_internal,
                       0, 0) ||
          check_access(thd, INSERT_ACL | CREATE_ACL, table->next_local->db,
                       &table->next_local->grant.privilege,
                       &table->next_local->grant.m_internal,
                       0, 0))
	goto error;
      TABLE_LIST old_list, new_list;
      /*
        we do not need initialize old_list and new_list because we will
        come table[0] and table->next[0] there
      */
      old_list= table[0];
      new_list= table->next_local[0];
      if (check_grant(thd, ALTER_ACL | DROP_ACL, &old_list, FALSE, 1, FALSE) ||
         (!test_all_bits(table->next_local->grant.privilege,
                         INSERT_ACL | CREATE_ACL) &&
          check_grant(thd, INSERT_ACL | CREATE_ACL, &new_list, FALSE, 1,
                      FALSE)))
        goto error;
    }

    if (mysql_rename_tables(thd, first_table, 0))
      goto error;
    break;
  }
#ifndef EMBEDDED_LIBRARY
  case SQLCOM_SHOW_BINLOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0)); /* purecov: inspected */
    goto error;
#else
    {
      if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
	goto error;
      res = show_binlogs(thd);
      break;
    }
#endif
#endif /* EMBEDDED_LIBRARY */
  case SQLCOM_SHOW_CREATE:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0)); /* purecov: inspected */
    goto error;
#else
    {
     /*
        Access check:
        SHOW CREATE TABLE require any privileges on the table level (ie
        effecting all columns in the table).
        SHOW CREATE VIEW require the SHOW_VIEW and SELECT ACLs on the table
        level.
        NOTE: SHOW_VIEW ACL is checked when the view is created.
      */

      DBUG_PRINT("debug", ("lex->only_view: %d, table: %s.%s",
                           lex->only_view,
                           first_table->db, first_table->table_name));
      if (lex->only_view)
      {
        if (check_table_access(thd, SELECT_ACL, first_table, FALSE, 1, FALSE))
        {
          DBUG_PRINT("debug", ("check_table_access failed"));
          my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                  "SHOW", thd->security_ctx->priv_user,
                  thd->security_ctx->host_or_ip, first_table->alias);
          goto error;
        }
        DBUG_PRINT("debug", ("check_table_access succeeded"));

        /* Ignore temporary tables if this is "SHOW CREATE VIEW" */
        first_table->open_type= OT_BASE_ONLY;

      }
      else
      {
        /*
          Temporary tables should be opened for SHOW CREATE TABLE, but not
          for SHOW CREATE VIEW.
        */
        if (open_temporary_tables(thd, all_tables))
          goto error;

        /*
          The fact that check_some_access() returned FALSE does not mean that
          access is granted. We need to check if first_table->grant.privilege
          contains any table-specific privilege.
        */
        DBUG_PRINT("debug", ("first_table->grant.privilege: %lx",
                             first_table->grant.privilege));
        if (check_some_access(thd, SHOW_CREATE_TABLE_ACLS, first_table) ||
            (first_table->grant.privilege & SHOW_CREATE_TABLE_ACLS) == 0)
        {
          my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                  "SHOW", thd->security_ctx->priv_user,
                  thd->security_ctx->host_or_ip, first_table->alias);
          goto error;
        }
      }

      /* Access is granted. Execute the command.  */
      res= mysqld_show_create(thd, first_table);
      break;
    }
#endif
  case SQLCOM_CHECKSUM:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL, all_tables,
                           FALSE, UINT_MAX, FALSE))
      goto error; /* purecov: inspected */

    res = mysql_checksum_table(thd, first_table, &lex->check_opt);
    break;
  }
  case SQLCOM_UPDATE:
  {
    ha_rows found= 0, updated= 0;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (update_precheck(thd, all_tables))
      break;

    /*
      UPDATE IGNORE can be unsafe. We therefore use row based
      logging if mixed or row based logging is available.
      TODO: Check if the order of the output of the select statement is
      deterministic. Waiting for BUG#42415
    */
    if (lex->ignore)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UPDATE_IGNORE);

    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);
    MYSQL_UPDATE_START(thd->query());
    res= (up_result= mysql_update(thd, all_tables,
                                  select_lex->item_list,
                                  lex->value_list,
                                  select_lex->where,
                                  select_lex->order_list.elements,
                                  select_lex->order_list.first,
                                  unit->select_limit_cnt,
                                  lex->duplicates, lex->ignore,
                                  &found, &updated));
    MYSQL_UPDATE_DONE(res, found, updated);
    /* mysql_update return 2 if we need to switch to multi-update */
    if (up_result != 2)
      break;
    /* Fall through */
  }
  case SQLCOM_UPDATE_MULTI:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    /* if we switched from normal update, rights are checked */
    if (up_result != 2)
    {
      if ((res= multi_update_precheck(thd, all_tables)))
        break;
    }
    else
      res= 0;

    res= mysql_multi_update_prepare(thd);

#ifdef HAVE_REPLICATION
    /* Check slave filtering rules */
    if (unlikely(thd->slave_thread && !have_table_map_for_update))
    {
      if (all_tables_not_ok(thd, all_tables))
      {
        if (res!= 0)
        {
          res= 0;             /* don't care of prev failure  */
          thd->clear_error(); /* filters are of highest prior */
        }
        /* we warn the slave SQL thread */
        my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
        break;
      }
      if (res)
        break;
    }
    else
    {
#endif /* HAVE_REPLICATION */
      if (res)
        break;
      if (opt_readonly &&
	  !(thd->security_ctx->master_access & SUPER_ACL) &&
	  some_non_temp_table_to_be_updated(thd, all_tables))
      {
	my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
	break;
      }
#ifdef HAVE_REPLICATION
    }  /* unlikely */
#endif
    {
      multi_update *result_obj;
      MYSQL_MULTI_UPDATE_START(thd->query());
      res= mysql_multi_update(thd, all_tables,
                              &select_lex->item_list,
                              &lex->value_list,
                              select_lex->where,
                              select_lex->options,
                              lex->duplicates,
                              lex->ignore,
                              unit,
                              select_lex,
                              &result_obj);
      if (result_obj)
      {
        MYSQL_MULTI_UPDATE_DONE(res, result_obj->num_found(),
                                result_obj->num_updated());
        res= FALSE; /* Ignore errors here */
        delete result_obj;
      }
      else
      {
        MYSQL_MULTI_UPDATE_DONE(1, 0, 0);
      }
    }
    break;
  }
  case SQLCOM_REPLACE:
#ifndef DBUG_OFF
    if (mysql_bin_log.is_open())
    {
      /*
        Generate an incident log event before writing the real event
        to the binary log.  We put this event is before the statement
        since that makes it simpler to check that the statement was
        not executed on the slave (since incidents usually stop the
        slave).

        Observe that any row events that are generated will be
        generated before.

        This is only for testing purposes and will not be present in a
        release build.
      */

      Incident incident= INCIDENT_NONE;
      DBUG_PRINT("debug", ("Just before generate_incident()"));
      DBUG_EXECUTE_IF("incident_database_resync_on_replace",
                      incident= INCIDENT_LOST_EVENTS;);
      if (incident)
      {
        Incident_log_event ev(thd, incident);
        if (mysql_bin_log.write_incident(&ev, true/*need_lock_log=true*/))
        {
          res= 1;
          break;
        }
      }
      DBUG_PRINT("debug", ("Just after generate_incident()"));
    }
#endif
  case SQLCOM_INSERT:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);

    if ((res= open_temporary_tables(thd, all_tables)))
      break;

    if ((res= insert_precheck(thd, all_tables)))
      break;

    MYSQL_INSERT_START(thd->query());
    res= mysql_insert(thd, all_tables, lex->field_list, lex->many_values,
		      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);
    MYSQL_INSERT_DONE(res, (ulong) thd->get_row_count_func());
    /*
      If we have inserted into a VIEW, and the base table has
      AUTO_INCREMENT column, but this column is not accessible through
      a view, then we should restore LAST_INSERT_ID to the value it
      had before the statement.
    */
    if (first_table->view && !first_table->contain_auto_increment)
      thd->first_successful_insert_id_in_cur_stmt=
        thd->first_successful_insert_id_in_prev_stmt;

    DBUG_EXECUTE_IF("after_mysql_insert",
                    {
                      const char act[]=
                        "now "
                        "wait_for signal.continue";
                      DBUG_ASSERT(opt_debug_sync_timeout > 0);
                      DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                         STRING_WITH_LEN(act)));
                    };);
    break;
  }
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    select_insert *sel_result;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;
    /*
      INSERT...SELECT...ON DUPLICATE KEY UPDATE/REPLACE SELECT/
      INSERT...IGNORE...SELECT can be unsafe, unless ORDER BY PRIMARY KEY
      clause is used in SELECT statement. We therefore use row based
      logging if mixed or row based logging is available.
      TODO: Check if the order of the output of the select statement is
      deterministic. Waiting for BUG#42415
    */
    if (lex->sql_command == SQLCOM_INSERT_SELECT &&
        lex->duplicates == DUP_UPDATE)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_SELECT_UPDATE);

    if (lex->sql_command == SQLCOM_INSERT_SELECT && lex->ignore)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_IGNORE_SELECT);

    if (lex->sql_command == SQLCOM_REPLACE_SELECT)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_REPLACE_SELECT);

    /* Don't unlock tables until command is written to binary log */
    select_lex->options|= SELECT_NO_UNLOCK;

    unit->set_limit(select_lex);

    if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
    {
      MYSQL_INSERT_SELECT_START(thd->query());
      /* Skip first table, which is the table we are inserting in */
      TABLE_LIST *second_table= first_table->next_local;
      select_lex->table_list.first= second_table;
      select_lex->context.table_list= 
        select_lex->context.first_name_resolution_table= second_table;
      res= mysql_insert_select_prepare(thd);
      if (!res && (sel_result= new select_insert(first_table,
                                                 first_table->table,
                                                 &lex->field_list,
                                                 &lex->field_list,
                                                 &lex->update_list,
                                                 &lex->value_list,
                                                 lex->duplicates,
                                                 lex->ignore)))
      {
        if (lex->describe)
          res= explain_multi_table_modification(thd, sel_result);
        else
          res= handle_select(thd, sel_result, OPTION_SETUP_TABLES_DONE);
        delete sel_result;
      }
      /* revert changes for SP */
      MYSQL_INSERT_SELECT_DONE(res, (ulong) thd->get_row_count_func());
      select_lex->table_list.first= first_table;
    }
    /*
      If we have inserted into a VIEW, and the base table has
      AUTO_INCREMENT column, but this column is not accessible through
      a view, then we should restore LAST_INSERT_ID to the value it
      had before the statement.
    */
    if (first_table->view && !first_table->contain_auto_increment)
      thd->first_successful_insert_id_in_cur_stmt=
        thd->first_successful_insert_id_in_prev_stmt;

    break;
  }
  case SQLCOM_DELETE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= delete_precheck(thd, all_tables)))
      break;
    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);

    MYSQL_DELETE_START(thd->query());
    res = mysql_delete(thd, all_tables, select_lex->where,
                       &select_lex->order_list,
                       unit->select_limit_cnt, select_lex->options);
    MYSQL_DELETE_DONE(res, (ulong) thd->get_row_count_func());
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
    uint del_table_count;
    multi_delete *del_result;

    if ((res= multi_delete_precheck(thd, all_tables)))
      break;

    /* condition will be TRUE on SP re-excuting */
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (add_item_to_list(thd, new Item_null()))
      goto error;

    THD_STAGE_INFO(thd, stage_init);
    if ((res= open_normal_and_derived_tables(thd, all_tables, 0)))
      break;

    MYSQL_MULTI_DELETE_START(thd->query());
    if ((res= mysql_multi_delete_prepare(thd, &del_table_count)))
    {
      MYSQL_MULTI_DELETE_DONE(1, 0);
      goto error;
    }

    if (!thd->is_fatal_error &&
        (del_result= new multi_delete(aux_tables, del_table_count)))
    {
      if (lex->describe)
        res= explain_multi_table_modification(thd, del_result);
      else
      {
        res= mysql_select(thd,
                          select_lex->get_table_list(),
                          select_lex->with_wild,
                          select_lex->item_list,
                          select_lex->where,
                          (SQL_I_List<ORDER> *)NULL, (SQL_I_List<ORDER> *)NULL,
                          (Item *)NULL,
                          (select_lex->options | thd->variables.option_bits |
                          SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                          OPTION_SETUP_TABLES_DONE) & ~OPTION_BUFFER_RESULT,
                          del_result, unit, select_lex);
        res|= thd->is_error();
        if (res)
          del_result->abort_result_set();
      }
      MYSQL_MULTI_DELETE_DONE(res, del_result->num_deleted());
      delete del_result;
    }
    else
    {
      res= TRUE;                                // Error
      MYSQL_MULTI_DELETE_DONE(1, 0);
    }
    break;
  }
  case SQLCOM_DROP_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (!lex->drop_temporary)
    {
      if (check_table_access(thd, DROP_ACL, all_tables, FALSE, UINT_MAX, FALSE))
	goto error;				/* purecov: inspected */
    }
    /* DDL and binlog write order are protected by metadata locks. */
    res= mysql_rm_table(thd, first_table, lex->drop_if_exists,
			lex->drop_temporary);
  }
  break;
  case SQLCOM_SHOW_PROCESSLIST:
    if (!thd->security_ctx->priv_user[0] &&
        check_global_access(thd,PROCESS_ACL))
      break;
    mysqld_list_processes(thd,
			  (thd->security_ctx->master_access & PROCESS_ACL ?
                           NullS :
                           thd->security_ctx->priv_user),
                          lex->verbose);
    break;
  case SQLCOM_SHOW_PRIVILEGES:
    res= mysqld_show_privileges(thd);
    break;
  case SQLCOM_SHOW_ENGINE_LOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0));	/* purecov: inspected */
    goto error;
#else
    {
      if (check_access(thd, FILE_ACL, any_db, NULL, NULL, 0, 0))
	goto error;
      res= ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_LOGS);
      break;
    }
#endif
  case SQLCOM_CHANGE_DB:
  {
    LEX_STRING db_str= { (char *) select_lex->db, strlen(select_lex->db) };

    if (!mysql_change_db(thd, &db_str, FALSE))
      my_ok(thd);

    break;
  }

  case SQLCOM_LOAD:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    uint privilege= (lex->duplicates == DUP_REPLACE ?
		     INSERT_ACL | DELETE_ACL : INSERT_ACL) |
                    (lex->local_file ? 0 : FILE_ACL);

    if (lex->local_file)
    {
      if (!(thd->client_capabilities & CLIENT_LOCAL_FILES) ||
          !opt_local_infile)
      {
	my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND), MYF(0));
	goto error;
      }
    }

    if (check_one_table_access(thd, privilege, all_tables))
      goto error;

    res= mysql_load(thd, lex->exchange, first_table, lex->field_list,
                    lex->update_list, lex->value_list, lex->duplicates,
                    lex->ignore, (bool) lex->local_file);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;

    if ((check_table_access(thd, SELECT_ACL, all_tables, FALSE, UINT_MAX, FALSE)
         || open_and_lock_tables(thd, all_tables, TRUE, 0)))
      goto error;
    if (!(res= sql_set_variables(thd, lex_var_list)))
    {
      /*
        If the previous command was a SET ONE_SHOT, we don't want to forget
        about the ONE_SHOT property of that SET. So we use a |= instead of = .
      */
      thd->one_shot_set|= lex->one_shot_set;
      my_ok(thd);
    }
    else
    {
      /*
        We encountered some sort of error, but no message was sent.
        Send something semi-generic here since we don't know which
        assignment in the list caused the error.
      */
      if (!thd->is_error())
        my_error(ER_WRONG_ARGUMENTS,MYF(0),"SET");
      goto error;
    }

    break;
  }

  case SQLCOM_UNLOCK_TABLES:
    /*
      It is critical for mysqldump --single-transaction --master-data that
      UNLOCK TABLES does not implicitely commit a connection which has only
      done FLUSH TABLES WITH READ LOCK + BEGIN. If this assumption becomes
      false, mysqldump will not work.
    */
    if (thd->variables.option_bits & OPTION_TABLE_LOCK)
    {
      res= trans_commit_implicit(thd);
      thd->locked_tables_list.unlock_locked_tables(thd);
      thd->mdl_context.release_transactional_locks();
      thd->variables.option_bits&= ~(OPTION_TABLE_LOCK);
    }
    if (thd->global_read_lock.is_acquired())
      thd->global_read_lock.unlock_global_read_lock(thd);
    if (res)
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_LOCK_TABLES:
    /* We must end the transaction first, regardless of anything */
    res= trans_commit_implicit(thd);
    thd->locked_tables_list.unlock_locked_tables(thd);
    /* Release transactional metadata locks. */
    thd->mdl_context.release_transactional_locks();
    if (res)
      goto error;

    /*
      Here we have to pre-open temporary tables for LOCK TABLES.

      CF_PREOPEN_TMP_TABLES is not set for this SQL statement simply
      because LOCK TABLES calls close_thread_tables() as a first thing
      (it's called from unlock_locked_tables() above). So even if
      CF_PREOPEN_TMP_TABLES was set and the tables would be pre-opened
      in a usual way, they would have been closed.
    */

    if (open_temporary_tables(thd, all_tables))
      goto error;

    if (lock_tables_precheck(thd, all_tables))
      goto error;

    thd->variables.option_bits|= OPTION_TABLE_LOCK;

    res= lock_tables_open_and_lock_tables(thd, all_tables);

    if (res)
    {
      thd->variables.option_bits&= ~(OPTION_TABLE_LOCK);
    }
    else
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->variables.query_cache_wlock_invalidate)
        query_cache.invalidate_locked_for_write(first_table);
#endif /*HAVE_QUERY_CACHE*/
      my_ok(thd);
    }
    break;
  case SQLCOM_CREATE_DB:
  {
    /*
      As mysql_create_db() may modify HA_CREATE_INFO structure passed to
      it, we need to use a copy of LEX::create_info to make execution
      prepared statement- safe.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    char *alias;
    if (!(alias=thd->strmake(lex->name.str, lex->name.length)) ||
        (check_and_convert_db_name(&lex->name, FALSE) != IDENT_NAME_OK))
      break;
    /*
      If in a slave thread :
      CREATE DATABASE DB was certainly not preceded by USE DB.
      For that reason, db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, CREATE_ACL, lex->name.str, NULL, NULL, 1, 0))
      break;
    res= mysql_create_db(thd,(lower_case_table_names == 2 ? alias :
                              lex->name.str), &create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (check_and_convert_db_name(&lex->name, FALSE) != IDENT_NAME_OK)
      break;
    /*
      If in a slave thread :
      DROP DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the 
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, DROP_ACL, lex->name.str, NULL, NULL, 1, 0))
      break;
    res= mysql_rm_db(thd, lex->name.str, lex->drop_if_exists, 0);
    break;
  }
  case SQLCOM_ALTER_DB_UPGRADE:
  {
    LEX_STRING *db= & lex->name;
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      res= 1;
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_and_convert_db_name(db, FALSE) != IDENT_NAME_OK)
      break;
    if (check_access(thd, ALTER_ACL, db->str, NULL, NULL, 1, 0) ||
        check_access(thd, DROP_ACL, db->str, NULL, NULL, 1, 0) ||
        check_access(thd, CREATE_ACL, db->str, NULL, NULL, 1, 0))
    {
      res= 1;
      break;
    }
    res= mysql_upgrade_db(thd, db);
    if (!res)
      my_ok(thd);
    break;
  }
  case SQLCOM_ALTER_DB:
  {
    LEX_STRING *db= &lex->name;
    HA_CREATE_INFO create_info(lex->create_info);
    if (check_and_convert_db_name(db, FALSE) != IDENT_NAME_OK)
      break;
    /*
      If in a slave thread :
      ALTER DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, ALTER_ACL, db->str, NULL, NULL, 1, 0))
      break;
    res= mysql_alter_db(thd, db->str, &create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    DBUG_EXECUTE_IF("4x_server_emul",
                    my_error(ER_UNKNOWN_ERROR, MYF(0)); goto error;);
    if (check_and_convert_db_name(&lex->name, TRUE) != IDENT_NAME_OK)
      break;
    res= mysqld_show_create_db(thd, lex->name.str, &lex->create_info);
    break;
  }
  case SQLCOM_CREATE_EVENT:
  case SQLCOM_ALTER_EVENT:
  #ifdef HAVE_EVENT_SCHEDULER
  do
  {
    DBUG_ASSERT(lex->event_parse_data);
    if (lex->table_or_sp_used())
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Usage of subqueries or stored "
               "function calls as part of this statement");
      break;
    }

    res= sp_process_definer(thd);
    if (res)
      break;

    switch (lex->sql_command) {
    case SQLCOM_CREATE_EVENT:
    {
      bool if_not_exists= (lex->create_info.options &
                           HA_LEX_CREATE_IF_NOT_EXISTS);
      res= Events::create_event(thd, lex->event_parse_data, if_not_exists);
      break;
    }
    case SQLCOM_ALTER_EVENT:
      res= Events::update_event(thd, lex->event_parse_data,
                                lex->spname ? &lex->spname->m_db : NULL,
                                lex->spname ? &lex->spname->m_name : NULL);
      break;
    default:
      DBUG_ASSERT(0);
    }
    DBUG_PRINT("info",("DDL error code=%d", res));
    if (!res)
      my_ok(thd);

  } while (0);
  /* Don't do it, if we are inside a SP */
  if (!thd->sp_runtime_ctx)
  {
    delete lex->sphead;
    lex->sphead= NULL;
  }
  /* lex->unit.cleanup() is called outside, no need to call it here */
  break;
  case SQLCOM_SHOW_CREATE_EVENT:
    res= Events::show_create_event(thd, lex->spname->m_db,
                                   lex->spname->m_name);
    break;
  case SQLCOM_DROP_EVENT:
    if (!(res= Events::drop_event(thd,
                                  lex->spname->m_db, lex->spname->m_name,
                                  lex->drop_if_exists)))
      my_ok(thd);
    break;
#else
    my_error(ER_NOT_SUPPORTED_YET,MYF(0),"embedded server");
    break;
#endif
  case SQLCOM_CREATE_FUNCTION:                  // UDF function
  {
    if (check_access(thd, INSERT_ACL, "mysql", NULL, NULL, 1, 0))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_create_function(thd, &lex->udf)))
      my_ok(thd);
#else
    my_error(ER_CANT_OPEN_LIBRARY, MYF(0), lex->udf.dl, 0, "feature disabled");
    res= TRUE;
#endif
    break;
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_CREATE_USER:
  {
    if (check_access(thd, INSERT_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    /* Conditionally writes to binlog */
    if (!(res= mysql_create_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_DROP_USER:
  {
    if (check_access(thd, DELETE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    /* Conditionally writes to binlog */
    if (!(res= mysql_drop_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_RENAME_USER:
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    /* Conditionally writes to binlog */
    if (!(res= mysql_rename_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE_ALL:
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;

    /* Replicate current user as grantor */
    thd->binlog_invoker();

    /* Conditionally writes to binlog */
    if (!(res = mysql_revoke_all(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  {
    if (lex->type != TYPE_ENUM_PROXY &&
        check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
                     first_table ?  first_table->db : select_lex->db,
                     first_table ? &first_table->grant.privilege : NULL,
                     first_table ? &first_table->grant.m_internal : NULL,
                     first_table ? 0 : 1, 0))
      goto error;

    /* Replicate current user as grantor */
    thd->binlog_invoker();

    if (thd->security_ctx->user)              // If not replication
    {
      LEX_USER *user, *tmp_user;
      bool first_user= TRUE;

      List_iterator <LEX_USER> user_list(lex->users_list);
      while ((tmp_user= user_list++))
      {
        if (!(user= get_current_user(thd, tmp_user)))
          goto error;
        if (specialflag & SPECIAL_NO_RESOLVE &&
            hostname_requires_resolving(user->host.str))
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_HOSTNAME_WONT_WORK,
                              ER(ER_WARN_HOSTNAME_WONT_WORK));
        // Are we trying to change a password of another user
        DBUG_ASSERT(user->host.str != 0);

        /*
          GRANT/REVOKE PROXY has the target user as a first entry in the list. 
         */
        if (lex->type == TYPE_ENUM_PROXY && first_user)
        {
          first_user= FALSE;
          if (acl_check_proxy_grant_access (thd, user->host.str, user->user.str,
                                        lex->grant & GRANT_ACL))
            goto error;
        } 
        else if (is_acl_user(user->host.str, user->user.str) &&
                 user->password.str &&
                 check_change_password (thd, user->host.str, user->user.str, 
                                        user->password.str, 
                                        user->password.length))
          goto error;
      }
    }
    if (first_table)
    {
      if (lex->type == TYPE_ENUM_PROCEDURE ||
          lex->type == TYPE_ENUM_FUNCTION)
      {
        uint grants= lex->all_privileges 
		   ? (PROC_ACLS & ~GRANT_ACL) | (lex->grant & GRANT_ACL)
		   : lex->grant;
        if (check_grant_routine(thd, grants | GRANT_ACL, all_tables,
                                lex->type == TYPE_ENUM_PROCEDURE, 0))
	  goto error;
        /* Conditionally writes to binlog */
        res= mysql_routine_grant(thd, all_tables,
                                 lex->type == TYPE_ENUM_PROCEDURE, 
                                 lex->users_list, grants,
                                 lex->sql_command == SQLCOM_REVOKE, TRUE);
        if (!res)
          my_ok(thd);
      }
      else
      {
	if (check_grant(thd,(lex->grant | lex->grant_tot_col | GRANT_ACL),
                        all_tables, FALSE, UINT_MAX, FALSE))
	  goto error;
        /* Conditionally writes to binlog */
        res= mysql_table_grant(thd, all_tables, lex->users_list,
			       lex->columns, lex->grant,
			       lex->sql_command == SQLCOM_REVOKE);
      }
    }
    else
    {
      if (lex->columns.elements || (lex->type && lex->type != TYPE_ENUM_PROXY))
      {
	my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
                   MYF(0));
        goto error;
      }
      else
      {
        /* Conditionally writes to binlog */
        res = mysql_grant(thd, select_lex->db, lex->users_list, lex->grant,
                          lex->sql_command == SQLCOM_REVOKE,
                          lex->type == TYPE_ENUM_PROXY);
      }
      if (!res)
      {
	if (lex->sql_command == SQLCOM_GRANT)
	{
	  List_iterator <LEX_USER> str_list(lex->users_list);
	  LEX_USER *user, *tmp_user;
	  while ((tmp_user=str_list++))
          {
            if (!(user= get_current_user(thd, tmp_user)))
              goto error;
	    reset_mqh(user, 0);
          }
	}
      }
    }
    break;
  }
#endif /*!NO_EMBEDDED_ACCESS_CHECKS*/
  case SQLCOM_RESET:
    /*
      RESET commands are never written to the binary log, so we have to
      initialize this variable because RESET shares the same code as FLUSH
    */
    lex->no_write_to_binlog= 1;
  case SQLCOM_FLUSH:
  {
    int write_to_binlog;
    if (check_global_access(thd,RELOAD_ACL))
      goto error;

    if (first_table && lex->type & REFRESH_READ_LOCK)
    {
      /* Check table-level privileges. */
      if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                             FALSE, UINT_MAX, FALSE))
        goto error;
      if (flush_tables_with_read_lock(thd, all_tables))
        goto error;
      my_ok(thd);
      break;
    }
    else if (first_table && lex->type & REFRESH_FOR_EXPORT)
    {
      /* Check table-level privileges. */
      if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                             FALSE, UINT_MAX, FALSE))
        goto error;
      if (flush_tables_for_export(thd, all_tables))
        goto error;
      my_ok(thd);
      break;
    }

    /*
      reload_acl_and_cache() will tell us if we are allowed to write to the
      binlog or not.
    */
    if (!reload_acl_and_cache(thd, lex->type, first_table, &write_to_binlog))
    {
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
          res= write_bin_log(thd, FALSE, thd->query(), thd->query_length());
      } else if (write_to_binlog < 0) 
      {
        /* 
           We should not write, but rather report error because 
           reload_acl_and_cache binlog interactions failed 
         */
        res= 1;
      } 

      if (!res)
        my_ok(thd);
    } 
    
    break;
  }
  case SQLCOM_KILL:
  {
    Item *it= (Item *)lex->value_list.head();

    if (lex->table_or_sp_used())
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Usage of subqueries or stored "
               "function calls as part of this statement");
      break;
    }

    if ((!it->fixed && it->fix_fields(lex->thd, &it)) || it->check_cols(1))
    {
      my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY),
		 MYF(0));
      goto error;
    }
    sql_kill(thd, (ulong)it->val_int(), lex->type & ONLY_KILL_QUERY);
    break;
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_SHOW_GRANTS:
  {
    LEX_USER *grant_user= get_current_user(thd, lex->grant_user);
    if (!grant_user)
      goto error;
    if ((thd->security_ctx->priv_user &&
	 !strcmp(thd->security_ctx->priv_user, grant_user->user.str)) ||
        !check_access(thd, SELECT_ACL, "mysql", NULL, NULL, 1, 0))
    {
      res = mysql_show_grants(thd, grant_user);
    }
    break;
  }
#endif
  case SQLCOM_BEGIN:
    if (trans_begin(thd, lex->start_transaction_opt))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_COMMIT:
  {
    DBUG_ASSERT(thd->lock == NULL ||
                thd->locked_tables_mode == LTM_LOCK_TABLES);
    bool tx_chain= (lex->tx_chain == TVL_YES ||
                    (thd->variables.completion_type == 1 &&
                     lex->tx_chain != TVL_NO));
    bool tx_release= (lex->tx_release == TVL_YES ||
                      (thd->variables.completion_type == 2 &&
                       lex->tx_release != TVL_NO));
    if (trans_commit(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    /* Begin transaction with the same isolation level. */
    if (tx_chain)
    {
      if (trans_begin(thd))
      goto error;
    }
    else
    {
      /* Reset the isolation level and access mode if no chaining transaction.*/
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
    /* Disconnect the current client connection. */
    if (tx_release)
      thd->killed= THD::KILL_CONNECTION;
    my_ok(thd);
    break;
  }
  case SQLCOM_ROLLBACK:
  {
    DBUG_ASSERT(thd->lock == NULL ||
                thd->locked_tables_mode == LTM_LOCK_TABLES);
    bool tx_chain= (lex->tx_chain == TVL_YES ||
                    (thd->variables.completion_type == 1 &&
                     lex->tx_chain != TVL_NO));
    bool tx_release= (lex->tx_release == TVL_YES ||
                      (thd->variables.completion_type == 2 &&
                       lex->tx_release != TVL_NO));
    if (trans_rollback(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    /* Begin transaction with the same isolation level. */
    if (tx_chain)
    {
      if (trans_begin(thd))
        goto error;
    }
    else
    {
      /* Reset the isolation level and access mode if no chaining transaction.*/
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
    /* Disconnect the current client connection. */
    if (tx_release)
      thd->killed= THD::KILL_CONNECTION;
    my_ok(thd);
    break;
  }
  case SQLCOM_RELEASE_SAVEPOINT:
    if (trans_release_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
    if (trans_rollback_to_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_SAVEPOINT:
    if (trans_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_CREATE_PROCEDURE:
  case SQLCOM_CREATE_SPFUNCTION:
  {
    uint namelen;
    char *name;

    DBUG_ASSERT(lex->sphead != 0);
    DBUG_ASSERT(lex->sphead->m_db.str); /* Must be initialized in the parser */
    /*
      Verify that the database name is allowed, optionally
      lowercase it.
    */
    if (check_and_convert_db_name(&lex->sphead->m_db, FALSE) != IDENT_NAME_OK)
      goto error;

    if (check_access(thd, CREATE_PROC_ACL, lex->sphead->m_db.str,
                     NULL, NULL, 0, 0))
      goto error;

    name= lex->sphead->name(&namelen);
#ifdef HAVE_DLOPEN
    if (lex->sphead->m_type == SP_TYPE_FUNCTION)
    {
      udf_func *udf = find_udf(name, namelen);

      if (udf)
      {
        my_error(ER_UDF_EXISTS, MYF(0), name);
        goto error;
      }
    }
#endif

    if (sp_process_definer(thd))
      goto error;

    if (! (res= sp_create_routine(thd, lex->sphead)))
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* only add privileges if really neccessary */

      Security_context security_context;
      bool restore_backup_context= false;
      Security_context *backup= NULL;
      LEX_USER *definer= thd->lex->definer;
      /*
        We're going to issue an implicit GRANT statement so we close all
        open tables. We have to keep metadata locks as this ensures that
        this statement is atomic against concurent FLUSH TABLES WITH READ
        LOCK. Deadlocks which can arise due to fact that this implicit
        statement takes metadata locks should be detected by a deadlock
        detector in MDL subsystem and reported as errors.

        No need to commit/rollback statement transaction, it's not started.

        TODO: Long-term we should either ensure that implicit GRANT statement
              is written into binary log as a separate statement or make both
              creation of routine and implicit GRANT parts of one fully atomic
              statement.
      */
      DBUG_ASSERT(thd->transaction.stmt.is_empty());
      close_thread_tables(thd);
      /*
        Check if the definer exists on slave, 
        then use definer privilege to insert routine privileges to mysql.procs_priv.

        For current user of SQL thread has GLOBAL_ACL privilege, 
        which doesn't any check routine privileges, 
        so no routine privilege record  will insert into mysql.procs_priv.
      */
      if (thd->slave_thread && is_acl_user(definer->host.str, definer->user.str))
      {
        security_context.change_security_context(thd, 
                                                 &thd->lex->definer->user,
                                                 &thd->lex->definer->host,
                                                 &thd->lex->sphead->m_db,
                                                 &backup);
        restore_backup_context= true;
      }

      if (sp_automatic_privileges && !opt_noacl &&
          check_routine_access(thd, DEFAULT_CREATE_PROC_ACLS,
                               lex->sphead->m_db.str, name,
                               lex->sql_command == SQLCOM_CREATE_PROCEDURE, 1))
      {
        if (sp_grant_privileges(thd, lex->sphead->m_db.str, name,
                                lex->sql_command == SQLCOM_CREATE_PROCEDURE))
          push_warning(thd, Sql_condition::SL_WARNING,
                       ER_PROC_AUTO_GRANT_FAIL, ER(ER_PROC_AUTO_GRANT_FAIL));
        thd->clear_error();
      }

      /*
        Restore current user with GLOBAL_ACL privilege of SQL thread
      */ 
      if (restore_backup_context)
      {
        DBUG_ASSERT(thd->slave_thread == 1);
        thd->security_ctx->restore_security_context(thd, backup);
      }
#endif
      my_ok(thd);
    }
    break; /* break super switch */
  } /* end case group bracket */
  case SQLCOM_CALL:
    {
      sp_head *sp;

      /* Here we check for the execute privilege on stored procedure. */
      if (check_routine_access(thd, EXECUTE_ACL, lex->spname->m_db.str,
                               lex->spname->m_name.str,
                               lex->sql_command == SQLCOM_CALL, 0))
        goto error;

      /*
        This will cache all SP and SF and open and lock all tables
        required for execution.
      */
      if (check_table_access(thd, SELECT_ACL, all_tables, FALSE,
                             UINT_MAX, FALSE) ||
          open_and_lock_tables(thd, all_tables, TRUE, 0))
       goto error;

      /*
        By this moment all needed SPs should be in cache so no need to look 
        into DB. 
      */
      if (!(sp= sp_find_routine(thd, SP_TYPE_PROCEDURE, lex->spname,
                                &thd->sp_proc_cache, TRUE)))
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PROCEDURE",
                 lex->spname->m_qname.str);
	goto error;
      }
      else
      {
	ha_rows select_limit;
        /* bits that should be cleared in thd->server_status */
	uint bits_to_be_cleared= 0;
        /*
          Check that the stored procedure doesn't contain Dynamic SQL
          and doesn't return result sets: such stored procedures can't
          be called from a function or trigger.
        */
        if (thd->in_sub_stmt)
        {
          const char *where= (thd->in_sub_stmt & SUB_STMT_TRIGGER ?
                              "trigger" : "function");
          if (sp->is_not_allowed_in_function(where))
            goto error;
        }

	if (sp->m_flags & sp_head::MULTI_RESULTS)
	{
	  if (! (thd->client_capabilities & CLIENT_MULTI_RESULTS))
	  {
            /*
              The client does not support multiple result sets being sent
              back
            */
	    my_error(ER_SP_BADSELECT, MYF(0), sp->m_qname.str);
	    goto error;
	  }
          /*
            If SERVER_MORE_RESULTS_EXISTS is not set,
            then remember that it should be cleared
          */
	  bits_to_be_cleared= (~thd->server_status &
                               SERVER_MORE_RESULTS_EXISTS);
	  thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
	}

	if (check_routine_access(thd, EXECUTE_ACL,
				 sp->m_db.str, sp->m_name.str, TRUE, FALSE))
	{
	  goto error;
	}
	select_limit= thd->variables.select_limit;
	thd->variables.select_limit= HA_POS_ERROR;

        /* 
          We never write CALL statements into binlog:
           - If the mode is non-prelocked, each statement will be logged
             separately.
           - If the mode is prelocked, the invoking statement will care
             about writing into binlog.
          So just execute the statement.
        */
	res= sp->execute_procedure(thd, &lex->value_list);

	thd->variables.select_limit= select_limit;

        thd->server_status&= ~bits_to_be_cleared;

	if (!res)
        {
          my_ok(thd, (thd->get_row_count_func() < 0) ? 0 : thd->get_row_count_func());
        }
	else
        {
          DBUG_ASSERT(thd->is_error() || thd->killed);
	  goto error;		// Substatement should already have sent error
        }
      }
      break;
    }
  case SQLCOM_ALTER_PROCEDURE:
  case SQLCOM_ALTER_FUNCTION:
    {
      if (check_routine_access(thd, ALTER_PROC_ACL, lex->spname->m_db.str,
                               lex->spname->m_name.str,
                               lex->sql_command == SQLCOM_ALTER_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type= (lex->sql_command == SQLCOM_ALTER_PROCEDURE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;
      /*
        Note that if you implement the capability of ALTER FUNCTION to
        alter the body of the function, this command should be made to
        follow the restrictions that log-bin-trust-function-creators=0
        already puts on CREATE FUNCTION.
      */
      /* Conditionally writes to binlog */
      int sp_result= sp_update_routine(thd, sp_type, lex->spname,
                                       &lex->sp_chistics);
      if (thd->killed)
        goto error;
      switch (sp_result)
      {
      case SP_OK:
	my_ok(thd);
	break;
      case SP_KEY_NOT_FOUND:
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      default:
	my_error(ER_SP_CANT_ALTER, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      }
      break;
    }
  case SQLCOM_DROP_PROCEDURE:
  case SQLCOM_DROP_FUNCTION:
    {
#ifdef HAVE_DLOPEN
      if (lex->sql_command == SQLCOM_DROP_FUNCTION &&
          ! lex->spname->m_explicit_name)
      {
        /* DROP FUNCTION <non qualified name> */
        udf_func *udf = find_udf(lex->spname->m_name.str,
                                 lex->spname->m_name.length);
        if (udf)
        {
          if (check_access(thd, DELETE_ACL, "mysql", NULL, NULL, 1, 0))
            goto error;

          if (!(res = mysql_drop_function(thd, &lex->spname->m_name)))
          {
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DROP_FAILED, MYF(0),
                   "FUNCTION (UDF)", lex->spname->m_name.str);
          goto error;
        }

        if (lex->spname->m_db.str == NULL)
        {
          if (lex->drop_if_exists)
          {
            push_warning_printf(thd, Sql_condition::SL_NOTE,
                                ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                                "FUNCTION (UDF)", lex->spname->m_name.str);
            res= FALSE;
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                   "FUNCTION (UDF)", lex->spname->m_name.str);
          goto error;
        }
        /* Fall thought to test for a stored function */
      }
#endif

      char *db= lex->spname->m_db.str;
      char *name= lex->spname->m_name.str;

      if (check_routine_access(thd, ALTER_PROC_ACL, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type= (lex->sql_command == SQLCOM_DROP_PROCEDURE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;

      /* Conditionally writes to binlog */
      int sp_result= sp_drop_routine(thd, sp_type, lex->spname);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /*
        We're going to issue an implicit REVOKE statement so we close all
        open tables. We have to keep metadata locks as this ensures that
        this statement is atomic against concurent FLUSH TABLES WITH READ
        LOCK. Deadlocks which can arise due to fact that this implicit
        statement takes metadata locks should be detected by a deadlock
        detector in MDL subsystem and reported as errors.

        No need to commit/rollback statement transaction, it's not started.

        TODO: Long-term we should either ensure that implicit REVOKE statement
              is written into binary log as a separate statement or make both
              dropping of routine and implicit REVOKE parts of one fully atomic
              statement.
      */
      DBUG_ASSERT(thd->transaction.stmt.is_empty());
      close_thread_tables(thd);

      if (sp_result != SP_KEY_NOT_FOUND &&
          sp_automatic_privileges && !opt_noacl &&
          sp_revoke_privileges(thd, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE))
      {
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_PROC_AUTO_REVOKE_FAIL,
                     ER(ER_PROC_AUTO_REVOKE_FAIL));
        /* If this happens, an error should have been reported. */
        goto error;
      }
#endif

      res= sp_result;
      switch (sp_result) {
      case SP_OK:
	my_ok(thd);
	break;
      case SP_KEY_NOT_FOUND:
	if (lex->drop_if_exists)
	{
          res= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
	  push_warning_printf(thd, Sql_condition::SL_NOTE,
			      ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                              SP_COM_STRING(lex), lex->spname->m_qname.str);
          if (!res)
            my_ok(thd);
	  break;
	}
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      default:
	my_error(ER_SP_DROP_FAILED, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      }
      break;
    }
  case SQLCOM_SHOW_CREATE_PROC:
    {
      if (sp_show_create_routine(thd, SP_TYPE_PROCEDURE, lex->spname))
        goto error;
      break;
    }
  case SQLCOM_SHOW_CREATE_FUNC:
    {
      if (sp_show_create_routine(thd, SP_TYPE_FUNCTION, lex->spname))
	goto error;
      break;
    }
  case SQLCOM_SHOW_PROC_CODE:
  case SQLCOM_SHOW_FUNC_CODE:
    {
#ifndef DBUG_OFF
      sp_head *sp;
      enum_sp_type sp_type= (lex->sql_command == SQLCOM_SHOW_PROC_CODE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;

      if (sp_cache_routine(thd, sp_type, lex->spname, false, &sp))
        goto error;
      if (!sp || sp->show_routine_code(thd))
      {
        /* We don't distinguish between errors for now */
        my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_name.str);
        goto error;
      }
      break;
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               "SHOW PROCEDURE|FUNCTION CODE", "--with-debug");
      goto error;
#endif // ifndef DBUG_OFF
    }
  case SQLCOM_SHOW_CREATE_TRIGGER:
    {
      if (lex->spname->m_name.length > NAME_LEN)
      {
        my_error(ER_TOO_LONG_IDENT, MYF(0), lex->spname->m_name.str);
        goto error;
      }

      if (show_create_trigger(thd, lex->spname))
        goto error; /* Error has been already logged. */

      break;
    }
  case SQLCOM_CREATE_VIEW:
    {
      /*
        Note: SQLCOM_CREATE_VIEW also handles 'ALTER VIEW' commands
        as specified through the thd->lex->create_view_mode flag.
      */
      res= mysql_create_view(thd, first_table, thd->lex->create_view_mode);
      break;
    }
  case SQLCOM_DROP_VIEW:
    {
      if (check_table_access(thd, DROP_ACL, all_tables, FALSE, UINT_MAX, FALSE))
        goto error;
      /* Conditionally writes to binlog. */
      res= mysql_drop_view(thd, first_table, thd->lex->drop_mode);
      break;
    }
  case SQLCOM_CREATE_TRIGGER:
  {
    /* Conditionally writes to binlog. */
    res= mysql_create_or_drop_trigger(thd, all_tables, 1);

    break;
  }
  case SQLCOM_DROP_TRIGGER:
  {
    /* Conditionally writes to binlog. */
    res= mysql_create_or_drop_trigger(thd, all_tables, 0);
    break;
  }
  case SQLCOM_XA_START:
    if (trans_xa_start(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_END:
    if (trans_xa_end(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_PREPARE:
    if (trans_xa_prepare(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_COMMIT:
    if (trans_xa_commit(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    /*
      We've just done a commit, reset transaction
      isolation level and access mode to the session default.
    */
    thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
    thd->tx_read_only= thd->variables.tx_read_only;
    my_ok(thd);
    break;
  case SQLCOM_XA_ROLLBACK:
    if (trans_xa_rollback(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    /*
      We've just done a rollback, reset transaction
      isolation level and access mode to the session default.
    */
    thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
    thd->tx_read_only= thd->variables.tx_read_only;
    my_ok(thd);
    break;
  case SQLCOM_XA_RECOVER:
    res= mysql_xa_recover(thd);
    break;
  case SQLCOM_ALTER_TABLESPACE:
    if (check_global_access(thd, CREATE_TABLESPACE_ACL))
      break;
    if (!(res= mysql_alter_tablespace(thd, lex->alter_tablespace_info)))
      my_ok(thd);
    break;
  case SQLCOM_INSTALL_PLUGIN:
    if (! (res= mysql_install_plugin(thd, &thd->lex->comment,
                                     &thd->lex->ident)))
      my_ok(thd);
    break;
  case SQLCOM_UNINSTALL_PLUGIN:
    if (! (res= mysql_uninstall_plugin(thd, &thd->lex->comment)))
      my_ok(thd);
    break;
  case SQLCOM_BINLOG_BASE64_EVENT:
  {
#ifndef EMBEDDED_LIBRARY
    mysql_client_binlog_statement(thd);
#else /* EMBEDDED_LIBRARY */
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "embedded");
#endif /* EMBEDDED_LIBRARY */
    break;
  }
  case SQLCOM_CREATE_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    if (create_server(thd, &thd->lex->server_options))
      goto error;

    my_ok(thd, 1);
    break;
  }
  case SQLCOM_ALTER_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    if (alter_server(thd, &thd->lex->server_options))
      goto error;

    my_ok(thd, 1);
    break;
  }
  case SQLCOM_DROP_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    LEX *lex= thd->lex;
    if (drop_server(thd, &lex->server_options, lex->drop_if_exists))
    {
      /*
        drop_server() can fail without reporting an error
        due to IF EXISTS clause. In this case, call my_ok().
      */
      if (thd->is_error() || thd->killed)
        goto error;
      DBUG_ASSERT(lex->drop_if_exists);
      my_ok(thd, 0);
      break;
    }

    my_ok(thd, 1);
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
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    /* fall through */
  case SQLCOM_SIGNAL:
  case SQLCOM_RESIGNAL:
  case SQLCOM_GET_DIAGNOSTICS:
    DBUG_ASSERT(lex->m_sql_cmd != NULL);
    res= lex->m_sql_cmd->execute(thd);
    break;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_ALTER_USER:
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd, CREATE_USER_ACL))
      break;
    /* Conditionally writes to binlog */
    if (!(res= mysql_user_password_expire(thd, lex->users_list)))
      my_ok(thd);
    break;
#endif
  default:
#ifndef EMBEDDED_LIBRARY
    DBUG_ASSERT(0);                             /* Impossible */
#endif
    my_ok(thd);
    break;
  }
  THD_STAGE_INFO(thd, stage_query_end);

  /*
    Binlog-related cleanup:
    Reset system variables temporarily modified by SET ONE SHOT.

    Exception: If this is a SET, do nothing. This is to allow
    mysqlbinlog to print many SET commands (in this case we want the
    charset temp setting to live until the real query). This is also
    needed so that SET CHARACTER_SET_CLIENT... does not cancel itself
    immediately.
  */
  if (thd->one_shot_set && lex->sql_command != SQLCOM_SET_OPTION)
    reset_one_shot_variables(thd);

  goto finish;

error:
  res= TRUE;

finish:

  DBUG_ASSERT(!thd->in_active_multi_stmt_transaction() ||
               thd->in_multi_stmt_transaction_mode());

  if (! thd->in_sub_stmt)
  {
    /* report error issued during command execution */
    if (thd->killed_errno())
      thd->send_kill_message();
    if (thd->killed == THD::KILL_QUERY || thd->killed == THD::KILL_BAD_DATA)
    {
      thd->killed= THD::NOT_KILLED;
      thd->mysys_var->abort= 0;
    }
    if (thd->is_error() || (thd->variables.option_bits & OPTION_MASTER_SQL_ERROR))
      trans_rollback_stmt(thd);
    else
    {
      /* If commit fails, we should be able to reset the OK status. */
      thd->get_stmt_da()->set_overwrite_status(true);
      trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }
  }

  lex->unit.cleanup();
  /* Free tables */
  THD_STAGE_INFO(thd, stage_closing_tables);
  close_thread_tables(thd);

#ifndef DBUG_OFF
  if (lex->sql_command != SQLCOM_SET_OPTION && ! thd->in_sub_stmt)
    DEBUG_SYNC(thd, "execute_command_after_close_tables");
#endif

  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END))
  {
    /* No transaction control allowed in sub-statements. */
    DBUG_ASSERT(! thd->in_sub_stmt);
    /* If commit fails, we should be able to reset the OK status. */
    thd->get_stmt_da()->set_overwrite_status(true);
    /* Commit the normal transaction if one is active. */
    trans_commit_implicit(thd);
    thd->get_stmt_da()->set_overwrite_status(false);
    thd->mdl_context.release_transactional_locks();
  }
  else if (! thd->in_sub_stmt && ! thd->in_multi_stmt_transaction_mode())
  {
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
  }
  else if (! thd->in_sub_stmt)
  {
    thd->mdl_context.release_statement_locks();
  }

  DBUG_RETURN(res || thd->is_error());
}

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables)
{
  LEX	*lex= thd->lex;
  select_result *result= lex->result;
  bool res;
  /* assign global limit variable if limit is not given */
  {
    SELECT_LEX *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((ulonglong) thd->variables.select_limit);
  }
  if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
  {
    if (lex->describe)
    {
      /*
        We always use select_send for EXPLAIN, even if it's an EXPLAIN
        for SELECT ... INTO OUTFILE: a user application should be able
        to prepend EXPLAIN to any query and receive output for it,
        even if the query itself redirects the output.
      */
      if (!(result= new select_send()))
        return 1;                               /* purecov: inspected */
      res= explain_query_expression(thd, result);
      delete result;
    }
    else
    {
      if (!result && !(result= new select_send()))
        return 1;                               /* purecov: inspected */
      select_result *save_result= result;
      select_result *analyse_result= NULL;
      if (lex->proc_analyse)
      {
        if ((result= analyse_result=
               new select_analyse(result, lex->proc_analyse)) == NULL)
          return true;
      }
      res= handle_select(thd, result, 0);
      delete analyse_result;
      if (save_result != lex->result)
        delete save_result;
    }
  }
  return res;
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
/**
  Check grants for commands which work only with one table.

  @param thd                    Thread handler
  @param privilege              requested privilege
  @param all_tables             global table list of query
  @param no_errors              FALSE/TRUE - report/don't report error to
                            the client (using my_error() call).

  @retval
    0   OK
  @retval
    1   access denied, error is sent to client
*/

bool check_single_table_access(THD *thd, ulong privilege, 
                               TABLE_LIST *all_tables, bool no_errors)
{
  Security_context * backup_ctx= thd->security_ctx;

  /* we need to switch to the saved context (if any) */
  if (all_tables->security_ctx)
    thd->security_ctx= all_tables->security_ctx;

  const char *db_name;
  if ((all_tables->view || all_tables->field_translation) &&
      !all_tables->schema_table)
    db_name= all_tables->view_db.str;
  else
    db_name= all_tables->db;

  if (check_access(thd, privilege, db_name,
                   &all_tables->grant.privilege,
                   &all_tables->grant.m_internal,
                   0, no_errors))
    goto deny;

  /* Show only 1 table for check_grant */
  if (!(all_tables->belong_to_view &&
        (thd->lex->sql_command == SQLCOM_SHOW_FIELDS)) &&
      check_grant(thd, privilege, all_tables, FALSE, 1, no_errors))
    goto deny;

  thd->security_ctx= backup_ctx;
  return 0;

deny:
  thd->security_ctx= backup_ctx;
  return 1;
}

/**
  Check grants for commands which work only with one table and all other
  tables belonging to subselects or implicitly opened tables.

  @param thd			Thread handler
  @param privilege		requested privilege
  @param all_tables		global table list of query

  @retval
    0   OK
  @retval
    1   access denied, error is sent to client
*/

bool check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *all_tables)
{
  if (check_single_table_access (thd,privilege,all_tables, FALSE))
    return 1;

  /* Check rights on tables of subselects and implicitly opened tables */
  TABLE_LIST *subselects_tables, *view= all_tables->view ? all_tables : 0;
  if ((subselects_tables= all_tables->next_global))
  {
    /*
      Access rights asked for the first table of a view should be the same
      as for the view
    */
    if (view && subselects_tables->belong_to_view == view)
    {
      if (check_single_table_access (thd, privilege, subselects_tables, FALSE))
        return 1;
      subselects_tables= subselects_tables->next_global;
    }
    if (subselects_tables &&
        (check_table_access(thd, SELECT_ACL, subselects_tables, FALSE,
                            UINT_MAX, FALSE)))
      return 1;
  }
  return 0;
}


/**
  @brief Compare requested privileges with the privileges acquired from the
    User- and Db-tables.
  @param thd          Thread handler
  @param want_access  The requested access privileges.
  @param db           A pointer to the Db name.
  @param[out] save_priv A pointer to the granted privileges will be stored.
  @param grant_internal_info A pointer to the internal grant cache.
  @param dont_check_global_grants True if no global grants are checked.
  @param no_error     True if no errors should be sent to the client.

  'save_priv' is used to save the User-table (global) and Db-table grants for
  the supplied db name. Note that we don't store db level grants if the global
  grants is enough to satisfy the request AND the global grants contains a
  SELECT grant.

  For internal databases (INFORMATION_SCHEMA, PERFORMANCE_SCHEMA),
  additional rules apply, see ACL_internal_schema_access.

  @see check_grant

  @return Status of denial of access by exclusive ACLs.
    @retval FALSE Access can't exclusively be denied by Db- and User-table
      access unless Column- and Table-grants are checked too.
    @retval TRUE Access denied.
*/

bool
check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
             GRANT_INTERNAL_INFO *grant_internal_info,
             bool dont_check_global_grants, bool no_errors)
{
  Security_context *sctx= thd->security_ctx;
  ulong db_access;

  /*
    GRANT command:
    In case of database level grant the database name may be a pattern,
    in case of table|column level grant the database name can not be a pattern.
    We use 'dont_check_global_grants' as a flag to determine
    if it's database level grant command
    (see SQLCOM_GRANT case, mysql_execute_command() function) and
    set db_is_pattern according to 'dont_check_global_grants' value.
  */
  bool  db_is_pattern= ((want_access & GRANT_ACL) && dont_check_global_grants);
  ulong dummy;
  DBUG_ENTER("check_access");
  DBUG_PRINT("enter",("db: %s  want_access: %lu  master_access: %lu",
                      db ? db : "", want_access, sctx->master_access));

  if (save_priv)
    *save_priv=0;
  else
  {
    save_priv= &dummy;
    dummy= 0;
  }

  THD_STAGE_INFO(thd, stage_checking_permissions);
  if ((!db || !db[0]) && !thd->db && !dont_check_global_grants)
  {
    DBUG_PRINT("error",("No database"));
    if (!no_errors)
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR),
                 MYF(0));                       /* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if ((db != NULL) && (db != any_db))
  {
    const ACL_internal_schema_access *access;
    access= get_cached_schema_access(grant_internal_info, db);
    if (access)
    {
      switch (access->check(want_access, save_priv))
      {
      case ACL_INTERNAL_ACCESS_GRANTED:
        /*
          All the privileges requested have been granted internally.
          [out] *save_privileges= Internal privileges.
        */
        DBUG_RETURN(FALSE);
      case ACL_INTERNAL_ACCESS_DENIED:
        if (! no_errors)
        {
          my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
                   sctx->priv_user, sctx->priv_host, db);
        }
        DBUG_RETURN(TRUE);
      case ACL_INTERNAL_ACCESS_CHECK_GRANT:
        /*
          Only some of the privilege requested have been granted internally,
          proceed with the remaining bits of the request (want_access).
        */
        want_access&= ~(*save_priv);
        break;
      }
    }
  }

  if ((sctx->master_access & want_access) == want_access)
  {
    /*
      1. If we don't have a global SELECT privilege, we have to get the
      database specific access rights to be able to handle queries of type
      UPDATE t1 SET a=1 WHERE b > 0
      2. Change db access if it isn't current db which is being addressed
    */
    if (!(sctx->master_access & SELECT_ACL))
    {
      if (db && (!thd->db || db_is_pattern || strcmp(db, thd->db)))
        db_access= acl_get(sctx->host, sctx->ip, sctx->priv_user, db,
                           db_is_pattern);
      else
      {
        /* get access for current db */
        db_access= sctx->db_access;
      }
      /*
        The effective privileges are the union of the global privileges
        and the intersection of db- and host-privileges,
        plus the internal privileges.
      */
      *save_priv|= sctx->master_access | db_access;
    }
    else
      *save_priv|= sctx->master_access;
    DBUG_RETURN(FALSE);
  }
  if (((want_access & ~sctx->master_access) & ~DB_ACLS) ||
      (! db && dont_check_global_grants))
  {						// We can never grant this
    DBUG_PRINT("error",("No possible access"));
    if (!no_errors)
    {
      if (thd->password == 2)
        my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
                 sctx->priv_user,
                 sctx->priv_host);
      else
        my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
                 sctx->priv_user,
                 sctx->priv_host,
                 (thd->password ?
                  ER(ER_YES) :
                  ER(ER_NO)));                    /* purecov: tested */
    }
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (db == any_db)
  {
    /*
      Access granted; Allow select on *any* db.
      [out] *save_privileges= 0
    */
    DBUG_RETURN(FALSE);
  }

  if (db && (!thd->db || db_is_pattern || strcmp(db,thd->db)))
    db_access= acl_get(sctx->host, sctx->ip, sctx->priv_user, db,
                       db_is_pattern);
  else
    db_access= sctx->db_access;
  DBUG_PRINT("info",("db_access: %lu  want_access: %lu",
                     db_access, want_access));

  /*
    Save the union of User-table and the intersection between Db-table and
    Host-table privileges, with the already saved internal privileges.
  */
  db_access= (db_access | sctx->master_access);
  *save_priv|= db_access;

  /*
    We need to investigate column- and table access if all requested privileges
    belongs to the bit set of .
  */
  bool need_table_or_column_check=
    (want_access & (TABLE_ACLS | PROC_ACLS | db_access)) == want_access;

  /*
    Grant access if the requested access is in the intersection of
    host- and db-privileges (as retrieved from the acl cache),
    also grant access if all the requested privileges are in the union of
    TABLES_ACLS and PROC_ACLS; see check_grant.
  */
  if ( (db_access & want_access) == want_access ||
      (!dont_check_global_grants &&
       need_table_or_column_check))
  {
    /*
       Ok; but need to check table- and column privileges.
       [out] *save_privileges is (User-priv | (Db-priv & Host-priv) | Internal-priv)
    */
    DBUG_RETURN(FALSE);
  }

  /*
    Access is denied;
    [out] *save_privileges is (User-priv | (Db-priv & Host-priv) | Internal-priv)
  */
  DBUG_PRINT("error",("Access denied"));
  if (!no_errors)
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user, sctx->priv_host,
             (db ? db : (thd->db ?
                         thd->db :
                         "unknown")));
  DBUG_RETURN(TRUE);

}


/**
  Check if user has enough privileges for execution of SHOW statement,
  which was converted to query to one of I_S tables.

  @param thd    Thread context.
  @param table  Table list element for I_S table to be queried..

  @retval FALSE - Success.
  @retval TRUE  - Failure.
*/

static bool check_show_access(THD *thd, TABLE_LIST *table)
{
  switch (get_schema_table_idx(table->schema_table)) {
  case SCH_SCHEMATA:
    return (specialflag & SPECIAL_SKIP_SHOW_DB) &&
      check_global_access(thd, SHOW_DB_ACL);

  case SCH_TABLE_NAMES:
  case SCH_TABLES:
  case SCH_VIEWS:
  case SCH_TRIGGERS:
  case SCH_EVENTS:
  {
    const char *dst_db_name= table->schema_select_lex->db;

    DBUG_ASSERT(dst_db_name);

    if (check_access(thd, SELECT_ACL, dst_db_name,
                     &thd->col_access, NULL, FALSE, FALSE))
      return TRUE;

    if (!thd->col_access && check_grant_db(thd, dst_db_name))
    {
      my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
               thd->security_ctx->priv_user,
               thd->security_ctx->priv_host,
               dst_db_name);
      return TRUE;
    }

    return FALSE;
  }

  case SCH_COLUMNS:
  case SCH_STATISTICS:
  {
    TABLE_LIST *dst_table;
    dst_table= table->schema_select_lex->table_list.first;

    DBUG_ASSERT(dst_table);

    /*
      Open temporary tables to be able to detect them during privilege check.
    */
    if (open_temporary_tables(thd, dst_table))
      return TRUE;

    if (check_access(thd, SELECT_ACL, dst_table->db,
                     &dst_table->grant.privilege,
                     &dst_table->grant.m_internal,
                     FALSE, FALSE))
          return TRUE; /* Access denied */

    /*
      Check_grant will grant access if there is any column privileges on
      all of the tables thanks to the fourth parameter (bool show_table).
    */
    if (check_grant(thd, SELECT_ACL, dst_table, TRUE, UINT_MAX, FALSE))
      return TRUE; /* Access denied */

    close_thread_tables(thd);
    dst_table->table= NULL;

    /* Access granted */
    return FALSE;
  }
  default:
    break;
  }

  return FALSE;
}



/**
  @brief Check if the requested privileges exists in either User-, Host- or
    Db-tables.
  @param thd          Thread context
  @param want_access  Privileges requested
  @param tables       List of tables to be compared against
  @param no_errors    Don't report error to the client (using my_error() call).
  @param any_combination_of_privileges_will_do TRUE if any privileges on any
    column combination is enough.
  @param number       Only the first 'number' tables in the linked list are
                      relevant.

  The suppled table list contains cached privileges. This functions calls the
  help functions check_access and check_grant to verify the first three steps
  in the privileges check queue:
  1. Global privileges
  2. OR (db privileges AND host privileges)
  3. OR table privileges
  4. OR column privileges (not checked by this function!)
  5. OR routine privileges (not checked by this function!)

  @see check_access
  @see check_grant

  @note This functions assumes that table list used and
  thd->lex->query_tables_own_last value correspond to each other
  (the latter should be either 0 or point to next_global member
  of one of elements of this table list).

  @return
    @retval FALSE OK
    @retval TRUE  Access denied; But column or routine privileges might need to
      be checked also.
*/

bool
check_table_access(THD *thd, ulong requirements,TABLE_LIST *tables,
		   bool any_combination_of_privileges_will_do,
                   uint number, bool no_errors)
{
  TABLE_LIST *org_tables= tables;
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();
  uint i= 0;
  Security_context *sctx= thd->security_ctx, *backup_ctx= thd->security_ctx;
  /*
    The check that first_not_own_table is not reached is for the case when
    the given table list refers to the list for prelocking (contains tables
    of other queries). For simple queries first_not_own_table is 0.
  */
  for (; i < number && tables != first_not_own_table && tables;
       tables= tables->next_global, i++)
  {
    ulong want_access= requirements;
    if (tables->security_ctx)
      sctx= tables->security_ctx;
    else
      sctx= backup_ctx;

    /*
       Register access for view underlying table.
       Remove SHOW_VIEW_ACL, because it will be checked during making view
     */
    tables->grant.orig_want_privilege= (want_access & ~SHOW_VIEW_ACL);

    /*
      We should not encounter table list elements for reformed SHOW
      statements unless this is first table list element in the main
      select.
      Such table list elements require additional privilege check
      (see check_show_access()). This check is carried out by caller,
      but only for the first table list element from the main select.
    */
    DBUG_ASSERT(!tables->schema_table_reformed ||
                tables == thd->lex->select_lex.table_list.first);

    DBUG_PRINT("info", ("derived: %d  view: %d", tables->derived != 0,
                        tables->view != 0));

    if (tables->is_anonymous_derived_table())
      continue;

    thd->security_ctx= sctx;

    if (check_access(thd, want_access, tables->get_db_name(),
                     &tables->grant.privilege,
                     &tables->grant.m_internal,
                     0, no_errors))
      goto deny;
  }
  thd->security_ctx= backup_ctx;
  return check_grant(thd,requirements,org_tables,
                     any_combination_of_privileges_will_do,
                     number, no_errors);
deny:
  thd->security_ctx= backup_ctx;
  return TRUE;
}


bool
check_routine_access(THD *thd, ulong want_access,char *db, char *name,
		     bool is_proc, bool no_errors)
{
  TABLE_LIST tables[1];
  
  memset(tables, 0, sizeof(TABLE_LIST));
  tables->db= db;
  tables->table_name= tables->alias= name;
  
  /*
    The following test is just a shortcut for check_access() (to avoid
    calculating db_access) under the assumption that it's common to
    give persons global right to execute all stored SP (but not
    necessary to create them).
    Note that this effectively bypasses the ACL_internal_schema_access checks
    that are implemented for the INFORMATION_SCHEMA and PERFORMANCE_SCHEMA,
    which are located in check_access().
    Since the I_S and P_S do not contain routines, this bypass is ok,
    as long as this code path is not abused to create routines.
    The assert enforce that.
  */
  DBUG_ASSERT((want_access & CREATE_PROC_ACL) == 0);
  if ((thd->security_ctx->master_access & want_access) == want_access)
    tables->grant.privilege= want_access;
  else if (check_access(thd, want_access, db,
                        &tables->grant.privilege,
                        &tables->grant.m_internal,
                        0, no_errors))
    return TRUE;
  
  return check_grant_routine(thd, want_access, tables, is_proc, no_errors);
}


/**
  Check if the routine has any of the routine privileges.

  @param thd	       Thread handler
  @param db           Database name
  @param name         Routine name

  @retval
    0            ok
  @retval
    1            error
*/

bool check_some_routine_access(THD *thd, const char *db, const char *name,
                               bool is_proc)
{
  ulong save_priv;
  /*
    The following test is just a shortcut for check_access() (to avoid
    calculating db_access)
    Note that this effectively bypasses the ACL_internal_schema_access checks
    that are implemented for the INFORMATION_SCHEMA and PERFORMANCE_SCHEMA,
    which are located in check_access().
    Since the I_S and P_S do not contain routines, this bypass is ok,
    as it only opens SHOW_PROC_ACLS.
  */
  if (thd->security_ctx->master_access & SHOW_PROC_ACLS)
    return FALSE;
  if (!check_access(thd, SHOW_PROC_ACLS, db, &save_priv, NULL, 0, 1) ||
      (save_priv & SHOW_PROC_ACLS))
    return FALSE;
  return check_routine_level_acl(thd, db, name, is_proc);
}


/**
  Check if the given table has any of the asked privileges

  @param thd		 Thread handler
  @param want_access	 Bitmap of possible privileges to check for

  @retval
    0  ok
  @retval
    1  error
*/

bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table)
{
  ulong access;
  DBUG_ENTER("check_some_access");

  /* This loop will work as long as we have less than 32 privileges */
  for (access= 1; access < want_access ; access<<= 1)
  {
    if (access & want_access)
    {
      if (!check_access(thd, access, table->db,
                        &table->grant.privilege,
                        &table->grant.m_internal,
                        0, 1) &&
           !check_grant(thd, access, table, FALSE, 1, TRUE))
        DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("exit",("no matching access rights"));
  DBUG_RETURN(1);
}

#else

static bool check_show_access(THD *thd, TABLE_LIST *table)
{
  return false;
}

#endif /*NO_EMBEDDED_ACCESS_CHECKS*/


/**
  check for global access and give descriptive error message if it fails.

  @param thd			Thread handler
  @param want_access		Use should have any of these global rights

  @warning
    One gets access right if one has ANY of the rights in want_access.
    This is useful as one in most cases only need one global right,
    but in some case we want to check if the user has SUPER or
    REPL_CLIENT_ACL rights.

  @retval
    0	ok
  @retval
    1	Access denied.  In this case an error is sent to the client
*/

bool check_global_access(THD *thd, ulong want_access)
{
  DBUG_ENTER("check_global_access");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  char command[128];
  if ((thd->security_ctx->master_access & want_access))
    DBUG_RETURN(0);
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), command);
  DBUG_RETURN(1);
#else
  DBUG_RETURN(0);
#endif
}

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/


#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

#ifndef DBUG_OFF
long max_stack_used;
#endif

/**
  @note
  Note: The 'buf' parameter is necessary, even if it is unused here.
  - fix_fields functions has a "dummy" buffer large enough for the
    corresponding exec. (Thus we only have to check in fix_fields.)
  - Passing to check_stack_overrun() prevents the compiler from removing it.
*/
bool check_stack_overrun(THD *thd, long margin,
			 uchar *buf __attribute__((unused)))
{
  long stack_used;
  DBUG_ASSERT(thd == current_thd);
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) (my_thread_stack_size - margin))
  {
    /*
      Do not use stack for the message buffer to ensure correct
      behaviour in cases we have close to no stack left.
    */
    char* ebuff= new (std::nothrow) char[MYSQL_ERRMSG_SIZE];
    if (ebuff) {
      my_snprintf(ebuff, MYSQL_ERRMSG_SIZE, ER(ER_STACK_OVERRUN_NEED_MORE),
                  stack_used, my_thread_stack_size, margin);
      my_message(ER_STACK_OVERRUN_NEED_MORE, ebuff, MYF(ME_FATALERROR));
      delete [] ebuff;
    }
    return 1;
  }
#ifndef DBUG_OFF
  max_stack_used= max(max_stack_used, stack_used);
#endif
  return 0;
}


#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, ulong *yystacksize)
{
  Yacc_state *state= & current_thd->m_parser_state->m_yacc;
  ulong old_info=0;
  DBUG_ASSERT(state);
  if ((uint) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!state->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  if (!(state->yacc_yyvs= (uchar*)
        my_realloc(state->yacc_yyvs,
                   *yystacksize*sizeof(**yyvs),
                   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(state->yacc_yyss= (uchar*)
        my_realloc(state->yacc_yyss,
                   *yystacksize*sizeof(**yyss),
                   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return 1;
  if (old_info)
  {
    /*
      Only copy the old stack on the first call to my_yyoverflow(),
      when replacing a static stack (YYINITDEPTH) by a dynamic stack.
      For subsequent calls, my_realloc already did preserve the old stack.
    */
    memcpy(state->yacc_yyss, *yyss, old_info*sizeof(**yyss));
    memcpy(state->yacc_yyvs, *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss= (short*) state->yacc_yyss;
  *yyvs= (YYSTYPE*) state->yacc_yyvs;
  return 0;
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
void mysql_reset_thd_for_next_command(THD *thd)
{
  thd->reset_for_next_command();
}

void THD::reset_for_next_command()
{
  // TODO: Why on earth is this here?! We should probably fix this
  // function and move it to the proper file. /Matz
  THD *thd= this;
  DBUG_ENTER("mysql_reset_thd_for_next_command");
  DBUG_ASSERT(!thd->sp_runtime_ctx); /* not for substatements of routines */
  DBUG_ASSERT(! thd->in_sub_stmt);
  thd->free_list= 0;
  thd->select_number= 1;
  /*
    Those two lines below are theoretically unneeded as
    THD::cleanup_after_query() should take care of this already.
  */
  thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;

  thd->query_start_used= thd->query_start_usec_used= 0;
  thd->is_fatal_error= thd->time_zone_used= 0;
  /*
    Clear the status flag that are expected to be cleared at the
    beginning of each SQL statement.
  */
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
  /*
    If in autocommit mode and not in a transaction, reset flag
    that identifies if a transaction has done some operations
    that cannot be safely rolled back.

    If the flag is set an warning message is printed out in
    ha_rollback_trans() saying that some tables couldn't be
    rolled back.
  */
  if (!thd->in_multi_stmt_transaction_mode())
  {
    thd->transaction.all.reset_unsafe_rollback_flags();
  }
  DBUG_ASSERT(thd->security_ctx== &thd->main_security_ctx);
  thd->thread_specific_used= FALSE;

  if (opt_bin_log)
  {
    reset_dynamic(&thd->user_var_events);
    thd->user_var_events_alloc= thd->mem_root;
  }
  thd->clear_error();
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->reset_statement_cond_count();
  thd->rand_used= 0;
  thd->m_sent_row_count= thd->m_examined_row_count= 0;

  thd->reset_current_stmt_binlog_format_row();
  thd->binlog_unsafe_warning_flags= 0;

  thd->m_trans_end_pos= 0;
  thd->m_trans_log_file= NULL;
  thd->commit_error= 0;
  thd->durability_property= HA_REGULAR_DURABILITY;
  thd->set_trans_pos(NULL, 0);

  DBUG_PRINT("debug",
             ("is_current_stmt_binlog_format_row(): %d",
              thd->is_current_stmt_binlog_format_row()));

  DBUG_VOID_RETURN;
}


/**
  Resets the lex->current_select object.
  @note It is assumed that lex->current_select != NULL

  This function is a wrapper around select_lex->init_select() with an added
  check for the special situation when using INTO OUTFILE and LOAD DATA.
*/

void
mysql_init_select(LEX *lex)
{
  SELECT_LEX *select_lex= lex->current_select;
  select_lex->init_select();
  lex->wild= 0;
  if (select_lex == &lex->select_lex)
  {
    DBUG_ASSERT(lex->result == 0);
    lex->exchange= 0;
  }
}


/**
  Used to allocate a new SELECT_LEX object on the current thd mem_root and
  link it into the relevant lists.

  This function is always followed by mysql_init_select.

  @see mysql_init_select

  @retval TRUE An error occurred
  @retval FALSE The new SELECT_LEX was successfully allocated.
*/

bool
mysql_new_select(LEX *lex, bool move_down)
{
  SELECT_LEX *select_lex;
  THD *thd= lex->thd;
  Name_resolution_context *outer_context= lex->current_context();
  DBUG_ENTER("mysql_new_select");

  if (!(select_lex= new (thd->mem_root) SELECT_LEX()))
    DBUG_RETURN(1);
  select_lex->select_number= ++thd->select_number;
  select_lex->parent_lex= lex; /* Used in init_query. */
  select_lex->init_query();
  select_lex->init_select();
  lex->nest_level++;
  if (lex->nest_level > (int) MAX_SELECT_NESTING)
  {
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, MYF(0));
    DBUG_RETURN(1);
  }
  select_lex->nest_level= lex->nest_level;
  if (move_down)
  {
    SELECT_LEX_UNIT *unit;
    lex->subqueries= TRUE;
    /* first select_lex of subselect or derived table */
    if (!(unit= new (thd->mem_root) SELECT_LEX_UNIT()))
      DBUG_RETURN(1);

    unit->init_query();
    unit->init_select();
    unit->thd= thd;
    unit->include_down(lex->current_select);
    unit->link_next= 0;
    unit->link_prev= 0;
    select_lex->include_down(unit);
    /*
      By default we assume that it is usual subselect and we have outer name
      resolution context, if no we will assign it to 0 later
    */
    if (select_lex->outer_select()->parsing_place == IN_ON)
      /*
        This subquery is part of an ON clause, so we need to link the
        name resolution context for this subquery with the ON context.

        @todo In which cases is this not the same as
        &select_lex->outer_select()->context?
      */
      select_lex->context.outer_context= outer_context;
    else
      select_lex->context.outer_context= &select_lex->outer_select()->context;
  }
  else
  {
    if (lex->current_select->order_list.first && !lex->current_select->braces)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "UNION", "ORDER BY");
      DBUG_RETURN(1);
    }
    select_lex->include_neighbour(lex->current_select);
    SELECT_LEX_UNIT *unit= select_lex->master_unit();                              
    if (!unit->fake_select_lex && unit->add_fake_select_lex(lex->thd))
      DBUG_RETURN(1);
    select_lex->context.outer_context= 
                unit->first_select()->context.outer_context;
  }

  select_lex->master_unit()->global_parameters= select_lex;
  select_lex->include_global((st_select_lex_node**)&lex->all_selects_list);
  lex->current_select= select_lex;
  /*
    in subquery is SELECT query and we allow resolution of names in SELECT
    list
  */
  select_lex->context.resolve_in_select_list= TRUE;
  DBUG_RETURN(0);
}

/**
  Create a select to return the same output as 'SELECT @@var_name'.

  Used for SHOW COUNT(*) [ WARNINGS | ERROR].

  This will crash with a core dump if the variable doesn't exists.

  @param var_name		Variable name
*/

void create_select_for_variable(const char *var_name)
{
  THD *thd;
  LEX *lex;
  LEX_STRING tmp, null_lex_string;
  Item *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *end;
  DBUG_ENTER("create_select_for_variable");

  thd= current_thd;
  lex= thd->lex;
  mysql_init_select(lex);
  lex->sql_command= SQLCOM_SELECT;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  memset(&null_lex_string, 0, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if ((var= get_system_var(thd, OPT_SESSION, tmp, null_lex_string)))
  {
    end= strxmov(buff, "@@session.", var_name, NullS);
    var->item_name.copy(buff, end - buff);
    add_item_to_list(thd, var);
  }
  DBUG_VOID_RETURN;
}


void mysql_init_multi_delete(LEX *lex)
{
  lex->sql_command=  SQLCOM_DELETE_MULTI;
  mysql_init_select(lex);
  lex->select_lex.select_limit= 0;
  lex->unit.select_limit_cnt= HA_POS_ERROR;
  lex->select_lex.table_list.save_and_clear(&lex->auxiliary_table_list);
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
}


/*
  When you modify mysql_parse(), you may need to mofify
  mysql_test_parse_for_slave() in this same file.
*/

/**
  Parse a query.

  @param       thd     Current thread
  @param       rawbuf  Begining of the query text
  @param       length  Length of the query text
  @param[out]  found_semicolon For multi queries, position of the character of
                               the next query in the query text.
*/

void mysql_parse(THD *thd, char *rawbuf, uint length,
                 Parser_state *parser_state)
{
  int error __attribute__((unused));
  DBUG_ENTER("mysql_parse");

  DBUG_EXECUTE_IF("parser_debug", turn_parser_debug_on(););

  /*
    Warning.
    The purpose of query_cache_send_result_to_client() is to lookup the
    query in the query cache first, to avoid parsing and executing it.
    So, the natural implementation would be to:
    - first, call query_cache_send_result_to_client,
    - second, if caching failed, initialise the lexical and syntactic parser.
    The problem is that the query cache depends on a clean initialization
    of (among others) lex->safe_to_cache_query and thd->server_status,
    which are reset respectively in
    - lex_start()
    - mysql_reset_thd_for_next_command()
    So, initializing the lexical analyser *before* using the query cache
    is required for the cache to work properly.
    FIXME: cleanup the dependencies in the code to simplify this.
  */
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  if (query_cache_send_result_to_client(thd, rawbuf, length) <= 0)
  {
    LEX *lex= thd->lex;

    bool err= parse_sql(thd, parser_state, NULL);

    const char *found_semicolon= parser_state->m_lip.found_semicolon;
    size_t      qlen= found_semicolon
                      ? (found_semicolon - thd->query())
                      : thd->query_length();

    if (!err)
    {
      /*
        See whether we can do any query rewriting. opt_log_raw only controls
        writing to the general log, so rewriting still needs to happen because
        the other logs (binlog, slow query log, ...) can not be set to raw mode
        for security reasons.
        Query-cache only handles SELECT, which we don't rewrite, so it's no
        concern of ours.
        We're not general-logging if we're the slave, or if we've already
        done raw-logging earlier.
        Sub-routines of mysql_rewrite_query() should try to only rewrite when
        necessary (e.g. not do password obfuscation when query contains no
        password), but we can optimize out even those necessary rewrites when
        no logging happens at all. If rewriting does not happen here,
        thd->rewritten_query is still empty from being reset in alloc_query().
      */
      bool general= (opt_log && ! (opt_log_raw || thd->slave_thread));

      if (general || opt_slow_log || opt_bin_log)
      {
        mysql_rewrite_query(thd);

        if (thd->rewritten_query.length())
          lex->safe_to_cache_query= false; // see comments below
      }

      if (general)
      {
        if (thd->rewritten_query.length())
          general_log_write(thd, COM_QUERY, thd->rewritten_query.c_ptr_safe(),
                                            thd->rewritten_query.length());
        else
          general_log_write(thd, COM_QUERY, thd->query(), qlen);
      }
    }

    if (!err)
    {
      thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                   sql_statement_info[thd->lex->sql_command].m_key);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (mqh_used && thd->get_user_connect() &&
	  check_mqh(thd, lex->sql_command))
      {
	thd->net.error = 0;
      }
      else
#endif
      {
	if (! thd->is_error())
	{
          /*
            Binlog logs a string starting from thd->query and having length
            thd->query_length; so we set thd->query_length correctly (to not
            log several statements in one event, when we executed only first).
            We set it to not see the ';' (otherwise it would get into binlog
            and Query_log_event::print() would give ';;' output).
            This also helps display only the current query in SHOW
            PROCESSLIST.
            Note that we don't need LOCK_thread_count to modify query_length.
          */
          if (found_semicolon && (ulong) (found_semicolon - thd->query()))
            thd->set_query_inner(thd->query(),
                                 (uint32) (found_semicolon -
                                           thd->query() - 1),
                                 thd->charset());
          /* Actually execute the query */
          if (found_semicolon)
          {
            lex->safe_to_cache_query= 0;
            thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
          }
          lex->set_trg_event_type_for_tables();
          MYSQL_QUERY_EXEC_START(thd->query(),
                                 thd->thread_id,
                                 (char *) (thd->db ? thd->db : ""),
                                 &thd->security_ctx->priv_user[0],
                                 (char *) thd->security_ctx->host_or_ip,
                                 0);
          if (unlikely(thd->security_ctx->password_expired &&
                       !lex->is_change_password &&
                       lex->sql_command != SQLCOM_SET_OPTION))
          {
            my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
            error= 1;
          }
          else
            error= mysql_execute_command(thd);
          if (error == 0 &&
              thd->variables.gtid_next.type == GTID_GROUP &&
              thd->owned_gtid.sidno != 0 &&
              (thd->lex->sql_command == SQLCOM_COMMIT ||
               stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END)))
          {
            // This is executed at the end of a DDL statement or after
            // COMMIT.  It ensures that an empty group is logged if
            // needed.
            error= gtid_empty_group_log_and_cleanup(thd);
          }
          MYSQL_QUERY_EXEC_DONE(error);
	}
      }
    }
    else
    {
      /* Instrument this broken statement as "statement/sql/error" */
      thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                   sql_statement_info[SQLCOM_END].m_key);

      DBUG_ASSERT(thd->is_error());
      DBUG_PRINT("info",("Command aborted. Fatal_error: %d",
			 thd->is_fatal_error));

      query_cache_abort(&thd->query_cache_tls);
    }

    THD_STAGE_INFO(thd, stage_freeing_items);
    sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
    sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);
    thd->end_statement();
    thd->cleanup_after_query();
    DBUG_ASSERT(thd->change_list.is_empty());
  }
  else
  {
    /*
      Query cache hit. We need to write the general log here if
      we haven't already logged the statement earlier due to --log-raw.
      Right now, we only cache SELECT results; if the cache ever
      becomes more generic, we should also cache the rewritten
      query-string together with the original query-string (which
      we'd still use for the matching) when we first execute the
      query, and then use the obfuscated query-string for logging
      here when the query is given again.
    */
    thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                 sql_statement_info[SQLCOM_SELECT].m_key);
    if (!opt_log_raw)
      general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());
    parser_state->m_lip.found_semicolon= NULL;
  }

  DBUG_VOID_RETURN;
}


#ifdef HAVE_REPLICATION
/*
  Usable by the replication SQL thread only: just parse a query to know if it
  can be ignored because of replicate-*-table rules.

  @retval
    0	cannot be ignored
  @retval
    1	can be ignored
*/

bool mysql_test_parse_for_slave(THD *thd, char *rawbuf, uint length)
{
  LEX *lex= thd->lex;
  bool error= 0;
  PSI_statement_locker *parent_locker= thd->m_statement_psi;
  DBUG_ENTER("mysql_test_parse_for_slave");

  Parser_state parser_state;
  if (!(error= parser_state.init(thd, rawbuf, length)))
  {
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    thd->m_statement_psi= NULL;
    if (!parse_sql(thd, & parser_state, NULL) &&
        all_tables_not_ok(thd, lex->select_lex.table_list.first))
      error= 1;                  /* Ignore question */
    thd->m_statement_psi= parent_locker;
    thd->end_statement();
  }
  thd->cleanup_after_query();
  DBUG_RETURN(error);
}
#endif



/**
  Store field definition for create.

  @return
    Return 0 if ok
*/

bool add_field_to_list(THD *thd, LEX_STRING *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier,
		       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
		       char *change,
                       List<String> *interval_list, const CHARSET_INFO *cs,
		       uint uint_geom_type)
{
  Create_field *new_field;
  LEX  *lex= thd->lex;
  uint8 datetime_precision= decimals ? atoi(decimals) : 0;
  DBUG_ENTER("add_field_to_list");

  if (check_string_char_length(field_name, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), field_name->str); /* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (type_modifier & PRI_KEY_FLAG)
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::PRIMARY, null_lex_str,
                      &default_key_create_info,
                      0, lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::UNIQUE, null_lex_str,
                 &default_key_create_info, 0,
                 lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }

  if (default_value)
  {
    /* 
      Default value should be literal => basic constants =>
      no need fix_fields()

      We allow only CURRENT_TIMESTAMP as function default for the TIMESTAMP or
      DATETIME types.
    */
    if (default_value->type() == Item::FUNC_ITEM && 
        (static_cast<Item_func*>(default_value)->functype() !=
         Item_func::NOW_FUNC ||
         (!real_type_with_now_as_default(type)) ||
         default_value->decimals != datetime_precision))
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      DBUG_RETURN(1);
    }
    else if (default_value->type() == Item::NULL_ITEM)
    {
      default_value= 0;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
	  NOT_NULL_FLAG)
      {
	my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
	DBUG_RETURN(1);
      }
    }
    else if (type_modifier & AUTO_INCREMENT_FLAG)
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      DBUG_RETURN(1);
    }
  }

  if (on_update_value &&
      (!real_type_with_now_on_update(type) ||
       on_update_value->decimals != datetime_precision))
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    DBUG_RETURN(1);
  }

  if (!(new_field= new Create_field()) ||
      new_field->init(thd, field_name->str, type, length, decimals, type_modifier,
                      default_value, on_update_value, comment, change,
                      interval_list, cs, uint_geom_type))
    DBUG_RETURN(1);

  lex->alter_info.create_list.push_back(new_field);
  lex->last_field=new_field;
  DBUG_RETURN(0);
}


/** Store position for column in ALTER TABLE .. ADD column. */

void store_position_for_column(const char *name)
{
  current_thd->lex->last_field->after=(char*) (name);
}


/**
  save order by and tables in own lists.
*/

bool add_to_list(THD *thd, SQL_I_List<ORDER> &list, Item *item,bool asc)
{
  ORDER *order;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER))))
    DBUG_RETURN(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->direction= (asc ? ORDER::ORDER_ASC : ORDER::ORDER_DESC);
  order->used_alias= false;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list(order, &order->next);
  DBUG_RETURN(0);
}


/**
  Add a table to list of used tables.

  @param table		Table to add
  @param alias		alias for table (or null if no alias)
  @param table_options	A set of the following bits:
                         - TL_OPTION_UPDATING : Table will be updated
                         - TL_OPTION_FORCE_INDEX : Force usage of index
                         - TL_OPTION_ALIAS : an alias in multi table DELETE
  @param lock_type	How table should be locked
  @param mdl_type       Type of metadata lock to acquire on the table.
  @param use_index	List of indexed used in USE INDEX
  @param ignore_index	List of indexed used in IGNORE INDEX

  @retval
      0		Error
  @retval
    \#	Pointer to TABLE_LIST element added to the total table list
*/

TABLE_LIST *st_select_lex::add_table_to_list(THD *thd,
					     Table_ident *table,
					     LEX_STRING *alias,
					     ulong table_options,
					     thr_lock_type lock_type,
					     enum_mdl_type mdl_type,
					     List<Index_hint> *index_hints_arg,
                                             List<String> *partition_names,
                                             LEX_STRING *option)
{
  TABLE_LIST *ptr;
  TABLE_LIST *previous_table_ref; /* The table preceding the current one. */
  char *alias_str;
  LEX *lex= thd->lex;
  DBUG_ENTER("add_table_to_list");
  LINT_INIT(previous_table_ref);

  if (!table)
    DBUG_RETURN(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (!test(table_options & TL_OPTION_ALIAS))
  {
    enum_ident_name_check ident_check_status=
      check_table_name(table->table.str, table->table.length, FALSE);
    if (ident_check_status == IDENT_NAME_WRONG)
    {
      my_error(ER_WRONG_TABLE_NAME, MYF(0), table->table.str);
      DBUG_RETURN(0);
    }
    else if (ident_check_status == IDENT_NAME_TOO_LONG)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), table->table.str);
      DBUG_RETURN(0);
    }
  }
  if (table->is_derived_table() == FALSE && table->db.str &&
      (check_and_convert_db_name(&table->db, FALSE) != IDENT_NAME_OK))
    DBUG_RETURN(0);

  if (!alias)					/* Alias is case sensitive */
  {
    if (table->sel)
    {
      my_message(ER_DERIVED_MUST_HAVE_ALIAS,
                 ER(ER_DERIVED_MUST_HAVE_ALIAS), MYF(0));
      DBUG_RETURN(0);
    }
    if (!(alias_str= (char*) thd->memdup(alias_str,table->table.length+1)))
      DBUG_RETURN(0);
  }
  if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(0);				/* purecov: inspected */
  if (table->db.str)
  {
    ptr->is_fqtn= TRUE;
    ptr->db= table->db.str;
    ptr->db_length= table->db.length;
  }
  else if (lex->copy_db_to(&ptr->db, &ptr->db_length))
    DBUG_RETURN(0);
  else
    ptr->is_fqtn= FALSE;

  ptr->alias= alias_str;
  ptr->is_alias= alias ? TRUE : FALSE;
  if (lower_case_table_names && table->table.length)
    table->table.length= my_casedn_str(files_charset_info, table->table.str);
  ptr->table_name=table->table.str;
  ptr->table_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->updating=    test(table_options & TL_OPTION_UPDATING);
  /* TODO: remove TL_OPTION_FORCE_INDEX as it looks like it's not used */
  ptr->force_index= test(table_options & TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= test(table_options & TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  if (!ptr->derived && is_infoschema_db(ptr->db, ptr->db_length))
  {
    ST_SCHEMA_TABLE *schema_table;
    if (ptr->updating &&
        /* Special cases which are processed by commands itself */
        lex->sql_command != SQLCOM_CHECK &&
        lex->sql_command != SQLCOM_CHECKSUM)
    {
      my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
               thd->security_ctx->priv_user,
               thd->security_ctx->priv_host,
               INFORMATION_SCHEMA_NAME.str);
      DBUG_RETURN(0);
    }
    schema_table= find_schema_table(thd, ptr->table_name);
    if (!schema_table ||
        (schema_table->hidden && 
         ((sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) == 0 || 
          /*
            this check is used for show columns|keys from I_S hidden table
          */
          lex->sql_command == SQLCOM_SHOW_FIELDS ||
          lex->sql_command == SQLCOM_SHOW_KEYS)))
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0),
               ptr->table_name, INFORMATION_SCHEMA_NAME.str);
      DBUG_RETURN(0);
    }
    ptr->schema_table_name= ptr->table_name;
    ptr->schema_table= schema_table;
  }
  ptr->select_lex=  lex->current_select;
  ptr->cacheable_table= 1;
  ptr->index_hints= index_hints_arg;
  ptr->option= option ? option->str : 0;
  /* check that used name is unique */
  if (lock_type != TL_IGNORE)
  {
    TABLE_LIST *first_table= table_list.first;
    if (lex->sql_command == SQLCOM_CREATE_VIEW)
      first_table= first_table ? first_table->next_local : NULL;
    for (TABLE_LIST *tables= first_table ;
	 tables ;
	 tables=tables->next_local)
    {
      if (!my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
	  !strcmp(ptr->db, tables->db))
      {
	my_error(ER_NONUNIQ_TABLE, MYF(0), alias_str); /* purecov: tested */
	DBUG_RETURN(0);				/* purecov: tested */
      }
    }
  }
  /* Store the table reference preceding the current one. */
  if (table_list.elements > 0)
  {
    /*
      table_list.next points to the last inserted TABLE_LIST->next_local'
      element
      We don't use the offsetof() macro here to avoid warnings from gcc
    */
    previous_table_ref= (TABLE_LIST*) ((char*) table_list.next -
                                       ((char*) &(ptr->next_local) -
                                        (char*) ptr));
    /*
      Set next_name_resolution_table of the previous table reference to point
      to the current table reference. In effect the list
      TABLE_LIST::next_name_resolution_table coincides with
      TABLE_LIST::next_local. Later this may be changed in
      store_top_level_join_columns() for NATURAL/USING joins.
    */
    previous_table_ref->next_name_resolution_table= ptr;
  }

  /*
    Link the current table reference in a local list (list for current select).
    Notice that as a side effect here we set the next_local field of the
    previous table reference to 'ptr'. Here we also add one element to the
    list 'table_list'.
  */
  table_list.link_in_list(ptr, &ptr->next_local);
  ptr->next_name_resolution_table= NULL;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  ptr->partition_names= partition_names;
#endif /* WITH_PARTITION_STORAGE_ENGINE */
  /* Link table in global list (all used tables) */
  lex->add_to_query_tables(ptr);
  ptr->mdl_request.init(MDL_key::TABLE, ptr->db, ptr->table_name, mdl_type,
                        MDL_TRANSACTION);
  if (table->is_derived_table())
  {
    ptr->effective_algorithm= DERIVED_ALGORITHM_TMPTABLE;
    ptr->derived_key_list.empty();
  }
  DBUG_RETURN(ptr);
}


/**
  Initialize a new table list for a nested join.

    The function initializes a structure of the TABLE_LIST type
    for a nested join. It sets up its nested join list as empty.
    The created structure is added to the front of the current
    join list in the st_select_lex object. Then the function
    changes the current nest level for joins to refer to the newly
    created empty list after having saved the info on the old level
    in the initialized structure.

  @param thd         current thread

  @retval
    0   if success
  @retval
    1   otherwise
*/

bool st_select_lex::init_nested_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;
  DBUG_ENTER("init_nested_join");

  if (!(ptr= (TABLE_LIST*) thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST))+
                                       sizeof(NESTED_JOIN))))
    DBUG_RETURN(1);
  nested_join= ptr->nested_join=
    ((NESTED_JOIN*) ((uchar*) ptr + ALIGN_SIZE(sizeof(TABLE_LIST))));

  join_list->push_front(ptr);
  ptr->embedding= embedding;
  ptr->join_list= join_list;
  ptr->alias= (char*) "(nested_join)";
  embedding= ptr;
  join_list= &nested_join->join_list;
  join_list->empty();
  DBUG_RETURN(0);
}


/**
  End a nested join table list.

    The function returns to the previous join nest level.
    If the current level contains only one member, the function
    moves it one level up, eliminating the nest.

  @param thd         current thread

  @return
    - Pointer to TABLE_LIST element added to the total table list, if success
    - 0, otherwise
*/

TABLE_LIST *st_select_lex::end_nested_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;
  DBUG_ENTER("end_nested_join");

  DBUG_ASSERT(embedding);
  ptr= embedding;
  join_list= ptr->join_list;
  embedding= ptr->embedding;
  nested_join= ptr->nested_join;
  if (nested_join->join_list.elements == 1)
  {
    TABLE_LIST *embedded= nested_join->join_list.head();
    join_list->pop();
    embedded->join_list= join_list;
    embedded->embedding= embedding;
    join_list->push_front(embedded);
    ptr= embedded;
  }
  else if (nested_join->join_list.elements == 0)
  {
    join_list->pop();
    ptr= 0;                                     // return value
  }
  DBUG_RETURN(ptr);
}


/**
  Nest last join operation.

    The function nest last join operation as if it was enclosed in braces.

  @param thd         current thread

  @retval
    0  Error
  @retval
    \#  Pointer to TABLE_LIST element created for the new nested join
*/

TABLE_LIST *st_select_lex::nest_last_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;
  List<TABLE_LIST> *embedded_list;
  DBUG_ENTER("nest_last_join");

  if (!(ptr= (TABLE_LIST*) thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST))+
                                       sizeof(NESTED_JOIN))))
    DBUG_RETURN(0);
  nested_join= ptr->nested_join=
    ((NESTED_JOIN*) ((uchar*) ptr + ALIGN_SIZE(sizeof(TABLE_LIST))));

  ptr->embedding= embedding;
  ptr->join_list= join_list;
  ptr->alias= (char*) "(nest_last_join)";
  embedded_list= &nested_join->join_list;
  embedded_list->empty();

  for (uint i=0; i < 2; i++)
  {
    TABLE_LIST *table= join_list->pop();
    table->join_list= embedded_list;
    table->embedding= ptr;
    embedded_list->push_back(table);
    if (table->natural_join)
    {
      ptr->is_natural_join= TRUE;
      /*
        If this is a JOIN ... USING, move the list of joined fields to the
        table reference that describes the join.
      */
      if (prev_join_using)
        ptr->join_using_fields= prev_join_using;
    }
  }
  join_list->push_front(ptr);
  nested_join->used_tables= nested_join->not_null_tables= (table_map) 0;
  DBUG_RETURN(ptr);
}


/**
  Add a table to the current join list.

    The function puts a table in front of the current join list
    of st_select_lex object.
    Thus, joined tables are put into this list in the reverse order
    (the most outer join operation follows first).

  @param table       the table to add

  @return
    None
*/

void st_select_lex::add_joined_table(TABLE_LIST *table)
{
  DBUG_ENTER("add_joined_table");
  join_list->push_front(table);
  table->join_list= join_list;
  table->embedding= embedding;
  DBUG_VOID_RETURN;
}


/**
  Convert a right join into equivalent left join.

    The function takes the current join list t[0],t[1] ... and
    effectively converts it into the list t[1],t[0] ...
    Although the outer_join flag for the new nested table contains
    JOIN_TYPE_RIGHT, it will be handled as the inner table of a left join
    operation.

  EXAMPLES
  @verbatim
    SELECT * FROM t1 RIGHT JOIN t2 ON on_expr =>
      SELECT * FROM t2 LEFT JOIN t1 ON on_expr

    SELECT * FROM t1,t2 RIGHT JOIN t3 ON on_expr =>
      SELECT * FROM t1,t3 LEFT JOIN t2 ON on_expr

    SELECT * FROM t1,t2 RIGHT JOIN (t3,t4) ON on_expr =>
      SELECT * FROM t1,(t3,t4) LEFT JOIN t2 ON on_expr

    SELECT * FROM t1 LEFT JOIN t2 ON on_expr1 RIGHT JOIN t3  ON on_expr2 =>
      SELECT * FROM t3 LEFT JOIN (t1 LEFT JOIN t2 ON on_expr2) ON on_expr1
   @endverbatim

  @param thd         current thread

  @return
    - Pointer to the table representing the inner table, if success
    - 0, otherwise
*/

TABLE_LIST *st_select_lex::convert_right_join()
{
  TABLE_LIST *tab2= join_list->pop();
  TABLE_LIST *tab1= join_list->pop();
  DBUG_ENTER("convert_right_join");

  join_list->push_front(tab2);
  join_list->push_front(tab1);
  tab1->outer_join|= JOIN_TYPE_RIGHT;

  DBUG_RETURN(tab1);
}

/**
  Set lock for all tables in current select level.

  @param lock_type			Lock to set for tables

  @note
    If lock is a write lock, then tables->updating is set 1
    This is to get tables_ok to know that the table is updated by the
    query
*/

void st_select_lex::set_lock_for_tables(thr_lock_type lock_type)
{
  bool for_update= lock_type >= TL_READ_NO_INSERT;
  DBUG_ENTER("set_lock_for_tables");
  DBUG_PRINT("enter", ("lock_type: %d  for_update: %d", lock_type,
		       for_update));
  for (TABLE_LIST *tables= table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
    tables->mdl_request.set_type((lock_type >= TL_WRITE_ALLOW_WRITE) ?
                                 MDL_SHARED_WRITE : MDL_SHARED_READ);
  }
  DBUG_VOID_RETURN;
}


/**
  Create a fake SELECT_LEX for a unit.

    The method create a fake SELECT_LEX object for a unit.
    This object is created for any union construct containing a union
    operation and also for any single select union construct of the form
    @verbatim
    (SELECT ... ORDER BY order_list [LIMIT n]) ORDER BY ... 
    @endvarbatim
    or of the form
    @varbatim
    (SELECT ... ORDER BY LIMIT n) ORDER BY ...
    @endvarbatim
  
  @param thd_arg		   thread handle

  @note
    The object is used to retrieve rows from the temporary table
    where the result on the union is obtained.

  @retval
    1     on failure to create the object
  @retval
    0     on success
*/

bool st_select_lex_unit::add_fake_select_lex(THD *thd_arg)
{
  SELECT_LEX *first_sl= first_select();
  DBUG_ENTER("add_fake_select_lex");
  DBUG_ASSERT(!fake_select_lex);

  if (!(fake_select_lex= new (thd_arg->mem_root) SELECT_LEX()))
      DBUG_RETURN(1);
  fake_select_lex->include_standalone(this, 
                                      (SELECT_LEX_NODE**)&fake_select_lex);
  fake_select_lex->select_number= INT_MAX;
  fake_select_lex->parent_lex= thd_arg->lex; /* Used in init_query. */
  fake_select_lex->make_empty_select();
  fake_select_lex->linkage= GLOBAL_OPTIONS_TYPE;
  fake_select_lex->select_limit= 0;

  fake_select_lex->context.outer_context=first_sl->context.outer_context;
  /* allow item list resolving in fake select for ORDER BY */
  fake_select_lex->context.resolve_in_select_list= TRUE;
  fake_select_lex->context.select_lex= fake_select_lex;

  if (!is_union())
  {
    /* 
      This works only for 
      (SELECT ... ORDER BY list [LIMIT n]) ORDER BY order_list [LIMIT m],
      (SELECT ... LIMIT n) ORDER BY order_list [LIMIT m]
      just before the parser starts processing order_list
    */ 
    global_parameters= fake_select_lex;
    fake_select_lex->no_table_names_allowed= 1;
    thd_arg->lex->current_select= fake_select_lex;
  }
  thd_arg->lex->pop_context();
  DBUG_RETURN(0);
}


/**
  Push a new name resolution context for a JOIN ... ON clause to the
  context stack of a query block.

    Create a new name resolution context for a JOIN ... ON clause,
    set the first and last leaves of the list of table references
    to be used for name resolution, and push the newly created
    context to the stack of contexts of the query.

  @param thd       pointer to current thread
  @param left_op   left  operand of the JOIN
  @param right_op  rigth operand of the JOIN

  @retval
    FALSE  if all is OK
  @retval
    TRUE   if a memory allocation error occured
*/

bool
push_new_name_resolution_context(THD *thd,
                                 TABLE_LIST *left_op, TABLE_LIST *right_op)
{
  Name_resolution_context *on_context;
  if (!(on_context= new (thd->mem_root) Name_resolution_context))
    return TRUE;
  on_context->init();
  on_context->first_name_resolution_table=
    left_op->first_leaf_for_name_resolution();
  on_context->last_name_resolution_table=
    right_op->last_leaf_for_name_resolution();
  on_context->select_lex= thd->lex->current_select;
  return thd->lex->push_context(on_context);
}


/**
  Add an ON condition to the second operand of a JOIN ... ON.

    Add an ON condition to the right operand of a JOIN ... ON clause.

  @param b     the second operand of a JOIN ... ON
  @param expr  the condition to be added to the ON clause
*/

void add_join_on(TABLE_LIST *b, Item *expr)
{
  if (expr)
  {
    if (!b->join_cond())
      b->set_join_cond(expr);
    else
    {
      /*
        If called from the parser, this happens if you have both a
        right and left join. If called later, it happens if we add more
        than one condition to the ON clause.
      */
      b->set_join_cond(new Item_cond_and(b->join_cond(), expr));
    }
    b->join_cond()->top_level_item();
  }
}


/**
  Mark that there is a NATURAL JOIN or JOIN ... USING between two
  tables.

    This function marks that table b should be joined with a either via
    a NATURAL JOIN or via JOIN ... USING. Both join types are special
    cases of each other, so we treat them together. The function
    setup_conds() creates a list of equal condition between all fields
    of the same name for NATURAL JOIN or the fields in 'using_fields'
    for JOIN ... USING. The list of equality conditions is stored
    either in b->join_cond(), or in JOIN::conds, depending on whether there
    was an outer join.

  EXAMPLE
  @verbatim
    SELECT * FROM t1 NATURAL LEFT JOIN t2
     <=>
    SELECT * FROM t1 LEFT JOIN t2 ON (t1.i=t2.i and t1.j=t2.j ... )

    SELECT * FROM t1 NATURAL JOIN t2 WHERE <some_cond>
     <=>
    SELECT * FROM t1, t2 WHERE (t1.i=t2.i and t1.j=t2.j and <some_cond>)

    SELECT * FROM t1 JOIN t2 USING(j) WHERE <some_cond>
     <=>
    SELECT * FROM t1, t2 WHERE (t1.j=t2.j and <some_cond>)
   @endverbatim

  @param a		  Left join argument
  @param b		  Right join argument
  @param using_fields    Field names from USING clause
*/

void add_join_natural(TABLE_LIST *a, TABLE_LIST *b, List<String> *using_fields,
                      SELECT_LEX *lex)
{
  b->natural_join= a;
  lex->prev_join_using= using_fields;
}


/**
  kill on thread.

  @param thd			Thread class
  @param id			Thread id
  @param only_kill_query        Should it kill the query or the connection

  @note
    This is written such that we have a short lock on LOCK_thread_count
*/

uint kill_one_thread(THD *thd, ulong id, bool only_kill_query)
{
  THD *tmp= NULL;
  uint error=ER_NO_SUCH_THREAD;
  DBUG_ENTER("kill_one_thread");
  DBUG_PRINT("enter", ("id=%lu only_kill=%d", id, only_kill_query));

  mysql_mutex_lock(&LOCK_thread_count);
  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
  {
    if ((*it)->get_command() == COM_DAEMON)
      continue;
    if ((*it)->thread_id == id)
    {
      tmp= *it;
      mysql_mutex_lock(&tmp->LOCK_thd_data);    // Lock from delete
      break;
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {

    /*
      If we're SUPER, we can KILL anything, including system-threads.
      No further checks.

      KILLer: thd->security_ctx->user could in theory be NULL while
      we're still in "unauthenticated" state. This is a theoretical
      case (the code suggests this could happen, so we play it safe).

      KILLee: tmp->security_ctx->user will be NULL for system threads.
      We need to check so Jane Random User doesn't crash the server
      when trying to kill a) system threads or b) unauthenticated users'
      threads (Bug#43748).

      If user of both killer and killee are non-NULL, proceed with
      slayage if both are string-equal.
    */

    if ((thd->security_ctx->master_access & SUPER_ACL) ||
        thd->security_ctx->user_matches(tmp->security_ctx))
    {
      /* process the kill only if thread is not already undergoing any kill
         connection.
      */
      if (tmp->killed != THD::KILL_CONNECTION)
      {
        tmp->awake(only_kill_query ? THD::KILL_QUERY : THD::KILL_CONNECTION);
      }
      error= 0;
    }
    else
      error=ER_KILL_DENIED_ERROR;
    mysql_mutex_unlock(&tmp->LOCK_thd_data);
  }
  DBUG_PRINT("exit", ("%d", error));
  DBUG_RETURN(error);
}


/*
  kills a thread and sends response

  SYNOPSIS
    sql_kill()
    thd			Thread class
    id			Thread id
    only_kill_query     Should it kill the query or the connection
*/

static
void sql_kill(THD *thd, ulong id, bool only_kill_query)
{
  uint error;
  if (!(error= kill_one_thread(thd, id, only_kill_query)))
  {
    if (! thd->killed)
      my_ok(thd);
  }
  else
    my_error(error, MYF(0), id);
}


/** If pointer is not a null pointer, append filename to it. */

bool append_file_to_dir(THD *thd, const char **filename_ptr,
                        const char *table_name)
{
  char buff[FN_REFLEN],*ptr, *end;
  if (!*filename_ptr)
    return 0;					// nothing to do

  /* Check that the filename is not too long and it's a hard path */
  if (strlen(*filename_ptr)+strlen(table_name) >= FN_REFLEN-1 ||
      !test_if_hard_path(*filename_ptr))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), *filename_ptr);
    return 1;
  }
  /* Fix is using unix filename format on dos */
  strmov(buff,*filename_ptr);
  end=convert_dirname(buff, *filename_ptr, NullS);
  if (!(ptr= (char*) thd->alloc((size_t) (end-buff) + strlen(table_name)+1)))
    return 1;					// End of memory
  *filename_ptr=ptr;
  strxmov(ptr,buff,table_name,NullS);
  return 0;
}


/**
  Check if the select is a simple select (not an union).

  @retval
    0	ok
  @retval
    1	error	; In this case the error messege is sent to the client
*/

bool check_simple_select()
{
  THD *thd= current_thd;
  LEX *lex= thd->lex;
  if (lex->current_select != &lex->select_lex)
  {
    char command[80];
    Lex_input_stream *lip= & thd->m_parser_state->m_lip;
    strmake(command, lip->yylval->symbol.str,
	    min<size_t>(lip->yylval->symbol.length, sizeof(command)-1));
    my_error(ER_CANT_USE_OPTION_HERE, MYF(0), command);
    return 1;
  }
  return 0;
}


Comp_creator *comp_eq_creator(bool invert)
{
  return invert?(Comp_creator *)&ne_creator:(Comp_creator *)&eq_creator;
}


Comp_creator *comp_ge_creator(bool invert)
{
  return invert?(Comp_creator *)&lt_creator:(Comp_creator *)&ge_creator;
}


Comp_creator *comp_gt_creator(bool invert)
{
  return invert?(Comp_creator *)&le_creator:(Comp_creator *)&gt_creator;
}


Comp_creator *comp_le_creator(bool invert)
{
  return invert?(Comp_creator *)&gt_creator:(Comp_creator *)&le_creator;
}


Comp_creator *comp_lt_creator(bool invert)
{
  return invert?(Comp_creator *)&ge_creator:(Comp_creator *)&lt_creator;
}


Comp_creator *comp_ne_creator(bool invert)
{
  return invert?(Comp_creator *)&eq_creator:(Comp_creator *)&ne_creator;
}


/**
  Construct ALL/ANY/SOME subquery Item.

  @param left_expr   pointer to left expression
  @param cmp         compare function creator
  @param all         true if we create ALL subquery
  @param select_lex  pointer on parsed subquery structure

  @return
    constructed Item (or 0 if out of memory)
*/
Item * all_any_subquery_creator(Item *left_expr,
				chooser_compare_func_creator cmp,
				bool all,
				SELECT_LEX *select_lex)
{
  if ((cmp == &comp_eq_creator) && !all)       //  = ANY <=> IN
    return new Item_in_subselect(left_expr, select_lex);

  if ((cmp == &comp_ne_creator) && all)        // <> ALL <=> NOT IN
    return new Item_func_not(new Item_in_subselect(left_expr, select_lex));

  Item_allany_subselect *it=
    new Item_allany_subselect(left_expr, cmp, select_lex, all);
  if (all)
    return it->upper_item= new Item_func_not_all(it);	/* ALL */

  return it->upper_item= new Item_func_nop_all(it);      /* ANY/SOME */
}


/**
  Perform first stage of privilege checking for SELECT statement.

  @param thd          Thread context.
  @param lex          LEX for SELECT statement.
  @param tables       List of tables used by statement.
  @param first_table  First table in the main SELECT of the SELECT
                      statement.

  @retval FALSE - Success (column-level privilege checks might be required).
  @retval TRUE  - Failure, privileges are insufficient.
*/

bool select_precheck(THD *thd, LEX *lex, TABLE_LIST *tables,
                     TABLE_LIST *first_table)
{
  bool res;
  /*
    lex->exchange != NULL implies SELECT .. INTO OUTFILE and this
    requires FILE_ACL access.
  */
  ulong privileges_requested= lex->exchange ? SELECT_ACL | FILE_ACL :
                                              SELECT_ACL;

  if (tables)
  {
    res= check_table_access(thd,
                            privileges_requested,
                            tables, FALSE, UINT_MAX, FALSE) ||
         (first_table && first_table->schema_table_reformed &&
          check_show_access(thd, first_table));
  }
  else
    res= check_access(thd, privileges_requested, any_db, NULL, NULL, 0, 0);

  return res;
}


/**
  Multi update query pre-check.

  @param thd		Thread handler
  @param tables	Global/local table list (have to be the same)

  @retval
    FALSE OK
  @retval
    TRUE  Error
*/

bool multi_update_precheck(THD *thd, TABLE_LIST *tables)
{
  const char *msg= 0;
  TABLE_LIST *table;
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  DBUG_ENTER("multi_update_precheck");

  if (select_lex->item_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  /*
    Ensure that we have UPDATE or SELECT privilege for each table
    The exact privilege is checked in mysql_multi_update()
  */
  for (table= tables; table; table= table->next_local)
  {
    if (table->derived)
      table->grant.privilege= SELECT_ACL;
    else if ((check_access(thd, UPDATE_ACL, table->db,
                           &table->grant.privilege,
                           &table->grant.m_internal,
                           0, 1) ||
              check_grant(thd, UPDATE_ACL, table, FALSE, 1, TRUE)) &&
             (check_access(thd, SELECT_ACL, table->db,
                           &table->grant.privilege,
                           &table->grant.m_internal,
                           0, 0) ||
              check_grant(thd, SELECT_ACL, table, FALSE, 1, FALSE)))
      DBUG_RETURN(TRUE);

    table->table_in_first_from_clause= 1;
  }
  /*
    Is there tables of subqueries?
  */
  if (&lex->select_lex != lex->all_selects_list)
  {
    DBUG_PRINT("info",("Checking sub query list"));
    for (table= tables; table; table= table->next_global)
    {
      if (!table->table_in_first_from_clause)
      {
	if (check_access(thd, SELECT_ACL, table->db,
                         &table->grant.privilege,
                         &table->grant.m_internal,
                         0, 0) ||
	    check_grant(thd, SELECT_ACL, table, FALSE, 1, FALSE))
	  DBUG_RETURN(TRUE);
      }
    }
  }

  if (select_lex->order_list.elements)
    msg= "ORDER BY";
  else if (select_lex->select_limit)
    msg= "LIMIT";
  if (msg)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}

/**
  Multi delete query pre-check.

  @param thd			Thread handler
  @param tables		Global/local table list

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool multi_delete_precheck(THD *thd, TABLE_LIST *tables)
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
  TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
  DBUG_ENTER("multi_delete_precheck");

  /*
    Temporary tables are pre-opened in 'tables' list only. Here we need to
    initialize TABLE instances in 'aux_tables' list.
  */
  for (TABLE_LIST *tl= aux_tables; tl; tl= tl->next_global)
  {
    if (tl->table)
      continue;

    if (tl->correspondent_table)
      tl->table= tl->correspondent_table->table;
  }

  /* sql_yacc guarantees that tables and aux_tables are not zero */
  DBUG_ASSERT(aux_tables != 0);
  if (check_table_access(thd, SELECT_ACL, tables, FALSE, UINT_MAX, FALSE))
    DBUG_RETURN(TRUE);

  /*
    Since aux_tables list is not part of LEX::query_tables list we
    have to juggle with LEX::query_tables_own_last value to be able
    call check_table_access() safely.
  */
  thd->lex->query_tables_own_last= 0;
  if (check_table_access(thd, DELETE_ACL, aux_tables, FALSE, UINT_MAX, FALSE))
  {
    thd->lex->query_tables_own_last= save_query_tables_own_last;
    DBUG_RETURN(TRUE);
  }
  thd->lex->query_tables_own_last= save_query_tables_own_last;

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) && !select_lex->where)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/*
  Given a table in the source list, find a correspondent table in the
  table references list.

  @param lex Pointer to LEX representing multi-delete.
  @param src Source table to match.
  @param ref Table references list.

  @remark The source table list (tables listed before the FROM clause
  or tables listed in the FROM clause before the USING clause) may
  contain table names or aliases that must match unambiguously one,
  and only one, table in the target table list (table references list,
  after FROM/USING clause).

  @return Matching table, NULL otherwise.
*/

static TABLE_LIST *multi_delete_table_match(LEX *lex, TABLE_LIST *tbl,
                                            TABLE_LIST *tables)
{
  TABLE_LIST *match= NULL;
  DBUG_ENTER("multi_delete_table_match");

  for (TABLE_LIST *elem= tables; elem; elem= elem->next_local)
  {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias)
      continue; /* no match */
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp= my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);

    if (cmp)
      continue;

    if (match)
    {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      DBUG_RETURN(NULL);
    }

    match= elem;
  }

  if (!match)
    my_error(ER_UNKNOWN_TABLE, MYF(0), tbl->table_name, "MULTI DELETE");

  DBUG_RETURN(match);
}


/**
  Link tables in auxilary table list of multi-delete with corresponding
  elements in main table list, and set proper locks for them.

  @param lex   pointer to LEX representing multi-delete

  @retval
    FALSE   success
  @retval
    TRUE    error
*/

bool multi_delete_set_locks_and_link_aux_tables(LEX *lex)
{
  TABLE_LIST *tables= lex->select_lex.table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("multi_delete_set_locks_and_link_aux_tables");

  for (target_tbl= lex->auxiliary_table_list.first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk= multi_delete_table_match(lex, target_tbl, tables);
    if (!walk)
      DBUG_RETURN(TRUE);
    if (!walk->derived)
    {
      target_tbl->table_name= walk->table_name;
      target_tbl->table_name_length= walk->table_name_length;
    }
    walk->updating= target_tbl->updating;
    walk->lock_type= target_tbl->lock_type;
    /* We can assume that tables to be deleted from are locked for write. */
    DBUG_ASSERT(walk->lock_type >= TL_WRITE_ALLOW_WRITE);
    walk->mdl_request.set_type(MDL_SHARED_WRITE);
    target_tbl->correspondent_table= walk;	// Remember corresponding table
  }
  DBUG_RETURN(FALSE);
}


/**
  simple UPDATE query pre-check.

  @param thd		Thread handler
  @param tables	Global table list

  @retval
    FALSE OK
  @retval
    TRUE  Error
*/

bool update_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("update_precheck");
  if (thd->lex->select_lex.item_list.elements != thd->lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(check_one_table_access(thd, UPDATE_ACL, tables));
}


/**
  simple DELETE query pre-check.

  @param thd		Thread handler
  @param tables	Global table list

  @retval
    FALSE  OK
  @retval
    TRUE   error
*/

bool delete_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("delete_precheck");
  if (check_one_table_access(thd, DELETE_ACL, tables))
    DBUG_RETURN(TRUE);
  /* Set privilege for the WHERE clause */
  tables->grant.want_privilege=(SELECT_ACL & ~tables->grant.privilege);
  DBUG_RETURN(FALSE);
}


/**
  simple INSERT query pre-check.

  @param thd		Thread handler
  @param tables	Global table list

  @retval
    FALSE  OK
  @retval
    TRUE   error
*/

bool insert_precheck(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  DBUG_ENTER("insert_precheck");

  /*
    Check that we have modify privileges for the first table and
    select privileges for the rest
  */
  ulong privilege= (INSERT_ACL |
                    (lex->duplicates == DUP_REPLACE ? DELETE_ACL : 0) |
                    (lex->value_list.elements ? UPDATE_ACL : 0));

  if (check_one_table_access(thd, privilege, tables))
    DBUG_RETURN(TRUE);

  if (lex->update_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/**
   Set proper open mode and table type for element representing target table
   of CREATE TABLE statement, also adjust statement table list if necessary.
*/

void create_table_set_open_action_and_adjust_tables(LEX *lex)
{
  TABLE_LIST *create_table= lex->query_tables;

  if (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)
    create_table->open_type= OT_TEMPORARY_ONLY;
  else
    create_table->open_type= OT_BASE_ONLY;

  if (!lex->select_lex.item_list.elements)
  {
    /*
      Avoid opening and locking target table for ordinary CREATE TABLE
      or CREATE TABLE LIKE for write (unlike in CREATE ... SELECT we
      won't do any insertions in it anyway). Not doing this causes
      problems when running CREATE TABLE IF NOT EXISTS for already
      existing log table.
    */
    create_table->lock_type= TL_READ;
  }
}


/**
  CREATE TABLE query pre-check.

  @param thd			Thread handler
  @param tables		Global table list
  @param create_table	        Table which will be created

  @retval
    FALSE   OK
  @retval
    TRUE   Error
*/

bool create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  ulong want_priv;
  bool error= TRUE;                                 // Error message is given
  DBUG_ENTER("create_table_precheck");

  /*
    Require CREATE [TEMPORARY] privilege on new table; for
    CREATE TABLE ... SELECT, also require INSERT.
  */

  want_priv= (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) ?
             CREATE_TMP_ACL :
             (CREATE_ACL | (select_lex->item_list.elements ? INSERT_ACL : 0));

  if (check_access(thd, want_priv, create_table->db,
                   &create_table->grant.privilege,
                   &create_table->grant.m_internal,
                   0, 0))
    goto err;

  /* If it is a merge table, check privileges for merge children. */
  if (lex->create_info.merge_list.first)
  {
    /*
      The user must have (SELECT_ACL | UPDATE_ACL | DELETE_ACL) on the
      underlying base tables, even if there are temporary tables with the same
      names.

      From user's point of view, it might look as if the user must have these
      privileges on temporary tables to create a merge table over them. This is
      one of two cases when a set of privileges is required for operations on
      temporary tables (see also CREATE TABLE).

      The reason for this behavior stems from the following facts:

        - For merge tables, the underlying table privileges are checked only
          at CREATE TABLE / ALTER TABLE time.

          In other words, once a merge table is created, the privileges of
          the underlying tables can be revoked, but the user will still have
          access to the merge table (provided that the user has privileges on
          the merge table itself). 

        - Temporary tables shadow base tables.

          I.e. there might be temporary and base tables with the same name, and
          the temporary table takes the precedence in all operations.

        - For temporary MERGE tables we do not track if their child tables are
          base or temporary. As result we can't guarantee that privilege check
          which was done in presence of temporary child will stay relevant later
          as this temporary table might be removed.

      If SELECT_ACL | UPDATE_ACL | DELETE_ACL privileges were not checked for
      the underlying *base* tables, it would create a security breach as in
      Bug#12771903.
    */

    if (check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
                           lex->create_info.merge_list.first,
                           FALSE, UINT_MAX, FALSE))
      goto err;
  }

  if (want_priv != CREATE_TMP_ACL &&
      check_grant(thd, want_priv, create_table, FALSE, 1, FALSE))
    goto err;

  if (select_lex->item_list.elements)
  {
    /* Check permissions for used tables in CREATE TABLE ... SELECT */
    if (tables && check_table_access(thd, SELECT_ACL, tables, FALSE,
                                     UINT_MAX, FALSE))
      goto err;
  }
  else if (lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE)
  {
    if (check_table_access(thd, SELECT_ACL, tables, FALSE, UINT_MAX, FALSE))
      goto err;
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}


/**
  Check privileges for LOCK TABLES statement.

  @param thd     Thread context.
  @param tables  List of tables to be locked.

  @retval FALSE - Success.
  @retval TRUE  - Failure.
*/

static bool lock_tables_precheck(THD *thd, TABLE_LIST *tables)
{
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();

  for (TABLE_LIST *table= tables; table != first_not_own_table && table;
       table= table->next_global)
  {
    if (is_temporary_table(table))
      continue;

    if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, table,
                           FALSE, 1, FALSE))
      return TRUE;
  }

  return FALSE;
}


/**
  negate given expression.

  @param thd  thread handler
  @param expr expression for negation

  @return
    negated expression
*/

Item *negate_expression(THD *thd, Item *expr)
{
  Item *negated;
  if (expr->type() == Item::FUNC_ITEM &&
      ((Item_func *) expr)->functype() == Item_func::NOT_FUNC)
  {
    /* it is NOT(NOT( ... )) */
    Item *arg= ((Item_func *) expr)->arguments()[0];
    enum_parsing_place place= thd->lex->current_select->parsing_place;
    if (arg->is_bool_func() || place == IN_WHERE || place == IN_HAVING)
      return arg;
    /*
      if it is not boolean function then we have to emulate value of
      not(not(a)), it will be a != 0
    */
    return new Item_func_ne(arg, new Item_int_0());
  }

  if ((negated= expr->neg_transformer(thd)) != 0)
    return negated;
  return new Item_func_not(expr);
}

/**
  Set the specified definer to the default value, which is the
  current user in the thread.
 
  @param[in]  thd       thread handler
  @param[out] definer   definer
*/
 
void get_default_definer(THD *thd, LEX_USER *definer)
{
  const Security_context *sctx= thd->security_ctx;

  definer->user.str= (char *) sctx->priv_user;
  definer->user.length= strlen(definer->user.str);

  definer->host.str= (char *) sctx->priv_host;
  definer->host.length= strlen(definer->host.str);

  definer->password= null_lex_str;
  definer->plugin= empty_lex_str;
  definer->auth= empty_lex_str;
  definer->uses_identified_with_clause= false;
  definer->uses_identified_by_clause= false;
  definer->uses_authentication_string_clause= false;
  definer->uses_identified_by_password_clause= false;
}


/**
  Create default definer for the specified THD.

  @param[in] thd         thread handler

  @return
    - On success, return a valid pointer to the created and initialized
    LEX_USER, which contains definer information.
    - On error, return 0.
*/

LEX_USER *create_default_definer(THD *thd)
{
  LEX_USER *definer;

  if (! (definer= (LEX_USER*) thd->alloc(sizeof(LEX_USER))))
    return 0;

  thd->get_definer(definer);

  return definer;
}


/**
  Create definer with the given user and host names.

  @param[in] thd          thread handler
  @param[in] user_name    user name
  @param[in] host_name    host name

  @return
    - On success, return a valid pointer to the created and initialized
    LEX_USER, which contains definer information.
    - On error, return 0.
*/

LEX_USER *create_definer(THD *thd, LEX_STRING *user_name, LEX_STRING *host_name)
{
  LEX_USER *definer;

  /* Create and initialize. */

  if (! (definer= (LEX_USER*) thd->alloc(sizeof(LEX_USER))))
    return 0;

  definer->user= *user_name;
  definer->host= *host_name;
  definer->password.str= NULL;
  definer->password.length= 0;
  definer->uses_authentication_string_clause= false;
  definer->uses_identified_by_clause= false;
  definer->uses_identified_by_password_clause= false;
  definer->uses_identified_with_clause= false;
  return definer;
}


/**
  Retuns information about user or current user.

  @param[in] thd          thread handler
  @param[in] user         user

  @return
    - On success, return a valid pointer to initialized
    LEX_USER, which contains user information.
    - On error, return 0.
*/

LEX_USER *get_current_user(THD *thd, LEX_USER *user)
{
  if (!user->user.str)  // current_user
  {
    LEX_USER *default_definer= create_default_definer(thd);
    if (default_definer)
    {
      /*
        Inherit parser semantics from the statement in which the user parameter
        was used.
        This is needed because a st_lex_user is both used as a component in an
        AST and as a specifier for a particular user in the ACL subsystem.
      */
      default_definer->uses_authentication_string_clause=
        user->uses_authentication_string_clause;
      default_definer->uses_identified_by_clause=
        user->uses_identified_by_clause;
      default_definer->uses_identified_by_password_clause=
        user->uses_identified_by_password_clause;
      default_definer->uses_identified_with_clause=
        user->uses_identified_with_clause;
      default_definer->plugin.str= user->plugin.str;
      default_definer->plugin.length= user->plugin.length;
      default_definer->auth.str= user->auth.str;
      default_definer->auth.length= user->auth.length;
      return default_definer;
    }
  }

  return user;
}


/**
  Check that byte length of a string does not exceed some limit.

  @param str         string to be checked
  @param err_msg     error message to be displayed if the string is too long
  @param max_length  max length

  @retval
    FALSE   the passed string is not longer than max_length
  @retval
    TRUE    the passed string is longer than max_length

  NOTE
    The function is not used in existing code but can be useful later?
*/

bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length)
{
  if (str->length <= max_byte_length)
    return FALSE;

  my_error(ER_WRONG_STRING_LENGTH, MYF(0), str->str, err_msg, max_byte_length);

  return TRUE;
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
    FALSE   the passed string is not longer than max_char_length
    TRUE    the passed string is longer than max_char_length
*/


bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, const CHARSET_INFO *cs,
                              bool no_error)
{
  int well_formed_error;
  uint res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
                                      max_char_length, &well_formed_error);

  if (!well_formed_error &&  str->length == res)
    return FALSE;

  if (!no_error)
  {
    ErrConvString err(str->str, str->length, cs);
    my_error(ER_WRONG_STRING_LENGTH, MYF(0), err.ptr(), err_msg, max_char_length);
  }
  return TRUE;
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
C_MODE_START

int test_if_data_home_dir(const char *dir)
{
  char path[FN_REFLEN];
  int dir_len;
  DBUG_ENTER("test_if_data_home_dir");

  if (!dir)
    DBUG_RETURN(0);

  (void) fn_format(path, dir, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  dir_len= strlen(path);
  if (mysql_unpacked_real_data_home_len<= dir_len)
  {
    if (dir_len > mysql_unpacked_real_data_home_len &&
        path[mysql_unpacked_real_data_home_len] != FN_LIBCHAR)
      DBUG_RETURN(0);

    if (lower_case_file_system)
    {
      if (!my_strnncoll(default_charset_info, (const uchar*) path,
                        mysql_unpacked_real_data_home_len,
                        (const uchar*) mysql_unpacked_real_data_home,
                        mysql_unpacked_real_data_home_len))
        DBUG_RETURN(1);
    }
    else if (!memcmp(path, mysql_unpacked_real_data_home,
                     mysql_unpacked_real_data_home_len))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

C_MODE_END


/**
  Check that host name string is valid.

  @param[in] str string to be checked

  @return             Operation status
    @retval  FALSE    host name is ok
    @retval  TRUE     host name string is longer than max_length or
                      has invalid symbols
*/

bool check_host_name(LEX_STRING *str)
{
  const char *name= str->str;
  const char *end= str->str + str->length;
  if (check_string_byte_length(str, ER(ER_HOSTNAME), HOSTNAME_LENGTH))
    return TRUE;

  while (name != end)
  {
    if (*name == '@')
    {
      my_printf_error(ER_UNKNOWN_ERROR, 
                      "Malformed hostname (illegal symbol: '%c')", MYF(0),
                      *name);
      return TRUE;
    }
    name++;
  }
  return FALSE;
}


extern int MYSQLparse(void *thd); // from sql_yacc.cc


/**
  This is a wrapper of MYSQLparse(). All the code should call parse_sql()
  instead of MYSQLparse().

  @param thd Thread context.
  @param parser_state Parser state.
  @param creation_ctx Object creation context.

  @return Error status.
    @retval FALSE on success.
    @retval TRUE on parsing error.
*/

bool parse_sql(THD *thd,
               Parser_state *parser_state,
               Object_creation_ctx *creation_ctx)
{
  bool ret_value;
  DBUG_ASSERT(thd->m_parser_state == NULL);
  DBUG_ASSERT(thd->lex->m_sql_cmd == NULL);

  MYSQL_QUERY_PARSE_START(thd->query());
  /* Backup creation context. */

  Object_creation_ctx *backup_ctx= NULL;

  if (creation_ctx)
    backup_ctx= creation_ctx->set_n_backup(thd);

  /* Set parser state. */

  thd->m_parser_state= parser_state;

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
  /* Start Digest */
  thd->m_parser_state->m_lip.m_digest_psi= MYSQL_DIGEST_START(thd->m_statement_psi);
#endif

  /* Parse the query. */

  bool mysql_parse_status= MYSQLparse(thd) != 0;

  /*
    Check that if MYSQLparse() failed either thd->is_error() is set, or an
    internal error handler is set.

    The assert will not catch a situation where parsing fails without an
    error reported if an error handler exists. The problem is that the
    error handler might have intercepted the error, so thd->is_error() is
    not set. However, there is no way to be 100% sure here (the error
    handler might be for other errors than parsing one).
  */

  DBUG_ASSERT(!mysql_parse_status ||
              (mysql_parse_status && thd->is_error()) ||
              (mysql_parse_status && thd->get_internal_handler()));

  /* Reset parser state. */

  thd->m_parser_state= NULL;

  /* Restore creation context. */

  if (creation_ctx)
    creation_ctx->restore_env(thd, backup_ctx);

  /* That's it. */

  ret_value= mysql_parse_status || thd->is_fatal_error;
  MYSQL_QUERY_PARSE_DONE(ret_value);
  return ret_value;
}

/**
  @} (end of group Runtime_Environment)
*/



/**
  Check and merge "CHARACTER SET cs [ COLLATE cl ]" clause

  @param cs character set pointer.
  @param cl collation pointer.

  Check if collation "cl" is applicable to character set "cs".

  If "cl" is NULL (e.g. when COLLATE clause is not specified),
  then simply "cs" is returned.
  
  @return Error status.
    @retval NULL, if "cl" is not applicable to "cs".
    @retval pointer to merged CHARSET_INFO on success.
*/


const CHARSET_INFO*
merge_charset_and_collation(const CHARSET_INFO *cs, const CHARSET_INFO *cl)
{
  if (cl)
  {
    if (!my_charset_same(cs, cl))
    {
      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), cl->name, cs->csname);
      return NULL;
    }
    return cl;
  }
  return cs;
}

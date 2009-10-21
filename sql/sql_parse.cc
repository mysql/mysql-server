/* Copyright 2000-2008 MySQL AB, 2008 Sun Microsystems, Inc.

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

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include <m_ctype.h>
#include <myisam.h>
#include <my_dir.h>

#include "sp_head.h"
#include "sp.h"
#include "sp_cache.h"
#include "events.h"
#include "sql_trigger.h"
#include "probes_mysql.h"

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

/* Used in error handling only */
#define SP_TYPE_STRING(LP) \
  ((LP)->sphead->m_type == TYPE_ENUM_FUNCTION ? "FUNCTION" : "PROCEDURE")
#define SP_COM_STRING(LP) \
  ((LP)->sql_command == SQLCOM_CREATE_SPFUNCTION || \
   (LP)->sql_command == SQLCOM_ALTER_FUNCTION || \
   (LP)->sql_command == SQLCOM_SHOW_CREATE_FUNC || \
   (LP)->sql_command == SQLCOM_DROP_FUNCTION ? \
   "FUNCTION" : "PROCEDURE")

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables);
static bool check_show_create_table_access(THD *thd, TABLE_LIST *table);

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
  { C_STRING_WITH_LEN("Error") }  // Last command number
};

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED", "ROLLBACK ONLY"
};

/**
  Mark a XA transaction as rollback-only if the RM unilaterally
  rolled back the transaction branch.

  @note If a rollback was requested by the RM, this function sets
        the appropriate rollback error code and transits the state
        to XA_ROLLBACK_ONLY.

  @return TRUE if transaction was rolled back or if the transaction
          state is XA_ROLLBACK_ONLY. FALSE otherwise.
*/
static bool xa_trans_rolled_back(XID_STATE *xid_state)
{
  if (xid_state->rm_error)
  {
    switch (xid_state->rm_error) {
    case ER_LOCK_WAIT_TIMEOUT:
      my_error(ER_XA_RBTIMEOUT, MYF(0));
      break;
    case ER_LOCK_DEADLOCK:
      my_error(ER_XA_RBDEADLOCK, MYF(0));
      break;
    default:
      my_error(ER_XA_RBROLLBACK, MYF(0));
    }
    xid_state->xa_state= XA_ROLLBACK_ONLY;
  }

  return (xid_state->xa_state == XA_ROLLBACK_ONLY);
}

/**
  Rollback work done on behalf of at ransaction branch.
*/
static bool xa_trans_rollback(THD *thd)
{
  bool status= test(ha_rollback(thd));

  thd->options&= ~(ulong) OPTION_BEGIN;
  thd->transaction.all.modified_non_trans_table= FALSE;
  thd->server_status&= ~SERVER_STATUS_IN_TRANS;
  xid_cache_delete(&thd->transaction.xid_state);
  thd->transaction.xid_state.xa_state= XA_NOTR;
  thd->transaction.xid_state.rm_error= 0;

  return status;
}

static void unlock_locked_tables(THD *thd)
{
  if (thd->locked_tables)
  {
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;			// Will be automatically closed
    close_thread_tables(thd);			// Free tables
  }
}


bool end_active_trans(THD *thd)
{
  int error=0;
  DBUG_ENTER("end_active_trans");
  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(1);
  }
  if (thd->transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
    DBUG_RETURN(1);
  }
  if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN |
		      OPTION_TABLE_LOCK))
  {
    DBUG_PRINT("info",("options: 0x%llx", thd->options));
    /* Safety if one did "drop table" on locked tables */
    if (!thd->locked_tables)
      thd->options&= ~OPTION_TABLE_LOCK;
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_commit(thd))
      error=1;
  }
  thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
  thd->transaction.all.modified_non_trans_table= FALSE;
  DBUG_RETURN(error);
}


bool begin_trans(THD *thd)
{
  int error=0;
  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return 1;
  }
  if (thd->locked_tables)
  {
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;			// Will be automatically closed
    close_thread_tables(thd);			// Free tables
  }
  if (end_active_trans(thd))
    error= -1;
  else
  {
    thd->options|= OPTION_BEGIN;
    thd->server_status|= SERVER_STATUS_IN_TRANS;
  }
  return error;
}

#ifdef HAVE_REPLICATION
/**
  Returns true if all tables should be ignored.
*/
inline bool all_tables_not_ok(THD *thd, TABLE_LIST *tables)
{
  return rpl_filter->is_on() && tables && !thd->spcont &&
         !rpl_filter->tables_ok(thd->db, tables);
}
#endif


static bool some_non_temp_table_to_be_updated(THD *thd, TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    DBUG_ASSERT(table->db && table->table_name);
    if (table->updating &&
        !find_temporary_table(thd, table->db, table->table_name))
      return 1;
  }
  return 0;
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

void init_update_queries(void)
{
  bzero((uchar*) &sql_command_flags, sizeof(sql_command_flags));

  sql_command_flags[SQLCOM_CREATE_TABLE]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_CREATE_INDEX]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_TABLE]=    CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_TRUNCATE]=       CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_DROP_TABLE]=     CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_LOAD]=           CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_CREATE_DB]=      CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_DB]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_RENAME_TABLE]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_BACKUP_TABLE]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_RESTORE_TABLE]=  CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_INDEX]=     CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_VIEW]=    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_DROP_VIEW]=      CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_EVENT]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_EVENT]=    CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_EVENT]=     CF_CHANGES_DATA;

  sql_command_flags[SQLCOM_UPDATE]=	    CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_UPDATE_MULTI]=   CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_INSERT]=	    CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_INSERT_SELECT]=  CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_DELETE]=         CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_DELETE_MULTI]=   CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_REPLACE]=        CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_REPLACE_SELECT]= CF_CHANGES_DATA | CF_HAS_ROW_COUNT |
                                            CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SELECT]=         CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SET_OPTION]=     CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_DO]=             CF_REEXECUTION_FRAGILE;

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
  sql_command_flags[SQLCOM_SHOW_NEW_MASTER]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOGS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_COLUMN_TYPES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STORAGE_ENGINES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_AUTHORS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CONTRIBUTORS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PRIVILEGES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_WARNS]= CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ERRORS]= CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_MUTEX]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_LOGS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_GRANTS]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_PROC]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_FUNC]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_TRIGGER]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC]=  CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_PROC_CODE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FUNC_CODE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_EVENT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILE]= CF_STATUS_COMMAND;

   sql_command_flags[SQLCOM_SHOW_TABLES]=       (CF_STATUS_COMMAND |
                                                 CF_SHOW_TABLE_COMMAND |
                                                 CF_REEXECUTION_FRAGILE);
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS]= (CF_STATUS_COMMAND |
                                                CF_SHOW_TABLE_COMMAND |
                                                CF_REEXECUTION_FRAGILE);

  /*
    The following is used to preserver CF_ROW_COUNT during the
    a CALL or EXECUTE statement, so the value generated by the
    last called (or executed) statement is preserved.
    See mysql_execute_command() for how CF_ROW_COUNT is used.
  */
  sql_command_flags[SQLCOM_CALL]= 		CF_HAS_ROW_COUNT | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_EXECUTE]= 		CF_HAS_ROW_COUNT;

  /*
    The following admin table operations are allowed
    on log tables.
  */
  sql_command_flags[SQLCOM_REPAIR]=           CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_OPTIMIZE]=         CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_ANALYZE]=          CF_WRITE_LOGS_COMMAND;
}


bool is_update_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CHANGES_DATA) != 0;
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

void execute_init_command(THD *thd, sys_var_str *init_command_var,
			  rw_lock_t *var_mutex)
{
  Vio* save_vio;
  ulong save_client_capabilities;

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  thd->profiling.start_new_query();
  thd->profiling.set_query_source(init_command_var->value,
                                  init_command_var->value_length);
#endif

  thd_proc_info(thd, "Execution of init_command");
  /*
    We need to lock init_command_var because
    during execution of init_command_var query
    values of init_command_var can't be changed
  */
  rw_rdlock(var_mutex);
  save_client_capabilities= thd->client_capabilities;
  thd->client_capabilities|= CLIENT_MULTI_QUERIES;
  /*
    We don't need return result of execution to client side.
    To forbid this we should set thd->net.vio to 0.
  */
  save_vio= thd->net.vio;
  thd->net.vio= 0;
  dispatch_command(COM_QUERY, thd,
                   init_command_var->value,
                   init_command_var->value_length);
  rw_unlock(var_mutex);
  thd->client_capabilities= save_client_capabilities;
  thd->net.vio= save_vio;

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  thd->profiling.finish_current_query();
#endif
}


static void handle_bootstrap_impl(THD *thd)
{
  FILE *file=bootstrap_file;
  char *buff;
  const char* found_semicolon= NULL;

  DBUG_ENTER("handle_bootstrap");

#ifndef EMBEDDED_LIBRARY
  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd;
#endif /* EMBEDDED_LIBRARY */

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;

  thd_proc_info(thd, 0);
  thd->version=refresh_version;
  thd->security_ctx->priv_user=
    thd->security_ctx->user= (char*) my_strdup("boot", MYF(MY_WME));
  thd->security_ctx->priv_host[0]=0;
  /*
    Make the "client" handle multiple results. This is necessary
    to enable stored procedures with SELECTs and Dynamic SQL
    in init-file.
  */
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;

  buff= (char*) thd->net.buff;
  thd->init_for_queries();
  while (fgets(buff, thd->net.max_packet, file))
  {
    char *query, *res;
    /* strlen() can't be deleted because fgets() doesn't return length */
    ulong length= (ulong) strlen(buff);
    while (buff[length-1] != '\n' && !feof(file))
    {
      /*
        We got only a part of the current string. Will try to increase
        net buffer then read the rest of the current string.
      */
      /* purecov: begin tested */
      if (net_realloc(&(thd->net), 2 * thd->net.max_packet))
      {
        net_end_statement(thd);
        bootstrap_error= 1;
        break;
      }
      buff= (char*) thd->net.buff;
      res= fgets(buff + length, thd->net.max_packet - length, file);
      length+= (ulong) strlen(buff + length);
      /* purecov: end */
    }
    if (bootstrap_error)
      break;                                    /* purecov: inspected */

    while (length && (my_isspace(thd->charset(), buff[length-1]) ||
                      buff[length-1] == ';'))
      length--;
    buff[length]=0;

    /* Skip lines starting with delimiter */
    if (strncmp(buff, STRING_WITH_LEN("delimiter")) == 0)
      continue;

    query= (char *) thd->memdup_w_gap(buff, length + 1,
                                      thd->db_length + 1 +
                                      QUERY_CACHE_FLAGS_SIZE);
    thd->set_query(query, length);
    DBUG_PRINT("query",("%-.4096s",thd->query));
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
    thd->profiling.start_new_query();
    thd->profiling.set_query_source(thd->query, length);
#endif

    /*
      We don't need to obtain LOCK_thread_count here because in bootstrap
      mode we have only one thread.
    */
    thd->query_id=next_query_id();
    thd->set_time();
    mysql_parse(thd, thd->query, length, & found_semicolon);
    close_thread_tables(thd);			// Free tables

    bootstrap_error= thd->is_error();
    net_end_statement(thd);

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
#ifdef USING_TRANSACTIONS
    free_root(&thd->transaction.mem_root,MYF(MY_KEEP_PREALLOC));
#endif
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

  /* The following must be called before DBUG_ENTER */
  thd->thread_stack= (char*) &thd;
  if (my_thread_init() || thd->store_globals())
  {
#ifndef EMBEDDED_LIBRARY
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
#endif
    thd->fatal_error();
    goto end;
  }

  handle_bootstrap_impl(thd);

end:
  net_end(&thd->net);
  thd->cleanup();
  delete thd;

#ifndef EMBEDDED_LIBRARY
  (void) pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  in_bootstrap= FALSE;
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
  pthread_exit(0);
#endif

  return 0;
}


/**
  @brief Check access privs for a MERGE table and fix children lock types.

  @param[in]        thd         thread handle
  @param[in]        db          database name
  @param[in,out]    table_list  list of child tables (merge_list)
                                lock_type and optionally db set per table

  @return           status
    @retval         0           OK
    @retval         != 0        Error

  @detail
    This function is used for write access to MERGE tables only
    (CREATE TABLE, ALTER TABLE ... UNION=(...)). Set TL_WRITE for
    every child. Set 'db' for every child if not present.
*/
#ifndef NO_EMBEDDED_ACCESS_CHECKS
static bool check_merge_table_access(THD *thd, char *db,
                                     TABLE_LIST *table_list)
{
  int error= 0;

  if (table_list)
  {
    /* Check that all tables use the current database */
    TABLE_LIST *tlist;

    for (tlist= table_list; tlist; tlist= tlist->next_local)
    {
      if (!tlist->db || !tlist->db[0])
        tlist->db= db; /* purecov: inspected */
    }
    error= check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
                              table_list, UINT_MAX, FALSE);
  }
  return error;
}
#endif

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

/* This works because items are allocated with sql_alloc() */

void cleanup_items(Item *item)
{
  DBUG_ENTER("cleanup_items");  
  for (; item ; item=item->next)
    item->cleanup();
  DBUG_VOID_RETURN;
}

/**
  Handle COM_TABLE_DUMP command.

  @param thd           thread handle
  @param db            database name or an empty string. If empty,
                       the current database of the connection is used
  @param tbl_name      name of the table to dump

  @note
    This function is written to handle one specific command only.

  @retval
    0               success
  @retval
    1               error, the error message is set in THD
*/

static
int mysql_table_dump(THD *thd, LEX_STRING *db, char *tbl_name)
{
  TABLE* table;
  TABLE_LIST* table_list;
  int error = 0;
  DBUG_ENTER("mysql_table_dump");
  if (db->length == 0)
  {
    db->str= thd->db;            /* purecov: inspected */
    db->length= thd->db_length;  /* purecov: inspected */
  }
  if (!(table_list = (TABLE_LIST*) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(1); // out of memory
  table_list->db= db->str;
  table_list->table_name= table_list->alias= tbl_name;
  table_list->lock_type= TL_READ_NO_INSERT;
  table_list->prev_global= &table_list;	// can be removed after merge with 4.1

  if (check_db_name(db))
  {
    /* purecov: begin inspected */
    my_error(ER_WRONG_DB_NAME ,MYF(0), db->str ? db->str : "NULL");
    goto err;
    /* purecov: end */
  }
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tbl_name);

  if (!(table=open_ltable(thd, table_list, TL_READ_NO_INSERT, 0)))
    DBUG_RETURN(1);

  if (check_one_table_access(thd, SELECT_ACL, table_list))
    goto err;
  thd->free_list = 0;
  thd->set_query(tbl_name, (uint) strlen(tbl_name));
  if ((error = mysqld_dump_create_info(thd, table_list, -1)))
  {
    my_error(ER_GET_ERRNO, MYF(0), my_errno);
    goto err;
  }
  net_flush(&thd->net);
  if ((error= table->file->dump(thd,-1)))
    my_error(ER_GET_ERRNO, MYF(0), error);

err:
  DBUG_RETURN(error);
}

/**
  Ends the current transaction and (maybe) begin the next.

  @param thd            Current thread
  @param completion     Completion type

  @retval
    0   OK
*/

int end_trans(THD *thd, enum enum_mysql_completiontype completion)
{
  bool do_release= 0;
  int res= 0;
  DBUG_ENTER("end_trans");

  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(1);
  }
  if (thd->transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
    DBUG_RETURN(1);
  }
  switch (completion) {
  case COMMIT:
    /*
     We don't use end_active_trans() here to ensure that this works
     even if there is a problem with the OPTION_AUTO_COMMIT flag
     (Which of course should never happen...)
    */
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    res= ha_commit(thd);
    thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
    thd->transaction.all.modified_non_trans_table= FALSE;
    break;
  case COMMIT_RELEASE:
    do_release= 1; /* fall through */
  case COMMIT_AND_CHAIN:
    res= end_active_trans(thd);
    if (!res && completion == COMMIT_AND_CHAIN)
      res= begin_trans(thd);
    break;
  case ROLLBACK_RELEASE:
    do_release= 1; /* fall through */
  case ROLLBACK:
  case ROLLBACK_AND_CHAIN:
  {
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_rollback(thd))
      res= -1;
    thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
    thd->transaction.all.modified_non_trans_table= FALSE;
    if (!res && (completion == ROLLBACK_AND_CHAIN))
      res= begin_trans(thd);
    break;
  }
  default:
    res= -1;
    my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
    DBUG_RETURN(-1);
  }

  if (res < 0)
    my_error(thd->killed_errno(), MYF(0));
  else if ((res == 0) && do_release)
    thd->killed= THD::KILL_CONNECTION;

  DBUG_RETURN(res);
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
    number of seconds has passed
  */
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  /*
    XXX: this code is here only to clear possible errors of init_connect. 
    Consider moving to init_connect() instead.
  */
  thd->clear_error();				// Clear error message
  thd->stmt_da->reset_diagnostics_area();

  net_new_transaction(net);

  packet_length= my_net_read(net);
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  thd->profiling.start_new_query();
#endif
  if (packet_length == packet_error)
  {
    DBUG_PRINT("info",("Got error %d reading command from socket %s",
		       net->error,
		       vio_description(net->vio)));

    /* Check if we can continue without closing the connection */

    /* The error must be set. */
    DBUG_ASSERT(thd->is_error());
    net_end_statement(thd);

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
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  thd->profiling.finish_current_query();
#endif
  DBUG_RETURN(return_value);
}
#endif  /* EMBEDDED_LIBRARY */

/**
  @brief Determine if an attempt to update a non-temporary table while the
    read-only option was enabled has been made.

  This is a helper function to mysql_execute_command.

  @note SQLCOM_MULTI_UPDATE is an exception and delt with elsewhere.

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

  const my_bool drop_temp_tables= 
    (lex->sql_command == SQLCOM_DROP_TABLE) &&
    lex->drop_temporary;

  const my_bool update_real_tables=
    some_non_temp_table_to_be_updated(thd, all_tables) &&
    !(create_temp_tables || drop_temp_tables);


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

  MYSQL_COMMAND_START(thd->thread_id, command,
                      thd->security_ctx->priv_user,
                      (char *) thd->security_ctx->host_or_ip);
  
  thd->command=command;
  /*
    Commands which always take a long time are logged into
    the slow log only if opt_log_slow_admin_statements is set.
  */
  thd->enable_slow_log= TRUE;
  thd->lex->sql_command= SQLCOM_END; /* to avoid confusing VIEW detectors */
  thd->set_time();
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id= global_query_id;

  switch( command ) {
  /* Ignore these statements. */
  case COM_STATISTICS:
  case COM_PING:
    break;
  /* Only increase id on these statements but don't count them. */
  case COM_STMT_PREPARE: 
  case COM_STMT_CLOSE:
  case COM_STMT_RESET:
    next_query_id();
    break;
  /* Increase id and count all other statements. */
  default:
    statistic_increment(thd->status_var.questions, &LOCK_status);
    next_query_id();
  }

  thread_running++;
  /* TODO: set thd->lex->sql_command to SQLCOM_END here */
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  /**
    Clear the set of flags that are expected to be cleared at the
    beginning of each command.
  */
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
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
  case COM_TABLE_DUMP:
  {
    char *tbl_name;
    LEX_STRING db;
    /* Safe because there is always a trailing \0 at the end of the packet */
    uint db_len= *(uchar*) packet;
    if (db_len + 1 > packet_length || db_len > NAME_LEN)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    /* Safe because there is always a trailing \0 at the end of the packet */
    uint tbl_len= *(uchar*) (packet + db_len + 1);
    if (db_len + tbl_len + 2 > packet_length || tbl_len > NAME_LEN)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }

    status_var_increment(thd->status_var.com_other);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    db.str= (char*) thd->alloc(db_len + tbl_len + 2);
    if (!db.str)
    {
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
      break;
    }
    db.length= db_len;
    tbl_name= strmake(db.str, packet + 1, db_len)+1;
    strmake(tbl_name, packet + db_len + 2, tbl_len);
    if (mysql_table_dump(thd, &db, tbl_name) == 0)
      thd->stmt_da->disable_status();
    break;
  }
  case COM_CHANGE_USER:
  {
    status_var_increment(thd->status_var.com_other);
    char *user= (char*) packet, *packet_end= packet + packet_length;
    /* Safe because there is always a trailing \0 at the end of the packet */
    char *passwd= strend(user)+1;

    thd->change_user();
    thd->clear_error();                         // if errors from rollback

    /*
      Old clients send null-terminated string ('\0' for empty string) for
      password.  New clients send the size (1 byte) + string (not null
      terminated, so also '\0' for empty string).

      Cast *passwd to an unsigned char, so that it doesn't extend the sign
      for *passwd > 127 and become 2**32-127 after casting to uint.
    */
    char db_buff[NAME_LEN+1];                 // buffer to store db in utf8
    char *db= passwd;
    char *save_db;
    /*
      If there is no password supplied, the packet must contain '\0',
      in any type of handshake (4.1 or pre-4.1).
     */
    if (passwd >= packet_end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    uint passwd_len= (thd->client_capabilities & CLIENT_SECURE_CONNECTION ?
                      (uchar)(*passwd++) : strlen(passwd));
    uint dummy_errors, save_db_length, db_length;
    int res;
    Security_context save_security_ctx= *thd->security_ctx;
    USER_CONN *save_user_connect;

    db+= passwd_len + 1;
    /*
      Database name is always NUL-terminated, so in case of empty database
      the packet must contain at least the trailing '\0'.
    */
    if (db >= packet_end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    db_length= strlen(db);

    char *ptr= db + db_length + 1;
    uint cs_number= 0;

    if (ptr < packet_end)
    {
      if (ptr + 2 > packet_end)
      {
        my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
        break;
      }

      cs_number= uint2korr(ptr);
    }

    /* Convert database name to utf8 */
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info, db, db_length,
                             thd->charset(), &dummy_errors)]= 0;
    db= db_buff;

    /* Save user and privileges */
    save_db_length= thd->db_length;
    save_db= thd->db;
    save_user_connect= thd->user_connect;

    if (!(thd->security_ctx->user= my_strdup(user, MYF(0))))
    {
      thd->security_ctx->user= save_security_ctx.user;
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
      break;
    }

    /* Clear variables that are allocated */
    thd->user_connect= 0;
    thd->security_ctx->priv_user= thd->security_ctx->user;
    res= check_user(thd, COM_CHANGE_USER, passwd, passwd_len, db, FALSE);

    if (res)
    {
      x_free(thd->security_ctx->user);
      *thd->security_ctx= save_security_ctx;
      thd->user_connect= save_user_connect;
      thd->db= save_db;
      thd->db_length= save_db_length;
    }
    else
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* we've authenticated new user */
      if (save_user_connect)
	decrease_user_connections(save_user_connect);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
      x_free(save_db);
      x_free(save_security_ctx.user);

      if (cs_number)
      {
        thd_init_client_charset(thd, cs_number);
        thd->update_charset();
      }
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
    MYSQL_QUERY_START(thd->query, thd->thread_id,
                      (char *) (thd->db ? thd->db : ""),
                      thd->security_ctx->priv_user,
                      (char *) thd->security_ctx->host_or_ip);
    char *packet_end= thd->query + thd->query_length;
    /* 'b' stands for 'buffer' parameter', special for 'my_snprintf' */
    const char* end_of_stmt= NULL;

    general_log_write(thd, command, thd->query, thd->query_length);
    DBUG_PRINT("query",("%-.4096s",thd->query));
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
    thd->profiling.set_query_source(thd->query, thd->query_length);
#endif

    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),QUERY_PRIOR);

    mysql_parse(thd, thd->query, thd->query_length, &end_of_stmt);

    while (!thd->killed && (end_of_stmt != NULL) && ! thd->is_error())
    {
      char *beginning_of_next_stmt= (char*) end_of_stmt;

      net_end_statement(thd);
      query_cache_end_of_result(thd);
      /*
        Multiple queries exits, execute them individually
      */
      close_thread_tables(thd);
      ulong length= (ulong)(packet_end - beginning_of_next_stmt);

      log_slow_statement(thd);

      /* Remove garbage at start of query */
      while (length > 0 && my_isspace(thd->charset(), *beginning_of_next_stmt))
      {
        beginning_of_next_stmt++;
        length--;
      }

      if (MYSQL_QUERY_DONE_ENABLED())
      {
        MYSQL_QUERY_DONE(thd->is_error());
      }

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
      thd->profiling.finish_current_query();
      thd->profiling.start_new_query("continuing");
      thd->profiling.set_query_source(beginning_of_next_stmt, length);
#endif

      MYSQL_QUERY_START(beginning_of_next_stmt, thd->thread_id,
                        (char *) (thd->db ? thd->db : ""),
                        thd->security_ctx->priv_user,
                        (char *) thd->security_ctx->host_or_ip);

      thd->set_query(beginning_of_next_stmt, length);
      VOID(pthread_mutex_lock(&LOCK_thread_count));
      /*
        Count each statement from the client.
      */
      statistic_increment(thd->status_var.questions, &LOCK_status);
      thd->query_id= next_query_id();
      thd->set_time(); /* Reset the query start time. */
      /* TODO: set thd->lex->sql_command to SQLCOM_END here */
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
      mysql_parse(thd, beginning_of_next_stmt, length, &end_of_stmt);
    }

    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),WAIT_PRIOR);
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
    LEX_STRING conv_name;

    /* used as fields initializator */
    lex_start(thd);

    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_FIELDS]);
    bzero((char*) &table_list,sizeof(table_list));
    if (thd->copy_db_to(&table_list.db, &table_list.db_length))
      break;
    /*
      We have name + wildcard in packet, separated by endzero
    */
    arg_end= strend(packet);
    thd->convert_string(&conv_name, system_charset_info,
			packet, (uint) (arg_end - packet), thd->charset());
    table_list.alias= table_list.table_name= conv_name.str;
    packet= arg_end + 1;

    if (!my_strcasecmp(system_charset_info, table_list.db,
                       INFORMATION_SCHEMA_NAME.str))
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
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, table_list.table_name);

    if (check_access(thd,SELECT_ACL,table_list.db,&table_list.grant.privilege,
		     0, 0, test(table_list.schema_table)))
      break;
    if (check_grant(thd, SELECT_ACL, &table_list, 2, UINT_MAX, 0))
      break;
    /* init structures for VIEW processing */
    table_list.select_lex= &(thd->lex->select_lex);

    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    thd->lex->
      select_lex.table_list.link_in_list((uchar*) &table_list,
                                         (uchar**) &table_list.next_local);
    thd->lex->add_to_query_tables(&table_list);

    /* switch on VIEW optimisation: do not fill temporary tables */
    thd->lex->sql_command= SQLCOM_SHOW_FIELDS;
    mysqld_list_fields(thd,&table_list,fields);
    thd->lex->unit.cleanup();
    thd->cleanup_after_query();
    break;
  }
#endif
  case COM_QUIT:
    /* We don't calculate statistics for this command */
    general_log_print(thd, command, NullS);
    net->error=0;				// Don't give 'abort' message
    thd->stmt_da->disable_status();              // Don't send anything back
    error=TRUE;					// End server
    break;

#ifdef REMOVED
  case COM_CREATE_DB:				// QQ: To be removed
    {
      LEX_STRING db, alias;
      HA_CREATE_INFO create_info;

      status_var_increment(thd->status_var.com_stat[SQLCOM_CREATE_DB]);
      if (thd->make_lex_string(&db, packet, packet_length, FALSE) ||
          thd->make_lex_string(&alias, db.str, db.length, FALSE) ||
          check_db_name(&db))
      {
	my_error(ER_WRONG_DB_NAME, MYF(0), db.str ? db.str : "NULL");
	break;
      }
      if (check_access(thd, CREATE_ACL, db.str , 0, 1, 0,
                       is_schema_db(db.str)))
	break;
      general_log_print(thd, command, "%.*s", db.length, db.str);
      bzero(&create_info, sizeof(create_info));
      mysql_create_db(thd, (lower_case_table_names == 2 ? alias.str : db.str),
                      &create_info, 0);
      break;
    }
  case COM_DROP_DB:				// QQ: To be removed
    {
      status_var_increment(thd->status_var.com_stat[SQLCOM_DROP_DB]);
      LEX_STRING db;

      if (thd->make_lex_string(&db, packet, packet_length, FALSE) ||
          check_db_name(&db))
      {
	my_error(ER_WRONG_DB_NAME, MYF(0), db.str ? db.str : "NULL");
	break;
      }
      if (check_access(thd, DROP_ACL, db.str, 0, 1, 0, is_schema_db(db.str)))
	break;
      if (thd->locked_tables || thd->active_transaction())
      {
	my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                   ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
	break;
      }
      general_log_write(thd, command, "%.*s", db.length, db.str);
      mysql_rm_db(thd, db.str, 0, 0);
      break;
    }
#endif
#ifndef EMBEDDED_LIBRARY
  case COM_BINLOG_DUMP:
    {
      ulong pos;
      ushort flags;
      uint32 slave_server_id;

      status_var_increment(thd->status_var.com_other);
      thd->enable_slow_log= opt_log_slow_admin_statements;
      if (check_global_access(thd, REPL_SLAVE_ACL))
	break;

      /* TODO: The following has to be changed to an 8 byte integer */
      pos = uint4korr(packet);
      flags = uint2korr(packet + 4);
      thd->server_id=0; /* avoid suicide */
      if ((slave_server_id= uint4korr(packet+6))) // mysqlbinlog.server_id==0
	kill_zombie_dump_threads(slave_server_id);
      thd->server_id = slave_server_id;

      general_log_print(thd, command, "Log: '%s'  Pos: %ld", packet+10,
                      (long) pos);
      mysql_binlog_send(thd, thd->strdup(packet + 10), (my_off_t) pos, flags);
      unregister_slave(thd,1,1);
      /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
      error = TRUE;
      break;
    }
#endif
  case COM_REFRESH:
  {
    bool not_used;
    status_var_increment(thd->status_var.com_stat[SQLCOM_FLUSH]);
    ulong options= (ulong) (uchar) packet[0];
    if (check_global_access(thd,RELOAD_ACL))
      break;
    general_log_print(thd, command, NullS);
    if (!reload_acl_and_cache(thd, options, (TABLE_LIST*) 0, &not_used))
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
    enum mysql_enum_shutdown_level level=
      (enum mysql_enum_shutdown_level) (uchar) packet[0];
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
    close_thread_tables(thd);			// Free before kill
    kill_mysql();
    error=TRUE;
    break;
  }
#endif
  case COM_STATISTICS:
  {
    STATUS_VAR current_global_status_var;
    ulong uptime;
    uint length;
    ulonglong queries_per_second1000;
    char buff[250];
    uint buff_len= sizeof(buff);

    general_log_print(thd, command, NullS);
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_STATUS]);
    calc_sum_of_all_status(&current_global_status_var);
    if (!(uptime= (ulong) (thd->start_time - server_start_time)))
      queries_per_second1000= 0;
    else
      queries_per_second1000= thd->query_id * LL(1000) / uptime;

    length= my_snprintf((char*) buff, buff_len - 1,
                        "Uptime: %lu  Threads: %d  Questions: %lu  "
                        "Slow queries: %lu  Opens: %lu  Flush tables: %lu  "
                        "Open tables: %u  Queries per second avg: %u.%u",
                        uptime,
                        (int) thread_count, (ulong) thd->query_id,
                        current_global_status_var.long_query_count,
                        current_global_status_var.opened_tables,
                        refresh_version,
                        cached_open_tables(),
                        (uint) (queries_per_second1000 / 1000),
                        (uint) (queries_per_second1000 % 1000));
#ifdef EMBEDDED_LIBRARY
    /* Store the buffer in permanent memory */
    my_ok(thd, 0, 0, buff);
#endif
#ifdef SAFEMALLOC
    if (sf_malloc_cur_memory)				// Using SAFEMALLOC
    {
      char *end= buff + length;
      length+= my_snprintf(end, buff_len - length - 1,
                           end,"  Memory in use: %ldK  Max memory used: %ldK",
                           (sf_malloc_cur_memory+1023L)/1024L,
                           (sf_malloc_max_memory+1023L)/1024L);
    }
#endif
#ifndef EMBEDDED_LIBRARY
    VOID(my_net_write(net, (uchar*) buff, length));
    VOID(net_flush(net));
    thd->stmt_da->disable_status();
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
    status_var_increment(thd->status_var.com_stat[SQLCOM_KILL]);
    ulong id=(ulong) uint4korr(packet);
    sql_kill(thd,id,false);
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
  case COM_DELAYED_INSERT:
  case COM_END:
  default:
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    break;
  }

  /* report error issued during command execution */
  if (thd->killed_errno())
  {
    if (! thd->stmt_da->is_set())
      thd->send_kill_message();
  }
  if (thd->killed == THD::KILL_QUERY || thd->killed == THD::KILL_BAD_DATA)
  {
    thd->killed= THD::NOT_KILLED;
    thd->mysys_var->abort= 0;
  }

  /* If commit fails, we should be able to reset the OK status. */
  thd->stmt_da->can_overwrite_status= TRUE;
  ha_autocommit_or_rollback(thd, thd->is_error());
  thd->stmt_da->can_overwrite_status= FALSE;

  thd->transaction.stmt.reset();

  net_end_statement(thd);
  query_cache_end_of_result(thd);

  thd->proc_info= "closing tables";
  /* Free tables */
  close_thread_tables(thd);

  log_slow_statement(thd);

  thd_proc_info(thd, "cleaning up");
  thd->set_query(NULL, 0);
  thd->command=COM_SLEEP;
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
  thread_running--;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd_proc_info(thd, 0);
  thd->packet.shrink(thd->variables.net_buffer_length);	// Reclaim some memory
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));

  if (MYSQL_QUERY_DONE_ENABLED() || MYSQL_COMMAND_DONE_ENABLED())
  {
    int res;
    res= (int) thd->is_error();
    if (command == COM_QUERY)
    {
      MYSQL_QUERY_DONE(res);
    }
    MYSQL_COMMAND_DONE(res);
  }
  DBUG_RETURN(error);
}


void log_slow_statement(THD *thd)
{
  DBUG_ENTER("log_slow_statement");

  /*
    The following should never be true with our current code base,
    but better to keep this here so we don't accidently try to log a
    statement in a trigger or stored function
  */
  if (unlikely(thd->in_sub_stmt))
    DBUG_VOID_RETURN;                           // Don't set time for sub stmt

  /*
    Do not log administrative statements unless the appropriate option is
    set; do not log into slow log if reading from backup.
  */
  if (thd->enable_slow_log && !thd->user_time)
  {
    ulonglong end_utime_of_query= thd->current_utime();
    thd_proc_info(thd, "logging slow query");

    if (((end_utime_of_query - thd->utime_after_lock) >
         thd->variables.long_query_time ||
         ((thd->server_status &
           (SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
          opt_log_queries_not_using_indexes &&
           !(sql_command_flags[thd->lex->sql_command] & CF_STATUS_COMMAND))) &&
        thd->examined_row_count >= thd->variables.min_examined_row_limit)
    {
      thd_proc_info(thd, "logging slow query");
      thd->status_var.long_query_count++;
      slow_log_print(thd, thd->query, thd->query_length, end_utime_of_query);
    }
  }
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

      if (check_db_name(&db))
      {
        my_error(ER_WRONG_DB_NAME, MYF(0), db.str);
        DBUG_RETURN(1);
      }
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
    if (!schema_select_lex->add_table_to_list(thd, table_ident, 0, 0, TL_READ))
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
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
    thd->profiling.discard_current_query();
#endif
    break;
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
  TABLE_LIST *table_list= (TABLE_LIST*) select_lex->table_list.first;
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
  /* We must allocate some extra memory for query cache */
  if (! (query= (char*) thd->memdup_w_gap(packet,
                                          packet_length,
                                          1 + thd->db_length +
                                          QUERY_CACHE_FLAGS_SIZE)))
      return TRUE;
  query[packet_length]= '\0';
  thd->set_query(query, packet_length);

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
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  DBUG_RETURN(FALSE);
}


/**
  Execute command saved in thd and lex->sql_command.

    Before every operation that can request a write lock for a table
    wait if a global read lock exists. However do not wait if this
    thread has locked tables already. No new locks can be requested
    until the other locks are released. The thread that requests the
    global read lock waits for write locked tables to become unlocked.

    Note that wait_if_global_read_lock() sets a protection against a new
    global read lock when it succeeds. This needs to be released by
    start_waiting_global_read_lock() after the operation.

  @param thd                       Thread handle

  @todo
    - Invalidate the table in the query cache if something changed
    after unlocking when changes become visible.
    TODO: this is workaround. right way will be move invalidating in
    the unlock procedure.
    - TODO: use check_change_password()
    - JOIN is not supported yet. TODO
    - SUSPEND and FOR MIGRATE are not supported yet. TODO

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/

int
mysql_execute_command(THD *thd)
{
  int res= FALSE;
  bool need_start_waiting= FALSE; // have protection against global read lock
  int  up_result= 0;
  LEX  *lex= thd->lex;
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= &lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *first_table= (TABLE_LIST*) select_lex->table_list.first;
  /* list of all tables in query */
  TABLE_LIST *all_tables;
  /* most outer SELECT_LEX_UNIT of query */
  SELECT_LEX_UNIT *unit= &lex->unit;
#ifdef HAVE_REPLICATION
  /* have table map for update for multi-update statement (BUG#37051) */
  bool have_table_map_for_update= FALSE;
#endif
  /* Saved variable value */
  DBUG_ENTER("mysql_execute_command");
#ifdef WITH_PARTITION_STORAGE_ENGINE
  thd->work_part_info= 0;
#endif

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
    context.resolve_in_table_list_only((TABLE_LIST*)select_lex->
                                       table_list.first);

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
    Don't reset warnings when executing a stored routine.
  */
  if ((sql_command_flags[lex->sql_command] & CF_DIAGNOSTIC_STMT) != 0)
    thd->warning_info->set_read_only(TRUE);
  else
  {
    thd->warning_info->set_read_only(FALSE);
    if (all_tables)
      thd->warning_info->opt_clear_warning_info(thd->query_id);
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

  DBUG_ASSERT(thd->transaction.stmt.modified_non_trans_table == FALSE);
  
  switch (lex->sql_command) {

  case SQLCOM_SHOW_EVENTS:
#ifndef HAVE_EVENT_SCHEDULER
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "embedded server");
    break;
#endif
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STATUS_FUNC:
    if (!(res= check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE)))
      res= execute_sqlcom_select(thd, all_tables);
    break;
  case SQLCOM_SHOW_STATUS:
  {
    system_status_var old_status_var= thd->status_var;
    thd->initial_status_var= &old_status_var;
    if (!(res= check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE)))
      res= execute_sqlcom_select(thd, all_tables);
    /* Don't log SHOW STATUS commands to slow query log */
    thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED);
    /*
      restore status variables, as we don't want 'show status' to cause
      changes
    */
    pthread_mutex_lock(&LOCK_status);
    add_diff_to_status(&global_status_var, &thd->status_var,
                       &old_status_var);
    thd->status_var= old_status_var;
    pthread_mutex_unlock(&LOCK_status);
    break;
  }
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
    thd->status_var.last_query_cost= 0.0;
    if (all_tables)
    {
      res= check_table_access(thd,
                              lex->exchange ? SELECT_ACL | FILE_ACL :
                              SELECT_ACL,
                              all_tables, UINT_MAX, FALSE);
    }
    else
      res= check_access(thd,
                        lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL,
                        any_db, 0, 0, 0, 0);

    if (res)
      break;

    if (!thd->locked_tables && lex->protect_against_global_read_lock &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      break;

    res= execute_sqlcom_select(thd, all_tables);
    break;
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
    if (check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE) ||
        open_and_lock_tables(thd, all_tables))
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
    res = purge_master_logs_before_date(thd, (ulong)it->val_int());
    break;
  }
#endif
  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      ((1L << (uint) MYSQL_ERROR::WARN_LEVEL_NOTE) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_WARN) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR));
    break;
  }
  case SQLCOM_SHOW_PROFILES:
  {
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
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
  case SQLCOM_SHOW_NEW_MASTER:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    /* This query don't work now. See comment in repl_failsafe.cc */
#ifndef WORKING_NEW_MASTER
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "SHOW NEW MASTER");
    goto error;
#else
    res = show_new_master(thd);
    break;
#endif
  }

#ifdef HAVE_REPLICATION
  case SQLCOM_SHOW_SLAVE_HOSTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = show_slave_hosts(thd);
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

  case SQLCOM_BACKUP_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE) ||
	check_global_access(thd, FILE_ACL))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_backup_table(thd, first_table);
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_RESTORE_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, INSERT_ACL, all_tables, UINT_MAX, FALSE) ||
	check_global_access(thd, FILE_ACL))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_restore_table(thd, first_table);
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege, 0, 0,
                     test(first_table->schema_table)))
      goto error;
    res= mysql_assign_to_keycache(thd, first_table, &lex->ident);
    break;
  }
  case SQLCOM_PRELOAD_KEYS:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege, 0, 0,
                     test(first_table->schema_table)))
      goto error;
    res = mysql_preload_keys(thd, first_table);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_CHANGE_MASTER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    pthread_mutex_lock(&LOCK_active_mi);
    res = change_master(thd,active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    pthread_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
    {
      res = show_master_info(thd, active_mi);
    }
    else
    {
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                   WARN_NO_MASTER_INFO, ER(WARN_NO_MASTER_INFO));
      my_ok(thd);
    }
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_MASTER_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    res = show_binlog_info(thd);
    break;
  }

  case SQLCOM_LOAD_MASTER_DATA: // sync with master
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    if (end_active_trans(thd))
      goto error;
    res = load_master_data(thd);
    break;
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
#ifdef HAVE_REPLICATION
  case SQLCOM_LOAD_MASTER_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    DBUG_ASSERT(first_table->db); /* Must be set in the parser */

    if (check_access(thd, CREATE_ACL, first_table->db,
		     &first_table->grant.privilege, 0, 0,
                     test(first_table->schema_table)))
      goto error;				/* purecov: inspected */
    /* Check that the first table has CREATE privilege */
    if (check_grant(thd, CREATE_ACL, all_tables, 0, 1, 0))
      goto error;

    pthread_mutex_lock(&LOCK_active_mi);
    /*
      fetch_master_table will send the error to the client on failure.
      Give error if the table already exists.
    */
    if (!fetch_master_table(thd, first_table->db, first_table->table_name,
			    active_mi, 0, 0))
    {
      my_ok(thd);
    }
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif /* HAVE_REPLICATION */

  case SQLCOM_CREATE_TABLE:
  {
    /* If CREATE TABLE of non-temporary table, do implicit commit */
    if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (end_active_trans(thd))
      {
	res= -1;
	break;
      }
    }
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    bool link_to_local;
    // Skip first table, which is the table we are creating
    TABLE_LIST *create_table= lex->unlink_first_table(&link_to_local);
    TABLE_LIST *select_tables= lex->query_tables;
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

#ifdef HAVE_READLINK
    /* Fix names if symlinked tables */
    if (append_file_to_dir(thd, &create_info.data_file_name,
			   create_table->table_name) ||
	append_file_to_dir(thd, &create_info.index_file_name,
			   create_table->table_name))
      goto end_with_restore_list;
#endif
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
    /*
      The create-select command will open and read-lock the select table
      and then create, open and write-lock the new table. If a global
      read lock steps in, we get a deadlock. The write lock waits for
      the global read lock, while the global read lock waits for the
      select table to be closed. So we wait until the global readlock is
      gone before starting both steps. Note that
      wait_if_global_read_lock() sets a protection against a new global
      read lock when it succeeds. This needs to be released by
      start_waiting_global_read_lock(). We protect the normal CREATE
      TABLE in the same way. That way we avoid that a new table is
      created during a gobal read lock.
    */
    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      goto end_with_restore_list;
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
                       MYSQL_ERROR::WARN_LEVEL_WARN,
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

      if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
      {
        lex->link_first_table_back(create_table, link_to_local);
        create_table->create= TRUE;
      }

      if (!(res= open_and_lock_tables(thd, lex->query_tables)))
      {
        /*
          Is table which we are changing used somewhere in other parts
          of query
        */
        if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
        {
          TABLE_LIST *duplicate;
          create_table= lex->unlink_first_table(&link_to_local);
          if ((duplicate= unique_table(thd, create_table, select_tables, 0)))
          {
            update_non_unique_table_error(create_table, "CREATE", duplicate);
            res= 1;
            goto end_with_restore_list;
          }
        }
        /* If we create merge table, we have to test tables in merge, too */
        if (create_info.used_fields & HA_CREATE_USED_UNION)
        {
          TABLE_LIST *tab;
          for (tab= (TABLE_LIST*) create_info.merge_list.first;
               tab;
               tab= tab->next_local)
          {
            TABLE_LIST *duplicate;
            if ((duplicate= unique_table(thd, tab, select_tables, 0)))
            {
              update_non_unique_table_error(tab, "CREATE", duplicate);
              res= 1;
              goto end_with_restore_list;
            }
          }
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
          res= handle_select(thd, lex, result, 0);
          delete result;
        }
      }
      else if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
        create_table= lex->unlink_first_table(&link_to_local);

    }
    else
    {
      /* So that CREATE TEMPORARY TABLE gets to binlog at commit/rollback */
      if (create_info.options & HA_LEX_CREATE_TMP_TABLE)
        thd->options|= OPTION_KEEP_LOG;
      /* regular create */
      if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
        res= mysql_create_like_table(thd, create_table, select_tables,
                                     &create_info);
      else
      {
        res= mysql_create_table(thd, create_table->db,
                                create_table->table_name, &create_info,
                                &alter_info, 0, 0);
      }
      if (!res)
	my_ok(thd);
    }

    /* put tables back for PS rexecuting */
end_with_restore_list:
    lex->link_first_table_back(create_table, link_to_local);
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
    if (end_active_trans(thd))
      goto error;
    /*
      Currently CREATE INDEX or DROP INDEX cause a full table rebuild
      and thus classify as slow administrative statements just like
      ALTER TABLE.
    */
    thd->enable_slow_log= opt_log_slow_admin_statements;

    bzero((char*) &create_info, sizeof(create_info));
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
    pthread_mutex_lock(&LOCK_active_mi);
    start_slave(thd,active_mi,1 /* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
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
  if (thd->locked_tables || thd->active_transaction() || thd->global_read_lock)
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    goto error;
  }
  {
    pthread_mutex_lock(&LOCK_active_mi);
    stop_slave(thd,active_mi,1/* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif /* HAVE_REPLICATION */

  case SQLCOM_ALTER_TABLE:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    {
      ulong priv=0;
      ulong priv_needed= ALTER_ACL;
      /*
        Code in mysql_alter_table() may modify its HA_CREATE_INFO argument,
        so we have to use a copy of this structure to make execution
        prepared statement- safe. A shallow copy is enough as no memory
        referenced from this structure will be modified.
      */
      HA_CREATE_INFO create_info(lex->create_info);
      Alter_info alter_info(lex->alter_info, thd->mem_root);

      if (thd->is_fatal_error) /* out of memory creating a copy of alter_info */
        goto error;
      /*
        We also require DROP priv for ALTER TABLE ... DROP PARTITION, as well
        as for RENAME TO, as being done by SQLCOM_RENAME_TABLE
      */
      if (alter_info.flags & (ALTER_DROP_PARTITION | ALTER_RENAME))
        priv_needed|= DROP_ACL;

      /* Must be set in the parser */
      DBUG_ASSERT(select_lex->db);
      if (check_access(thd, priv_needed, first_table->db,
		       &first_table->grant.privilege, 0, 0,
                       test(first_table->schema_table)) ||
	  check_access(thd,INSERT_ACL | CREATE_ACL,select_lex->db,&priv,0,0,
                       is_schema_db(select_lex->db))||
	  check_merge_table_access(thd, first_table->db,
				   (TABLE_LIST *)
				   create_info.merge_list.first))
	goto error;				/* purecov: inspected */
      if (check_grant(thd, priv_needed, all_tables, 0, UINT_MAX, 0))
        goto error;
      if (lex->name.str && !test_all_bits(priv,INSERT_ACL | CREATE_ACL))
      { // Rename of table
          TABLE_LIST tmp_table;
          bzero((char*) &tmp_table,sizeof(tmp_table));
          tmp_table.table_name= lex->name.str;
          tmp_table.db=select_lex->db;
          tmp_table.grant.privilege=priv;
          if (check_grant(thd, INSERT_ACL | CREATE_ACL, &tmp_table, 0,
              UINT_MAX, 0))
            goto error;
      }

      /* Don't yet allow changing of symlinks with ALTER TABLE */
      if (create_info.data_file_name)
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                            "DATA DIRECTORY");
      if (create_info.index_file_name)
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                            "INDEX DIRECTORY");
      create_info.data_file_name= create_info.index_file_name= NULL;
      /* ALTER TABLE ends previous transaction */
      if (end_active_trans(thd))
	goto error;

      if (!thd->locked_tables &&
          !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      {
        res= 1;
        break;
      }

      thd->enable_slow_log= opt_log_slow_admin_statements;
      res= mysql_alter_table(thd, select_lex->db, lex->name.str,
                             &create_info,
                             first_table,
                             &alter_info,
                             select_lex->order_list.elements,
                             (ORDER *) select_lex->order_list.first,
                             lex->ignore);
      break;
    }
  case SQLCOM_RENAME_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *table;
    for (table= first_table; table; table= table->next_local->next_local)
    {
      if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
		       &table->grant.privilege,0,0, test(table->schema_table)) ||
	  check_access(thd, INSERT_ACL | CREATE_ACL, table->next_local->db,
		       &table->next_local->grant.privilege, 0, 0,
                       test(table->next_local->schema_table)))
	goto error;
      TABLE_LIST old_list, new_list;
      /*
        we do not need initialize old_list and new_list because we will
        come table[0] and table->next[0] there
      */
      old_list= table[0];
      new_list= table->next_local[0];
      if (check_grant(thd, ALTER_ACL | DROP_ACL, &old_list, 0, 1, 0) ||
         (!test_all_bits(table->next_local->grant.privilege,
                         INSERT_ACL | CREATE_ACL) &&
          check_grant(thd, INSERT_ACL | CREATE_ACL, &new_list, 0, 1, 0)))
        goto error;
    }

    if (end_active_trans(thd) || mysql_rename_tables(thd, first_table, 0))
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
      if (check_global_access(thd, SUPER_ACL))
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
      /* Ignore temporary tables if this is "SHOW CREATE VIEW" */
      if (lex->only_view)
        first_table->skip_temporary= 1;
      if (check_show_create_table_access(thd, first_table))
	goto error;
      res= mysqld_show_create(thd, first_table);
      break;
    }
#endif
  case SQLCOM_CHECKSUM:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL | EXTRA_ACL, all_tables,
                           UINT_MAX, FALSE))
      goto error; /* purecov: inspected */
    res = mysql_checksum_table(thd, first_table, &lex->check_opt);
    break;
  }
  case SQLCOM_REPAIR:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL | INSERT_ACL, all_tables,
                           UINT_MAX, FALSE))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= mysql_repair_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, REPAIR and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_CHECK:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL | EXTRA_ACL , all_tables,
                           UINT_MAX, FALSE))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_check_table(thd, first_table, &lex->check_opt);
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_ANALYZE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL | INSERT_ACL, all_tables,
                           UINT_MAX, FALSE))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= mysql_analyze_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, ANALYZE and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }

  case SQLCOM_OPTIMIZE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL | INSERT_ACL, all_tables,
                           UINT_MAX, FALSE))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= (specialflag & (SPECIAL_SAFE_MODE | SPECIAL_NO_NEW_FUNC)) ?
      mysql_recreate_table(thd, first_table) :
      mysql_optimize_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, OPTIMIZE and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_UPDATE:
  {
    ha_rows found= 0, updated= 0;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (update_precheck(thd, all_tables))
      break;
    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      goto error;
    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);
    MYSQL_UPDATE_START(thd->query);
    res= (up_result= mysql_update(thd, all_tables,
                                  select_lex->item_list,
                                  lex->value_list,
                                  select_lex->where,
                                  select_lex->order_list.elements,
                                  (ORDER *) select_lex->order_list.first,
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

    /*
      Protection might have already been risen if its a fall through
      from the SQLCOM_UPDATE case above.
    */
    if (!thd->locked_tables &&
        lex->sql_command == SQLCOM_UPDATE_MULTI &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      goto error;

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
      MYSQL_MULTI_UPDATE_START(thd->query);
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
        mysql_bin_log.write(&ev);
        mysql_bin_log.rotate_and_purge(RP_FORCE_ROTATE);
      }
      DBUG_PRINT("debug", ("Just after generate_incident()"));
    }
#endif
  case SQLCOM_INSERT:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }
    MYSQL_INSERT_START(thd->query);
    res= mysql_insert(thd, all_tables, lex->field_list, lex->many_values,
		      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);
    MYSQL_INSERT_DONE(res, (ulong) thd->row_count_func);
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
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    select_result *sel_result;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;

    /* Fix lock for first table */
    if (first_table->lock_type == TL_WRITE_DELAYED)
      first_table->lock_type= TL_WRITE;

    /* Don't unlock tables until command is written to binary log */
    select_lex->options|= SELECT_NO_UNLOCK;

    unit->set_limit(select_lex);

    if (! thd->locked_tables &&
        ! (need_start_waiting= ! wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }
    if (!(res= open_and_lock_tables(thd, all_tables)))
    {
      MYSQL_INSERT_SELECT_START(thd->query);
      /* Skip first table, which is the table we are inserting in */
      TABLE_LIST *second_table= first_table->next_local;
      select_lex->table_list.first= (uchar*) second_table;
      select_lex->context.table_list= 
        select_lex->context.first_name_resolution_table= second_table;
      res= mysql_insert_select_prepare(thd);
      if (!res && (sel_result= new select_insert(first_table,
                                                 first_table->table,
                                                 &lex->field_list,
                                                 &lex->update_list,
                                                 &lex->value_list,
                                                 lex->duplicates,
                                                 lex->ignore)))
      {
	res= handle_select(thd, lex, sel_result, OPTION_SETUP_TABLES_DONE);
        /*
          Invalidate the table in the query cache if something changed
          after unlocking when changes become visible.
          TODO: this is workaround. right way will be move invalidating in
          the unlock procedure.
        */
        if (first_table->lock_type ==  TL_WRITE_CONCURRENT_INSERT &&
            thd->lock)
        {
          /* INSERT ... SELECT should invalidate only the very first table */
          TABLE_LIST *save_table= first_table->next_local;
          first_table->next_local= 0;
          query_cache_invalidate3(thd, first_table, 1);
          first_table->next_local= save_table;
        }
        delete sel_result;
      }
      /* revert changes for SP */
      MYSQL_INSERT_SELECT_DONE(res, (ulong) thd->row_count_func);
      select_lex->table_list.first= (uchar*) first_table;
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
  case SQLCOM_TRUNCATE:
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_one_table_access(thd, DROP_ACL, all_tables))
      goto error;
    /*
      Don't allow this within a transaction because we want to use
      re-generate table
    */
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    if (!(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      goto error;
    res= mysql_truncate(thd, first_table, 0);
    break;
  case SQLCOM_DELETE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= delete_precheck(thd, all_tables)))
      break;
    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }
    MYSQL_DELETE_START(thd->query);
    res = mysql_delete(thd, all_tables, select_lex->where,
                       &select_lex->order_list,
                       unit->select_limit_cnt, select_lex->options,
                       FALSE);
    MYSQL_DELETE_DONE(res, (ulong) thd->row_count_func);
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *aux_tables=
      (TABLE_LIST *)thd->lex->auxiliary_table_list.first;
    multi_delete *del_result;

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }

    if ((res= multi_delete_precheck(thd, all_tables)))
      break;

    /* condition will be TRUE on SP re-excuting */
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (add_item_to_list(thd, new Item_null()))
      goto error;

    thd_proc_info(thd, "init");
    if ((res= open_and_lock_tables(thd, all_tables)))
      break;

    MYSQL_MULTI_DELETE_START(thd->query);
    if ((res= mysql_multi_delete_prepare(thd)))
    {
      MYSQL_MULTI_DELETE_DONE(1, 0);
      goto error;
    }

    if (!thd->is_fatal_error &&
        (del_result= new multi_delete(aux_tables, lex->table_count)))
    {
      res= mysql_select(thd, &select_lex->ref_pointer_array,
			select_lex->get_table_list(),
			select_lex->with_wild,
			select_lex->item_list,
			select_lex->where,
			0, (ORDER *)NULL, (ORDER *)NULL, (Item *)NULL,
			(ORDER *)NULL,
			select_lex->options | thd->options |
			SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                        OPTION_SETUP_TABLES_DONE,
			del_result, unit, select_lex);
      res|= thd->is_error();
      MYSQL_MULTI_DELETE_DONE(res, del_result->num_deleted());
      if (res)
        del_result->abort();
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
      if (check_table_access(thd, DROP_ACL, all_tables, UINT_MAX, FALSE))
	goto error;				/* purecov: inspected */
      if (end_active_trans(thd))
        goto error;
    }
    else
    {
      /*
	If this is a slave thread, we may sometimes execute some 
	DROP / * 40005 TEMPORARY * / TABLE
	that come from parts of binlogs (likely if we use RESET SLAVE or CHANGE
	MASTER TO), while the temporary table has already been dropped.
	To not generate such irrelevant "table does not exist errors",
	we silently add IF EXISTS if TEMPORARY was used.
      */
      if (thd->slave_thread)
        lex->drop_if_exists= 1;

      /* So that DROP TEMPORARY TABLE gets to binlog at commit/rollback */
      thd->options|= OPTION_KEEP_LOG;
    }
    /* DDL and binlog write order protected by LOCK_open */
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
  case SQLCOM_SHOW_AUTHORS:
    res= mysqld_show_authors(thd);
    break;
  case SQLCOM_SHOW_CONTRIBUTORS:
    res= mysqld_show_contributors(thd);
    break;
  case SQLCOM_SHOW_PRIVILEGES:
    res= mysqld_show_privileges(thd);
    break;
  case SQLCOM_SHOW_COLUMN_TYPES:
    res= mysqld_show_column_types(thd);
    break;
  case SQLCOM_SHOW_ENGINE_LOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0));	/* purecov: inspected */
    goto error;
#else
    {
      if (check_access(thd, FILE_ACL, any_db,0,0,0,0))
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

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      goto error;

    res= mysql_load(thd, lex->exchange, first_table, lex->field_list,
                    lex->update_list, lex->value_list, lex->duplicates,
                    lex->ignore, (bool) lex->local_file);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;

    if (lex->autocommit && end_active_trans(thd))
      goto error;

    if ((check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE) ||
	 open_and_lock_tables(thd, all_tables)))
      goto error;
    if (lex->one_shot_set && not_all_support_one_shot(lex_var_list))
    {
      my_error(ER_RESERVED_SYNTAX, MYF(0), "SET ONE_SHOT");
      goto error;
    }
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
    unlock_locked_tables(thd);
    if (thd->options & OPTION_TABLE_LOCK)
    {
      end_active_trans(thd);
      thd->options&= ~(OPTION_TABLE_LOCK);
    }
    if (thd->global_read_lock)
      unlock_global_read_lock(thd);
    my_ok(thd);
    break;
  case SQLCOM_LOCK_TABLES:
    unlock_locked_tables(thd);
    /* we must end the trasaction first, regardless of anything */
    if (end_active_trans(thd))
      goto error;
    if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                           UINT_MAX, FALSE))
      goto error;
    if (lex->protect_against_global_read_lock &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      goto error;
    thd->in_lock_tables=1;
    thd->options|= OPTION_TABLE_LOCK;

    if (!(res= simple_open_n_lock_tables(thd, all_tables)))
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->variables.query_cache_wlock_invalidate)
	query_cache.invalidate_locked_for_write(first_table);
#endif /*HAVE_QUERY_CACHE*/
      thd->locked_tables=thd->lock;
      thd->lock=0;
      my_ok(thd);
    }
    else
    {
      /* 
        Need to end the current transaction, so the storage engine (InnoDB)
        can free its locks if LOCK TABLES locked some tables before finding
        that it can't lock a table in its list
      */
      ha_autocommit_or_rollback(thd, 1);
      end_active_trans(thd);
      thd->options&= ~(OPTION_TABLE_LOCK);
    }
    thd->in_lock_tables=0;
    break;
  case SQLCOM_CREATE_DB:
  {
    /*
      As mysql_create_db() may modify HA_CREATE_INFO structure passed to
      it, we need to use a copy of LEX::create_info to make execution
      prepared statement- safe.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    char *alias;
    if (!(alias=thd->strmake(lex->name.str, lex->name.length)) ||
        check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    /*
      If in a slave thread :
      CREATE DATABASE DB was certainly not preceded by USE DB.
      For that reason, db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread && 
	(!rpl_filter->db_ok(lex->name.str) ||
	 !rpl_filter->db_ok_with_wild_table(lex->name.str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd,CREATE_ACL,lex->name.str, 0, 1, 0,
                     is_schema_db(lex->name.str)))
      break;
    res= mysql_create_db(thd,(lower_case_table_names == 2 ? alias :
                              lex->name.str), &create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    /*
      If in a slave thread :
      DROP DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the 
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread && 
	(!rpl_filter->db_ok(lex->name.str) ||
	 !rpl_filter->db_ok_with_wild_table(lex->name.str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd,DROP_ACL,lex->name.str,0,1,0,
                     is_schema_db(lex->name.str)))
      break;
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_rm_db(thd, lex->name.str, lex->drop_if_exists, 0);
    break;
  }
  case SQLCOM_ALTER_DB_UPGRADE:
  {
    LEX_STRING *db= & lex->name;
    if (end_active_trans(thd))
    {
      res= 1;
      break;
    }
#ifdef HAVE_REPLICATION
    if (thd->slave_thread && 
       (!rpl_filter->db_ok(db->str) ||
        !rpl_filter->db_ok_with_wild_table(db->str)))
    {
      res= 1;
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_db_name(db))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), db->str);
      break;
    }
    if (check_access(thd, ALTER_ACL, db->str, 0, 1, 0, is_schema_db(db->str)) ||
        check_access(thd, DROP_ACL, db->str, 0, 1, 0, is_schema_db(db->str)) ||
        check_access(thd, CREATE_ACL, db->str, 0, 1, 0, is_schema_db(db->str)))
    {
      res= 1;
      break;
    }
    if (thd->locked_tables || thd->active_transaction())
    {
      res= 1;
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
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
    if (check_db_name(db))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), db->str);
      break;
    }
    /*
      If in a slave thread :
      ALTER DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread &&
	(!rpl_filter->db_ok(db->str) ||
	 !rpl_filter->db_ok_with_wild_table(db->str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, ALTER_ACL, db->str, 0, 1, 0, is_schema_db(db->str)))
      break;
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_alter_db(thd, db->str, &create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    DBUG_EXECUTE_IF("4x_server_emul",
                    my_error(ER_UNKNOWN_ERROR, MYF(0)); goto error;);
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
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
  if (!thd->spcont)
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
    if (check_access(thd,INSERT_ACL,"mysql",0,1,0,0))
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
    if (check_access(thd, INSERT_ACL, "mysql", 0, 1, 1, 0) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    if (end_active_trans(thd))
      goto error;
    /* Conditionally writes to binlog */
    if (!(res= mysql_create_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_DROP_USER:
  {
    if (check_access(thd, DELETE_ACL, "mysql", 0, 1, 1, 0) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    if (end_active_trans(thd))
      goto error;
    /* Conditionally writes to binlog */
    if (!(res= mysql_drop_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_RENAME_USER:
  {
    if (check_access(thd, UPDATE_ACL, "mysql", 0, 1, 1, 0) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    if (end_active_trans(thd))
      goto error;
    /* Conditionally writes to binlog */
    if (!(res= mysql_rename_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE_ALL:
  {
    if (end_active_trans(thd))
      goto error;
    if (check_access(thd, UPDATE_ACL, "mysql", 0, 1, 1, 0) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    /* Conditionally writes to binlog */
    if (!(res = mysql_revoke_all(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  {
    if (end_active_trans(thd))
      goto error;

    if (check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
		     first_table ?  first_table->db : select_lex->db,
		     first_table ? &first_table->grant.privilege : 0,
		     first_table ? 0 : 1, 0,
                     first_table ? (bool) first_table->schema_table :
                     select_lex->db ? is_schema_db(select_lex->db) : 0))
      goto error;

    if (thd->security_ctx->user)              // If not replication
    {
      LEX_USER *user, *tmp_user;

      List_iterator <LEX_USER> user_list(lex->users_list);
      while ((tmp_user= user_list++))
      {
        if (!(user= get_current_user(thd, tmp_user)))
          goto error;
        if (specialflag & SPECIAL_NO_RESOLVE &&
            hostname_requires_resolving(user->host.str))
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_WARN_HOSTNAME_WONT_WORK,
                              ER(ER_WARN_HOSTNAME_WONT_WORK),
                              user->host.str);
        // Are we trying to change a password of another user
        DBUG_ASSERT(user->host.str != 0);
        if (strcmp(thd->security_ctx->user, user->user.str) ||
            my_strcasecmp(system_charset_info,
                          user->host.str, thd->security_ctx->host_or_ip))
        {
          // TODO: use check_change_password()
          if (is_acl_user(user->host.str, user->user.str) &&
              user->password.str &&
              check_access(thd, UPDATE_ACL,"mysql",0,1,1,0))
          {
            my_message(ER_PASSWORD_NOT_ALLOWED,
                       ER(ER_PASSWORD_NOT_ALLOWED), MYF(0));
            goto error;
          }
        }
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
                        all_tables, 0, UINT_MAX, 0))
	  goto error;
        /* Conditionally writes to binlog */
        res= mysql_table_grant(thd, all_tables, lex->users_list,
			       lex->columns, lex->grant,
			       lex->sql_command == SQLCOM_REVOKE);
      }
    }
    else
    {
      if (lex->columns.elements || lex->type)
      {
	my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
                   MYF(0));
        goto error;
      }
      else
	/* Conditionally writes to binlog */
	res = mysql_grant(thd, select_lex->db, lex->users_list, lex->grant,
			  lex->sql_command == SQLCOM_REVOKE);
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
    bool write_to_binlog;
    if (check_global_access(thd,RELOAD_ACL))
      goto error;

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
      if (!lex->no_write_to_binlog && write_to_binlog)
      {
        write_bin_log(thd, FALSE, thd->query, thd->query_length);
      }
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
	!check_access(thd, SELECT_ACL, "mysql",0,1,0,0))
    {
      res = mysql_show_grants(thd, grant_user);
    }
    break;
  }
#endif
  case SQLCOM_HA_OPEN:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE))
      goto error;
    res= mysql_ha_open(thd, first_table, 0);
    break;
  case SQLCOM_HA_CLOSE:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    res= mysql_ha_close(thd, first_table);
    break;
  case SQLCOM_HA_READ:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    /*
      There is no need to check for table permissions here, because
      if a user has no permissions to read a table, he won't be
      able to open it (with SQLCOM_HA_OPEN) in the first place.
    */
    unit->set_limit(select_lex);
    res= mysql_ha_read(thd, first_table, lex->ha_read_mode, lex->ident.str,
                       lex->insert_list, lex->ha_rkey_mode, select_lex->where,
                       unit->select_limit_cnt, unit->offset_limit_cnt);
    break;

  case SQLCOM_BEGIN:
    if (thd->transaction.xid_state.xa_state != XA_NOTR)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    if (begin_trans(thd))
      goto error;
    if (lex->start_transaction_opt & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
    {
      if (ha_start_consistent_snapshot(thd))
        goto error;
    }
    my_ok(thd);
    break;
  case SQLCOM_COMMIT:
    if (end_trans(thd, lex->tx_release ? COMMIT_RELEASE :
                              lex->tx_chain ? COMMIT_AND_CHAIN : COMMIT))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_ROLLBACK:
    if (end_trans(thd, lex->tx_release ? ROLLBACK_RELEASE :
                              lex->tx_chain ? ROLLBACK_AND_CHAIN : ROLLBACK))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_RELEASE_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=thd->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (uchar *)lex->ident.str, lex->ident.length,
                       (uchar *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_release_savepoint(thd, sv))
        res= TRUE; // cannot happen
      else
        my_ok(thd);
      thd->transaction.savepoints=sv->prev;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=thd->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (uchar *)lex->ident.str, lex->ident.length,
                       (uchar *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_rollback_to_savepoint(thd, sv))
        res= TRUE; // cannot happen
      else
      {
        if (((thd->options & OPTION_KEEP_LOG) || 
             thd->transaction.all.modified_non_trans_table) &&
            !thd->slave_thread)
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_WARNING_NOT_COMPLETE_ROLLBACK,
                       ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
        my_ok(thd);
      }
      thd->transaction.savepoints=sv;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_SAVEPOINT:
    if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) ||
          thd->in_sub_stmt) || !opt_using_transactions)
      my_ok(thd);
    else
    {
      SAVEPOINT **sv, *newsv;
      for (sv=&thd->transaction.savepoints; *sv; sv=&(*sv)->prev)
      {
        if (my_strnncoll(system_charset_info,
                         (uchar *)lex->ident.str, lex->ident.length,
                         (uchar *)(*sv)->name, (*sv)->length) == 0)
          break;
      }
      if (*sv) /* old savepoint of the same name exists */
      {
        newsv=*sv;
        ha_release_savepoint(thd, *sv); // it cannot fail
        *sv=(*sv)->prev;
      }
      else if ((newsv=(SAVEPOINT *) alloc_root(&thd->transaction.mem_root,
                                               savepoint_alloc_size)) == 0)
      {
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
        break;
      }
      newsv->name=strmake_root(&thd->transaction.mem_root,
                               lex->ident.str, lex->ident.length);
      newsv->length=lex->ident.length;
      /*
        if we'll get an error here, don't add new savepoint to the list.
        we'll lose a little bit of memory in transaction mem_root, but it'll
        be free'd when transaction ends anyway
      */
      if (ha_savepoint(thd, newsv))
        res= TRUE;
      else
      {
        newsv->prev=thd->transaction.savepoints;
        thd->transaction.savepoints=newsv;
        my_ok(thd);
      }
    }
    break;
  case SQLCOM_CREATE_PROCEDURE:
  case SQLCOM_CREATE_SPFUNCTION:
  {
    uint namelen;
    char *name;
    int sp_result= SP_INTERNAL_ERROR;

    DBUG_ASSERT(lex->sphead != 0);
    DBUG_ASSERT(lex->sphead->m_db.str); /* Must be initialized in the parser */
    /*
      Verify that the database name is allowed, optionally
      lowercase it.
    */
    if (check_db_name(&lex->sphead->m_db))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->sphead->m_db.str);
      goto create_sp_error;
    }

    /*
      Check that a database directory with this name
      exists. Design note: This won't work on virtual databases
      like information_schema.
    */
    if (check_db_dir_existence(lex->sphead->m_db.str))
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), lex->sphead->m_db.str);
      goto create_sp_error;
    }

    if (check_access(thd, CREATE_PROC_ACL, lex->sphead->m_db.str, 0, 0, 0,
                     is_schema_db(lex->sphead->m_db.str)))
      goto create_sp_error;

    if (end_active_trans(thd))
      goto create_sp_error;

    name= lex->sphead->name(&namelen);
#ifdef HAVE_DLOPEN
    if (lex->sphead->m_type == TYPE_ENUM_FUNCTION)
    {
      udf_func *udf = find_udf(name, namelen);

      if (udf)
      {
        my_error(ER_UDF_EXISTS, MYF(0), name);
        goto create_sp_error;
      }
    }
#endif

    if (sp_process_definer(thd))
      goto create_sp_error;

    res= (sp_result= lex->sphead->create(thd));
    switch (sp_result) {
    case SP_OK: {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* only add privileges if really neccessary */

      Security_context security_context;
      bool restore_backup_context= false;
      Security_context *backup= NULL;
      LEX_USER *definer= thd->lex->definer;
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
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_PROC_AUTO_GRANT_FAIL,
                       ER(ER_PROC_AUTO_GRANT_FAIL));
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
    break;
    }
    case SP_WRITE_ROW_FAILED:
      my_error(ER_SP_ALREADY_EXISTS, MYF(0), SP_TYPE_STRING(lex), name);
    break;
    case SP_BAD_IDENTIFIER:
      my_error(ER_TOO_LONG_IDENT, MYF(0), name);
    break;
    case SP_BODY_TOO_LONG:
      my_error(ER_TOO_LONG_BODY, MYF(0), name);
    break;
    case SP_FLD_STORE_FAILED:
      my_error(ER_CANT_CREATE_SROUTINE, MYF(0), name);
      break;
    default:
      my_error(ER_SP_STORE_FAILED, MYF(0), SP_TYPE_STRING(lex), name);
    break;
    } /* end switch */

    /*
      Capture all errors within this CASE and
      clean up the environment.
    */
create_sp_error:
    if (sp_result != SP_OK )
      goto error;
    my_ok(thd);
    break; /* break super switch */
  } /* end case group bracket */
  case SQLCOM_CALL:
    {
      sp_head *sp;

      /*
        This will cache all SP and SF and open and lock all tables
        required for execution.
      */
      if (check_table_access(thd, SELECT_ACL, all_tables, UINT_MAX, FALSE) ||
	  open_and_lock_tables(thd, all_tables))
       goto error;

      /*
        By this moment all needed SPs should be in cache so no need to look 
        into DB. 
      */
      if (!(sp= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, lex->spname,
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
          my_ok(thd, (ulong) (thd->row_count_func < 0 ? 0 :
                              thd->row_count_func));
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
      int sp_result;
      sp_head *sp;
      st_sp_chistics chistics;

      memcpy(&chistics, &lex->sp_chistics, sizeof(chistics));
      if (lex->sql_command == SQLCOM_ALTER_PROCEDURE)
        sp= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, lex->spname,
                            &thd->sp_proc_cache, FALSE);
      else
        sp= sp_find_routine(thd, TYPE_ENUM_FUNCTION, lex->spname,
                            &thd->sp_func_cache, FALSE);
      thd->warning_info->opt_clear_warning_info(thd->query_id);
      if (! sp)
      {
	if (lex->spname->m_db.str)
	  sp_result= SP_KEY_NOT_FOUND;
	else
	{
	  my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
	  goto error;
	}
      }
      else
      {
        if (check_routine_access(thd, ALTER_PROC_ACL, sp->m_db.str, 
				 sp->m_name.str,
                                 lex->sql_command == SQLCOM_ALTER_PROCEDURE, 0))
	  goto error;

        if (end_active_trans(thd)) 
          goto error;
	memcpy(&lex->sp_chistics, &chistics, sizeof(lex->sp_chistics));
        if ((sp->m_type == TYPE_ENUM_FUNCTION) &&
            !trust_function_creators &&  mysql_bin_log.is_open() &&
            !sp->m_chistics->detistic &&
            (chistics.daccess == SP_CONTAINS_SQL ||
             chistics.daccess == SP_MODIFIES_SQL_DATA))
        {
          my_message(ER_BINLOG_UNSAFE_ROUTINE,
		     ER(ER_BINLOG_UNSAFE_ROUTINE), MYF(0));
          sp_result= SP_INTERNAL_ERROR;
        }
        else
        {
          /*
            Note that if you implement the capability of ALTER FUNCTION to
            alter the body of the function, this command should be made to
            follow the restrictions that log-bin-trust-function-creators=0
            already puts on CREATE FUNCTION.
          */
          /* Conditionally writes to binlog */

          int type= lex->sql_command == SQLCOM_ALTER_PROCEDURE ?
                    TYPE_ENUM_PROCEDURE :
                    TYPE_ENUM_FUNCTION;

          sp_result= sp_update_routine(thd,
                                       type,
                                       lex->spname,
                                       &lex->sp_chistics);
        }
      }
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
      int sp_result;
      int type= (lex->sql_command == SQLCOM_DROP_PROCEDURE ?
                 TYPE_ENUM_PROCEDURE : TYPE_ENUM_FUNCTION);

      sp_result= sp_routine_exists_in_table(thd, type, lex->spname);
      thd->warning_info->opt_clear_warning_info(thd->query_id);
      if (sp_result == SP_OK)
      {
        char *db= lex->spname->m_db.str;
	char *name= lex->spname->m_name.str;

	if (check_routine_access(thd, ALTER_PROC_ACL, db, name,
                                 lex->sql_command == SQLCOM_DROP_PROCEDURE, 0))
          goto error;

        if (end_active_trans(thd)) 
          goto error;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
	if (sp_automatic_privileges && !opt_noacl &&
	    sp_revoke_privileges(thd, db, name, 
                                 lex->sql_command == SQLCOM_DROP_PROCEDURE))
	{
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
		       ER_PROC_AUTO_REVOKE_FAIL,
		       ER(ER_PROC_AUTO_REVOKE_FAIL));
	}
#endif
        /* Conditionally writes to binlog */

        int type= lex->sql_command == SQLCOM_DROP_PROCEDURE ?
                  TYPE_ENUM_PROCEDURE :
                  TYPE_ENUM_FUNCTION;

        sp_result= sp_drop_routine(thd, type, lex->spname);
      }
      else
      {
#ifdef HAVE_DLOPEN
	if (lex->sql_command == SQLCOM_DROP_FUNCTION)
	{
          udf_func *udf = find_udf(lex->spname->m_name.str,
                                   lex->spname->m_name.length);
          if (udf)
          {
	    if (check_access(thd, DELETE_ACL, "mysql", 0, 1, 0, 0))
	      goto error;

	    if (!(res = mysql_drop_function(thd, &lex->spname->m_name)))
	    {
	      my_ok(thd);
	      break;
	    }
	  }
	}
#endif
	if (lex->spname->m_db.str)
	  sp_result= SP_KEY_NOT_FOUND;
	else
	{
	  my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
	  goto error;
	}
      }
      res= sp_result;
      switch (sp_result) {
      case SP_OK:
	my_ok(thd);
	break;
      case SP_KEY_NOT_FOUND:
	if (lex->drop_if_exists)
	{
          write_bin_log(thd, TRUE, thd->query, thd->query_length);
	  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			      ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
			      SP_COM_STRING(lex), lex->spname->m_name.str);
	  res= FALSE;
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
      if (sp_show_create_routine(thd, TYPE_ENUM_PROCEDURE, lex->spname))
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_name.str);
	goto error;
      }
      break;
    }
  case SQLCOM_SHOW_CREATE_FUNC:
    {
      if (sp_show_create_routine(thd, TYPE_ENUM_FUNCTION, lex->spname))
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_name.str);
	goto error;
      }
      break;
    }
#ifndef DBUG_OFF
  case SQLCOM_SHOW_PROC_CODE:
  case SQLCOM_SHOW_FUNC_CODE:
    {
      sp_head *sp;

      if (lex->sql_command == SQLCOM_SHOW_PROC_CODE)
        sp= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, lex->spname,
                            &thd->sp_proc_cache, FALSE);
      else
        sp= sp_find_routine(thd, TYPE_ENUM_FUNCTION, lex->spname,
                            &thd->sp_func_cache, FALSE);
      if (!sp || sp->show_routine_code(thd))
      {
        /* We don't distinguish between errors for now */
        my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_name.str);
        goto error;
      }
      break;
    }
#endif // ifndef DBUG_OFF
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
      if (end_active_trans(thd))
        goto error;

      res= mysql_create_view(thd, first_table, thd->lex->create_view_mode);
      break;
    }
  case SQLCOM_DROP_VIEW:
    {
      if (check_table_access(thd, DROP_ACL, all_tables, UINT_MAX, FALSE) ||
          end_active_trans(thd))
        goto error;
      /* Conditionally writes to binlog. */
      res= mysql_drop_view(thd, first_table, thd->lex->drop_mode);
      break;
    }
  case SQLCOM_CREATE_TRIGGER:
  {
    if (end_active_trans(thd))
      goto error;

    /* Conditionally writes to binlog. */
    res= mysql_create_or_drop_trigger(thd, all_tables, 1);

    break;
  }
  case SQLCOM_DROP_TRIGGER:
  {
    if (end_active_trans(thd))
      goto error;

    /* Conditionally writes to binlog. */
    res= mysql_create_or_drop_trigger(thd, all_tables, 0);
    break;
  }
  case SQLCOM_XA_START:
    if (thd->transaction.xid_state.xa_state == XA_IDLE &&
        thd->lex->xa_opt == XA_RESUME)
    {
      if (! thd->transaction.xid_state.xid.eq(thd->lex->xid))
      {
        my_error(ER_XAER_NOTA, MYF(0));
        break;
      }
      thd->transaction.xid_state.xa_state=XA_ACTIVE;
      my_ok(thd);
      break;
    }
    if (thd->lex->xa_opt != XA_NONE)
    { // JOIN is not supported yet. TODO
      my_error(ER_XAER_INVAL, MYF(0));
      break;
    }
    if (thd->transaction.xid_state.xa_state != XA_NOTR)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    if (thd->active_transaction() || thd->locked_tables)
    {
      my_error(ER_XAER_OUTSIDE, MYF(0));
      break;
    }
    if (xid_cache_search(thd->lex->xid))
    {
      my_error(ER_XAER_DUPID, MYF(0));
      break;
    }
    DBUG_ASSERT(thd->transaction.xid_state.xid.is_null());
    thd->transaction.xid_state.xa_state=XA_ACTIVE;
    thd->transaction.xid_state.rm_error= 0;
    thd->transaction.xid_state.xid.set(thd->lex->xid);
    xid_cache_insert(&thd->transaction.xid_state);
    thd->transaction.all.modified_non_trans_table= FALSE;
    thd->options= ((thd->options & ~(OPTION_KEEP_LOG)) | OPTION_BEGIN);
    thd->server_status|= SERVER_STATUS_IN_TRANS;
    my_ok(thd);
    break;
  case SQLCOM_XA_END:
    /* fake it */
    if (thd->lex->xa_opt != XA_NONE)
    { // SUSPEND and FOR MIGRATE are not supported yet. TODO
      my_error(ER_XAER_INVAL, MYF(0));
      break;
    }
    if (thd->transaction.xid_state.xa_state != XA_ACTIVE)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    {
      my_error(ER_XAER_NOTA, MYF(0));
      break;
    }
    if (xa_trans_rolled_back(&thd->transaction.xid_state))
      break;
    thd->transaction.xid_state.xa_state=XA_IDLE;
    my_ok(thd);
    break;
  case SQLCOM_XA_PREPARE:
    if (thd->transaction.xid_state.xa_state != XA_IDLE)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    {
      my_error(ER_XAER_NOTA, MYF(0));
      break;
    }
    if (ha_prepare(thd))
    {
      my_error(ER_XA_RBROLLBACK, MYF(0));
      xid_cache_delete(&thd->transaction.xid_state);
      thd->transaction.xid_state.xa_state=XA_NOTR;
      break;
    }
    thd->transaction.xid_state.xa_state=XA_PREPARED;
    my_ok(thd);
    break;
  case SQLCOM_XA_COMMIT:
    if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    {
      XID_STATE *xs=xid_cache_search(thd->lex->xid);
      if (!xs || xs->in_thd)
        my_error(ER_XAER_NOTA, MYF(0));
      else if (xa_trans_rolled_back(xs))
      {
        ha_commit_or_rollback_by_xid(thd->lex->xid, 0);
        xid_cache_delete(xs);
        break;
      }
      else
      {
        ha_commit_or_rollback_by_xid(thd->lex->xid, 1);
        xid_cache_delete(xs);
        my_ok(thd);
      }
      break;
    }
    if (xa_trans_rolled_back(&thd->transaction.xid_state))
    {
      xa_trans_rollback(thd);
      break;
    }
    if (thd->transaction.xid_state.xa_state == XA_IDLE &&
        thd->lex->xa_opt == XA_ONE_PHASE)
    {
      int r;
      if ((r= ha_commit(thd)))
        my_error(r == 1 ? ER_XA_RBROLLBACK : ER_XAER_RMERR, MYF(0));
      else
        my_ok(thd);
    }
    else if (thd->transaction.xid_state.xa_state == XA_PREPARED &&
             thd->lex->xa_opt == XA_NONE)
    {
      if (wait_if_global_read_lock(thd, 0, 0))
      {
        ha_rollback(thd);
        my_error(ER_XAER_RMERR, MYF(0));
      }
      else
      {
        if (ha_commit_one_phase(thd, 1))
          my_error(ER_XAER_RMERR, MYF(0));
        else
          my_ok(thd);
        start_waiting_global_read_lock(thd);
      }
    }
    else
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
    thd->transaction.all.modified_non_trans_table= FALSE;
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    xid_cache_delete(&thd->transaction.xid_state);
    thd->transaction.xid_state.xa_state=XA_NOTR;
    break;
  case SQLCOM_XA_ROLLBACK:
    if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    {
      XID_STATE *xs=xid_cache_search(thd->lex->xid);
      if (!xs || xs->in_thd)
        my_error(ER_XAER_NOTA, MYF(0));
      else
      {
        bool ok= !xa_trans_rolled_back(xs);
        ha_commit_or_rollback_by_xid(thd->lex->xid, 0);
        xid_cache_delete(xs);
        if (ok)
          my_ok(thd);
      }
      break;
    }
    if (thd->transaction.xid_state.xa_state != XA_IDLE &&
        thd->transaction.xid_state.xa_state != XA_PREPARED &&
        thd->transaction.xid_state.xa_state != XA_ROLLBACK_ONLY)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    if (xa_trans_rollback(thd))
      my_error(ER_XAER_RMERR, MYF(0));
    else
      my_ok(thd);
    break;
  case SQLCOM_XA_RECOVER:
    res= mysql_xa_recover(thd);
    break;
  case SQLCOM_ALTER_TABLESPACE:
    if (check_access(thd, ALTER_ACL, thd->db, 0, 1, 0, thd->db ? is_schema_db(thd->db) : 0))
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
    int error;
    LEX *lex= thd->lex;
    DBUG_PRINT("info", ("case SQLCOM_CREATE_SERVER"));

    if (check_global_access(thd, SUPER_ACL))
      break;

    if ((error= create_server(thd, &lex->server_options)))
    {
      DBUG_PRINT("info", ("problem creating server <%s>",
                          lex->server_options.server_name));
      my_error(error, MYF(0), lex->server_options.server_name);
      break;
    }
    my_ok(thd, 1);
    break;
  }
  case SQLCOM_ALTER_SERVER:
  {
    int error;
    LEX *lex= thd->lex;
    DBUG_PRINT("info", ("case SQLCOM_ALTER_SERVER"));

    if (check_global_access(thd, SUPER_ACL))
      break;

    if ((error= alter_server(thd, &lex->server_options)))
    {
      DBUG_PRINT("info", ("problem altering server <%s>",
                          lex->server_options.server_name));
      my_error(error, MYF(0), lex->server_options.server_name);
      break;
    }
    my_ok(thd, 1);
    break;
  }
  case SQLCOM_DROP_SERVER:
  {
    int err_code;
    LEX *lex= thd->lex;
    DBUG_PRINT("info", ("case SQLCOM_DROP_SERVER"));

    if (check_global_access(thd, SUPER_ACL))
      break;

    if ((err_code= drop_server(thd, &lex->server_options)))
    {
      if (! lex->drop_if_exists && err_code == ER_FOREIGN_SERVER_DOESNT_EXIST)
      {
        DBUG_PRINT("info", ("problem dropping server %s",
                            lex->server_options.server_name));
        my_error(err_code, MYF(0), lex->server_options.server_name);
      }
      else
      {
        my_ok(thd, 0);
      }
      break;
    }
    my_ok(thd, 1);
    break;
  }
  case SQLCOM_SIGNAL:
  case SQLCOM_RESIGNAL:
    DBUG_ASSERT(lex->m_stmt != NULL);
    res= lex->m_stmt->execute(thd);
    break;
  default:
#ifndef EMBEDDED_LIBRARY
    DBUG_ASSERT(0);                             /* Impossible */
#endif
    my_ok(thd);
    break;
  }
  thd_proc_info(thd, "query end");

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

  /*
    The return value for ROW_COUNT() is "implementation dependent" if the
    statement is not DELETE, INSERT or UPDATE, but -1 is what JDBC and ODBC
    wants. We also keep the last value in case of SQLCOM_CALL or
    SQLCOM_EXECUTE.
  */
  if (!(sql_command_flags[lex->sql_command] & CF_HAS_ROW_COUNT))
    thd->row_count_func= -1;

  goto finish;

error:
  res= TRUE;

finish:
  if (need_start_waiting)
  {
    /*
      Release the protection against the global read lock and wake
      everyone, who might want to set a global read lock.
    */
    start_waiting_global_read_lock(thd);
  }
  DBUG_RETURN(res || thd->is_error());
}


static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables)
{
  LEX	*lex= thd->lex;
  select_result *result=lex->result;
  bool res;
  /* assign global limit variable if limit is not given */
  {
    SELECT_LEX *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((ulonglong) thd->variables.select_limit);
  }
  if (!(res= open_and_lock_tables(thd, all_tables)))
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
      thd->send_explain_fields(result);
      res= mysql_explain_union(thd, &thd->lex->unit, result);
      if (lex->describe & DESCRIBE_EXTENDED)
      {
        char buff[1024];
        String str(buff,(uint32) sizeof(buff), system_charset_info);
        str.length(0);
        thd->lex->unit.print(&str, QT_ORDINARY);
        str.append('\0');
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                     ER_YES, str.ptr());
      }
      if (res)
        result->abort();
      else
        result->send_eof();
      delete result;
    }
    else
    {
      if (!result && !(result= new select_send()))
        return 1;                               /* purecov: inspected */
      query_cache_store_query(thd, all_tables);
      res= handle_select(thd, lex, result, 0);
      if (result != lex->result)
        delete result;
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
		   &all_tables->grant.privilege, 0, no_errors,
                   test(all_tables->schema_table)))
    goto deny;

  /* Show only 1 table for check_grant */
  if (!(all_tables->belong_to_view &&
        (thd->lex->sql_command == SQLCOM_SHOW_FIELDS)) &&
      !(all_tables->view &&
        all_tables->effective_algorithm == VIEW_ALGORITHM_TMPTABLE) &&
      check_grant(thd, privilege, all_tables, 0, 1, no_errors))
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

  /* Check rights on tables of subselects and implictly opened tables */
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
        (check_table_access(thd, SELECT_ACL, subselects_tables, UINT_MAX, FALSE)))
      return 1;
  }
  return 0;
}


/**
  Get the user (global) and database privileges for all used tables.

  @param save_priv    In this we store global and db level grants for the
                      table. Note that we don't store db level grants if the
                      global grants is enough to satisfy the request and the
                      global grants contains a SELECT grant.

  @note
    The idea of EXTRA_ACL is that one will be granted access to the table if
    one has the asked privilege on any column combination of the table; For
    example to be able to check a table one needs to have SELECT privilege on
    any column of the table.

  @retval
    0  ok
  @retval
    1  If we can't get the privileges and we don't use table/column
    grants.
*/
bool
check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
	     bool dont_check_global_grants, bool no_errors, bool schema_db)
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
  bool  db_is_pattern= (test(want_access & GRANT_ACL) &&
                        dont_check_global_grants);
  ulong dummy;
  DBUG_ENTER("check_access");
  DBUG_PRINT("enter",("db: %s  want_access: %lu  master_access: %lu",
                      db ? db : "", want_access, sctx->master_access));
  if (save_priv)
    *save_priv=0;
  else
    save_priv= &dummy;

  thd_proc_info(thd, "checking permissions");
  if ((!db || !db[0]) && !thd->db && !dont_check_global_grants)
  {
    DBUG_PRINT("error",("No database"));
    if (!no_errors)
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR),
                 MYF(0));                       /* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (schema_db)
  {
    if ((!(sctx->master_access & FILE_ACL) && (want_access & FILE_ACL)) ||
        (want_access & ~(SELECT_ACL | EXTRA_ACL | FILE_ACL)))
    {
      if (!no_errors)
      {
        const char *db_name= db ? db : thd->db;
        my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
                 sctx->priv_user, sctx->priv_host, db_name);
      }
      DBUG_RETURN(TRUE);
    }
    else
    {
      *save_priv= SELECT_ACL;
      DBUG_RETURN(FALSE);
    }
  }

  if ((sctx->master_access & want_access) == want_access)
  {
    /*
      If we don't have a global SELECT privilege, we have to get the database
      specific access rights to be able to handle queries of type
      UPDATE t1 SET a=1 WHERE b > 0
    */
    db_access= sctx->db_access;
    if (!(sctx->master_access & SELECT_ACL) &&
	(db && (!thd->db || db_is_pattern || strcmp(db,thd->db))))
      db_access=acl_get(sctx->host, sctx->ip, sctx->priv_user, db,
                        db_is_pattern);
    *save_priv=sctx->master_access | db_access;
    DBUG_RETURN(FALSE);
  }
  if (((want_access & ~sctx->master_access) & ~(DB_ACLS | EXTRA_ACL)) ||
      (! db && dont_check_global_grants))
  {						// We can never grant this
    DBUG_PRINT("error",("No possible access"));
    if (!no_errors)
      my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
               sctx->priv_user,
               sctx->priv_host,
               (thd->password ?
                ER(ER_YES) :
                ER(ER_NO)));                    /* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (db == any_db)
    DBUG_RETURN(FALSE);				// Allow select on anything

  if (db && (!thd->db || db_is_pattern || strcmp(db,thd->db)))
    db_access= acl_get(sctx->host, sctx->ip, sctx->priv_user, db,
                       db_is_pattern);
  else
    db_access= sctx->db_access;
  DBUG_PRINT("info",("db_access: %lu", db_access));
  /* Remove SHOW attribute and access rights we already have */
  want_access &= ~(sctx->master_access | EXTRA_ACL);
  DBUG_PRINT("info",("db_access: %lu  want_access: %lu",
                     db_access, want_access));
  db_access= ((*save_priv=(db_access | sctx->master_access)) & want_access);

  if (db_access == want_access ||
      (!dont_check_global_grants &&
       !(want_access & ~(db_access | TABLE_ACLS | PROC_ACLS))))
    DBUG_RETURN(FALSE);				/* Ok */

  DBUG_PRINT("error",("Access denied"));
  if (!no_errors)
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user, sctx->priv_host,
             (db ? db : (thd->db ?
                         thd->db :
                         "unknown")));          /* purecov: tested */
  DBUG_RETURN(TRUE);				/* purecov: tested */
}


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
                     &thd->col_access, FALSE, FALSE,
                     is_schema_db(dst_db_name)))
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
    dst_table= (TABLE_LIST *) table->schema_select_lex->table_list.first;

    DBUG_ASSERT(dst_table);

    if (check_access(thd, SELECT_ACL | EXTRA_ACL,
                     dst_table->db,
                     &dst_table->grant.privilege,
                     FALSE, FALSE,
                     test(dst_table->schema_table)))
      return FALSE;

    return (check_grant(thd, SELECT_ACL, dst_table, 2, UINT_MAX, FALSE));
  }
  default:
    break;
  }

  return FALSE;
}


/**
  Check the privilege for all used tables.

  @param    thd          Thread context
  @param    want_access  Privileges requested
  @param    tables       List of tables to be checked
  @param    number       Check at most this number of tables.
  @param    no_errors    FALSE/TRUE - report/don't report error to
                         the client (using my_error() call).

  @note
    Table privileges are cached in the table list for GRANT checking.
    This functions assumes that table list used and
    thd->lex->query_tables_own_last value correspond to each other
    (the latter should be either 0 or point to next_global member
    of one of elements of this table list).

  @retval  FALSE   OK
  @retval  TRUE    Access denied
*/

bool
check_table_access(THD *thd, ulong want_access,TABLE_LIST *tables,
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
  for (; i < number && tables != first_not_own_table;
       tables= tables->next_global, i++)
  {
    if (tables->security_ctx)
      sctx= tables->security_ctx;
    else
      sctx= backup_ctx;

    if (tables->schema_table && 
        (want_access & ~(SELECT_ACL | EXTRA_ACL | FILE_ACL)))
    {
      if (!no_errors)
        my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
                 sctx->priv_user, sctx->priv_host,
                 INFORMATION_SCHEMA_NAME.str);
      return TRUE;
    }
    /*
       Register access for view underlying table.
       Remove SHOW_VIEW_ACL, because it will be checked during making view
     */
    tables->grant.orig_want_privilege= (want_access & ~SHOW_VIEW_ACL);

    if (tables->schema_table_reformed)
    {
      if (check_show_access(thd, tables))
        goto deny;

      continue;
    }

    if (tables->is_anonymous_derived_table() ||
        (tables->table && (int)tables->table->s->tmp_table))
      continue;
    thd->security_ctx= sctx;
    if ((sctx->master_access & want_access) ==
        (want_access & ~EXTRA_ACL) &&
	thd->db)
      tables->grant.privilege= want_access;
    else if (tables->db && thd->db && strcmp(tables->db, thd->db) == 0)
    {
      if (check_access(thd, want_access, tables->get_db_name(),
                       &tables->grant.privilege, 0, no_errors, 
                       test(tables->schema_table)))
        goto deny;                            // Access denied
    }
    else if (check_access(thd, want_access, tables->get_db_name(),
                          &tables->grant.privilege, 0, no_errors, 
                          test(tables->schema_table)))
      goto deny;
  }
  thd->security_ctx= backup_ctx;
  return check_grant(thd,want_access & ~EXTRA_ACL,org_tables,
		       test(want_access & EXTRA_ACL), number, no_errors);
deny:
  thd->security_ctx= backup_ctx;
  return TRUE;
}


bool
check_routine_access(THD *thd, ulong want_access,char *db, char *name,
		     bool is_proc, bool no_errors)
{
  TABLE_LIST tables[1];
  
  bzero((char *)tables, sizeof(TABLE_LIST));
  tables->db= db;
  tables->table_name= tables->alias= name;
  
  /*
    The following test is just a shortcut for check_access() (to avoid
    calculating db_access) under the assumption that it's common to
    give persons global right to execute all stored SP (but not
    necessary to create them).
  */
  if ((thd->security_ctx->master_access & want_access) == want_access)
    tables->grant.privilege= want_access;
  else if (check_access(thd,want_access,db,&tables->grant.privilege,
			0, no_errors, 0))
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
  if (thd->security_ctx->master_access & SHOW_PROC_ACLS)
    return FALSE;
  /*
    There are no routines in information_schema db. So we can safely
    pass zero to last paramter of check_access function
  */
  if (!check_access(thd, SHOW_PROC_ACLS, db, &save_priv, 0, 1, 0) ||
      (save_priv & SHOW_PROC_ACLS))
    return FALSE;
  return check_routine_level_acl(thd, db, name, is_proc);
}


/*
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
                        &table->grant.privilege, 0, 1,
                        test(table->schema_table)) &&
           !check_grant(thd, access, table, 0, 1, 1))
        DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("exit",("no matching access rights"));
  DBUG_RETURN(1);
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
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  char command[128];
  if ((thd->security_ctx->master_access & want_access))
    return 0;
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), command);
  return 1;
#else
  return 0;
#endif
}

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

#ifndef EMBEDDED_LIBRARY

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
    char ebuff[MYSQL_ERRMSG_SIZE];
    my_snprintf(ebuff, sizeof(ebuff), ER(ER_STACK_OVERRUN_NEED_MORE),
                stack_used, my_thread_stack_size, margin);
    my_message(ER_STACK_OVERRUN_NEED_MORE, ebuff, MYF(ME_FATALERROR));
    thd->fatal_error();
    return 1;
  }
#ifndef DBUG_OFF
  max_stack_used= max(max_stack_used, stack_used);
#endif
  return 0;
}
#endif /* EMBEDDED_LIBRARY */

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
 Reset THD part responsible for command processing state.

   This needs to be called before execution of every statement
   (prepared or conventional).
   It is not called by substatements of routines.

  @todo
   Make it a method of THD and align its name with the rest of
   reset/end/start/init methods.
  @todo
   Call it after we use THD for queries, not before.
*/

void mysql_reset_thd_for_next_command(THD *thd)
{
  DBUG_ENTER("mysql_reset_thd_for_next_command");
  DBUG_ASSERT(!thd->spcont); /* not for substatements of routines */
  DBUG_ASSERT(! thd->in_sub_stmt);
  thd->free_list= 0;
  thd->select_number= 1;
  /*
    Those two lines below are theoretically unneeded as
    THD::cleanup_after_query() should take care of this already.
  */
  thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;

  thd->query_start_used= 0;
  thd->is_fatal_error= thd->time_zone_used= 0;
  /*
    Clear the status flag that are expected to be cleared at the
    beginning of each SQL statement.
  */
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
  /*
    If in autocommit mode and not in a transaction, reset
    OPTION_STATUS_NO_TRANS_UPDATE | OPTION_KEEP_LOG to not get warnings
    in ha_rollback_trans() about some tables couldn't be rolled back.
  */
  if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    thd->options&= ~OPTION_KEEP_LOG;
    thd->transaction.all.modified_non_trans_table= FALSE;
  }
  DBUG_ASSERT(thd->security_ctx== &thd->main_security_ctx);
  thd->thread_specific_used= FALSE;

  if (opt_bin_log)
  {
    reset_dynamic(&thd->user_var_events);
    thd->user_var_events_alloc= thd->mem_root;
  }
  thd->clear_error();
  thd->stmt_da->reset_diagnostics_area();
  thd->warning_info->reset_for_next_command();
  thd->rand_used= 0;
  thd->sent_row_count= thd->examined_row_count= 0;

  /*
    Because we come here only for start of top-statements, binlog format is
    constant inside a complex statement (using stored functions) etc.
  */
  thd->reset_current_stmt_binlog_row_based();

  DBUG_PRINT("debug",
             ("current_stmt_binlog_row_based: %d",
              thd->current_stmt_binlog_row_based));

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
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT,MYF(0),MAX_SELECT_NESTING);
    DBUG_RETURN(1);
  }
  select_lex->nest_level= lex->nest_level;
  /*
    Don't evaluate this subquery during statement prepare even if
    it's a constant one. The flag is switched off in the end of
    mysqld_stmt_prepare.
  */
  if (thd->stmt_arena->is_stmt_prepare())
    select_lex->uncacheable|= UNCACHEABLE_PREPARE;
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
    unit->return_to= lex->current_select;
    select_lex->include_down(unit);
    /*
      By default we assume that it is usual subselect and we have outer name
      resolution context, if no we will assign it to 0 later
    */
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
  bzero((char*) &null_lex_string.str, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if ((var= get_system_var(thd, OPT_SESSION, tmp, null_lex_string)))
  {
    end= strxmov(buff, "@@session.", var_name, NullS);
    var->set_name(buff, end-buff, system_charset_info);
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
  lex->lock_option= TL_READ_DEFAULT;
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
  @param       inBuf   Begining of the query text
  @param       length  Length of the query text
  @param[out]  found_semicolon For multi queries, position of the character of
                               the next query in the query text.
*/

void mysql_parse(THD *thd, const char *inBuf, uint length,
                 const char ** found_semicolon)
{
  int error;
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

  if (query_cache_send_result_to_client(thd, (char*) inBuf, length) <= 0)
  {
    LEX *lex= thd->lex;

    sp_cache_flush_obsolete(&thd->sp_proc_cache);
    sp_cache_flush_obsolete(&thd->sp_func_cache);

    Parser_state parser_state(thd, inBuf, length);

    bool err= parse_sql(thd, & parser_state, NULL);
    *found_semicolon= parser_state.m_lip.found_semicolon;

    if (!err)
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (mqh_used && thd->user_connect &&
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
          if (*found_semicolon &&
              (thd->query_length= (ulong)(*found_semicolon - thd->query)))
            thd->query_length--;
          /* Actually execute the query */
          if (*found_semicolon)
          {
            lex->safe_to_cache_query= 0;
            thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
          }
          lex->set_trg_event_type_for_tables();
          MYSQL_QUERY_EXEC_START(thd->query,
                                 thd->thread_id,
                                 (char *) (thd->db ? thd->db : ""),
                                 thd->security_ctx->priv_user,
                                 (char *) thd->security_ctx->host_or_ip,
                                 0);

          error= mysql_execute_command(thd);
          MYSQL_QUERY_EXEC_DONE(error);
	}
      }
    }
    else
    {
      DBUG_ASSERT(thd->is_error());
      DBUG_PRINT("info",("Command aborted. Fatal_error: %d",
			 thd->is_fatal_error));

      query_cache_abort(&thd->net);
    }
    if (thd->lex->sphead)
    {
      delete thd->lex->sphead;
      thd->lex->sphead= 0;
    }
    lex->unit.cleanup();
    thd_proc_info(thd, "freeing items");
    thd->end_statement();
    thd->cleanup_after_query();
    DBUG_ASSERT(thd->change_list.is_empty());
  }
  else
  {
    /* There are no multi queries in the cache. */
    *found_semicolon= NULL;
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

bool mysql_test_parse_for_slave(THD *thd, char *inBuf, uint length)
{
  LEX *lex= thd->lex;
  bool error= 0;
  DBUG_ENTER("mysql_test_parse_for_slave");

  Parser_state parser_state(thd, inBuf, length);
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  if (!parse_sql(thd, & parser_state, NULL) &&
      all_tables_not_ok(thd,(TABLE_LIST*) lex->select_lex.table_list.first))
    error= 1;                  /* Ignore question */
  thd->end_statement();
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
                       List<String> *interval_list, CHARSET_INFO *cs,
		       uint uint_geom_type)
{
  register Create_field *new_field;
  LEX  *lex= thd->lex;
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
    lex->col_list.push_back(new Key_part_spec(field_name->str, 0));
    key= new Key(Key::PRIMARY, NullS,
                      &default_key_create_info,
                      0, lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(field_name->str, 0));
    key= new Key(Key::UNIQUE, NullS,
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
      
      We allow only one function as part of default value - 
      NOW() as default for TIMESTAMP type.
    */
    if (default_value->type() == Item::FUNC_ITEM && 
        !(((Item_func*)default_value)->functype() == Item_func::NOW_FUNC &&
         type == MYSQL_TYPE_TIMESTAMP))
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

  if (on_update_value && type != MYSQL_TYPE_TIMESTAMP)
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    DBUG_RETURN(1);
  }

  if (type == MYSQL_TYPE_TIMESTAMP && length)
  {
    /* Display widths are no longer supported for TIMSTAMP as of MySQL 4.1.
       In other words, for declarations such as TIMESTAMP(2), TIMESTAMP(4),
       and so on, the display width is ignored.
    */
    char buf[32];
    my_snprintf(buf, sizeof(buf), "TIMESTAMP(%s)", length);
    WARN_DEPRECATED(thd, "6.0", buf, "'TIMESTAMP'");
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
  current_thd->lex->last_field->after=my_const_cast(char*) (name);
}

bool
add_proc_to_list(THD* thd, Item *item)
{
  ORDER *order;
  Item	**item_ptr;

  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER)+sizeof(Item*))))
    return 1;
  item_ptr = (Item**) (order+1);
  *item_ptr= item;
  order->item=item_ptr;
  order->free_me=0;
  thd->lex->proc_list.link_in_list((uchar*) order,(uchar**) &order->next);
  return 0;
}


/**
  save order by and tables in own lists.
*/

bool add_to_list(THD *thd, SQL_LIST &list,Item *item,bool asc)
{
  ORDER *order;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER))))
    DBUG_RETURN(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list((uchar*) order,(uchar**) &order->next);
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
					     List<Index_hint> *index_hints_arg,
                                             LEX_STRING *option)
{
  register TABLE_LIST *ptr;
  TABLE_LIST *previous_table_ref; /* The table preceding the current one. */
  char *alias_str;
  LEX *lex= thd->lex;
  DBUG_ENTER("add_table_to_list");
  LINT_INIT(previous_table_ref);

  if (!table)
    DBUG_RETURN(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (!test(table_options & TL_OPTION_ALIAS) && 
      check_table_name(table->table.str, table->table.length))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table->table.str);
    DBUG_RETURN(0);
  }

  if (table->is_derived_table() == FALSE && table->db.str &&
      check_db_name(&table->db))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), table->db.str);
    DBUG_RETURN(0);
  }

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
    ptr->db= table->db.str;
    ptr->db_length= table->db.length;
  }
  else if (lex->copy_db_to(&ptr->db, &ptr->db_length))
    DBUG_RETURN(0);

  ptr->alias= alias_str;
  if (lower_case_table_names && table->table.length)
    table->table.length= my_casedn_str(files_charset_info, table->table.str);
  ptr->table_name=table->table.str;
  ptr->table_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->updating=    test(table_options & TL_OPTION_UPDATING);
  ptr->force_index= test(table_options & TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= test(table_options & TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  if (!ptr->derived && !my_strcasecmp(system_charset_info, ptr->db,
                                      INFORMATION_SCHEMA_NAME.str))
  {
    ST_SCHEMA_TABLE *schema_table= find_schema_table(thd, ptr->table_name);
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
    TABLE_LIST *first_table= (TABLE_LIST*) table_list.first;
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
  table_list.link_in_list((uchar*) ptr, (uchar**) &ptr->next_local);
  ptr->next_name_resolution_table= NULL;
  /* Link table in global list (all used tables) */
  lex->add_to_query_tables(ptr);
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
  for (TABLE_LIST *tables= (TABLE_LIST*) table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
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
  return thd->lex->push_context(on_context);
}


/**
  Add an ON condition to the second operand of a JOIN ... ON.

    Add an ON condition to the right operand of a JOIN ... ON clause.

  @param b     the second operand of a JOIN ... ON
  @param expr  the condition to be added to the ON clause

  @retval
    FALSE  if there was some error
  @retval
    TRUE   if all is OK
*/

void add_join_on(TABLE_LIST *b, Item *expr)
{
  if (expr)
  {
    if (!b->on_expr)
      b->on_expr= expr;
    else
    {
      /*
        If called from the parser, this happens if you have both a
        right and left join. If called later, it happens if we add more
        than one condition to the ON clause.
      */
      b->on_expr= new Item_cond_and(b->on_expr,expr);
    }
    b->on_expr->top_level_item();
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
    either in b->on_expr, or in JOIN::conds, depending on whether there
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
  Reload/resets privileges and the different caches.

  @param thd Thread handler (can be NULL!)
  @param options What should be reset/reloaded (tables, privileges, slave...)
  @param tables Tables to flush (if any)
  @param write_to_binlog True if we can write to the binlog.
               
  @note Depending on 'options', it may be very bad to write the
    query to the binlog (e.g. FLUSH SLAVE); this is a
    pointer where reload_acl_and_cache() will put 0 if
    it thinks we really should not write to the binlog.
    Otherwise it will put 1.

  @return Error status code
    @retval 0 Ok
    @retval !=0  Error; thd->killed is set or thd->is_error() is true
*/

bool reload_acl_and_cache(THD *thd, ulong options, TABLE_LIST *tables,
                          bool *write_to_binlog)
{
  bool result=0;
  select_errors=0;				/* Write if more errors */
  bool tmp_write_to_binlog= 1;

  DBUG_ASSERT(!thd || !thd->in_sub_stmt);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (options & REFRESH_GRANT)
  {
    THD *tmp_thd= 0;
    /*
      If reload_acl_and_cache() is called from SIGHUP handler we have to
      allocate temporary THD for execution of acl_reload()/grant_reload().
    */
    if (!thd && (thd= (tmp_thd= new THD)))
    {
      thd->thread_stack= (char*) &tmp_thd;
      thd->store_globals();
      lex_start(thd);
    }
    
    if (thd)
    {
      bool reload_acl_failed= acl_reload(thd);
      bool reload_grants_failed= grant_reload(thd);
      bool reload_servers_failed= servers_reload(thd);
      
      if (reload_acl_failed || reload_grants_failed || reload_servers_failed)
      {
        result= 1;
        /*
          When an error is returned, my_message may have not been called and
          the client will hang waiting for a response.
        */
        my_error(ER_UNKNOWN_ERROR, MYF(0), "FLUSH PRIVILEGES failed");
      }
    }

    if (tmp_thd)
    {
      delete tmp_thd;
      /* Remember that we don't have a THD */
      my_pthread_setspecific_ptr(THR_THD,  0);
      thd= 0;
    }
    reset_mqh((LEX_USER *)NULL, TRUE);
  }
#endif
  if (options & REFRESH_LOG)
  {
    /*
      Flush the normal query log, the update log, the binary log,
      the slow query log, the relay log (if it exists) and the log
      tables.
    */

    /*
      Writing this command to the binlog may result in infinite loops
      when doing mysqlbinlog|mysql, and anyway it does not really make
      sense to log it automatically (would cause more trouble to users
      than it would help them)
    */
    tmp_write_to_binlog= 0;
    if( mysql_bin_log.is_open() )
    {
      mysql_bin_log.rotate_and_purge(RP_FORCE_ROTATE);
    }
#ifdef HAVE_REPLICATION
    pthread_mutex_lock(&LOCK_active_mi);
    rotate_relay_log(active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
#endif

    /* flush slow and general logs */
    logger.flush_logs(thd);

    if (ha_flush_logs(NULL))
      result=1;
    if (flush_error_log())
      result=1;
  }
#ifdef HAVE_QUERY_CACHE
  if (options & REFRESH_QUERY_CACHE_FREE)
  {
    query_cache.pack();				// FLUSH QUERY CACHE
    options &= ~REFRESH_QUERY_CACHE;    // Don't flush cache, just free memory
  }
  if (options & (REFRESH_TABLES | REFRESH_QUERY_CACHE))
  {
    query_cache.flush();			// RESET QUERY CACHE
  }
#endif /*HAVE_QUERY_CACHE*/
  /*
    Note that if REFRESH_READ_LOCK bit is set then REFRESH_TABLES is set too
    (see sql_yacc.yy)
  */
  if (options & (REFRESH_TABLES | REFRESH_READ_LOCK)) 
  {
    if ((options & REFRESH_READ_LOCK) && thd)
    {
      /*
        We must not try to aspire a global read lock if we have a write
        locked table. This would lead to a deadlock when trying to
        reopen (and re-lock) the table after the flush.
      */
      if (thd->locked_tables)
      {
        THR_LOCK_DATA **lock_p= thd->locked_tables->locks;
        THR_LOCK_DATA **end_p= lock_p + thd->locked_tables->lock_count;

        for (; lock_p < end_p; lock_p++)
        {
          if ((*lock_p)->type >= TL_WRITE_ALLOW_WRITE)
          {
            my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
            return 1;
          }
        }
      }
      /*
	Writing to the binlog could cause deadlocks, as we don't log
	UNLOCK TABLES
      */
      tmp_write_to_binlog= 0;
      if (lock_global_read_lock(thd))
	return 1;                               // Killed
      if (close_cached_tables(thd, tables, FALSE, (options & REFRESH_FAST) ?
                              FALSE : TRUE, TRUE))
          result= 1;
      
      if (make_global_read_lock_block_commit(thd)) // Killed
      {
        /* Don't leave things in a half-locked state */
        unlock_global_read_lock(thd);
        return 1;
      }
    }
    else
    {
      if (close_cached_tables(thd, tables, FALSE, (options & REFRESH_FAST) ?
                              FALSE : TRUE, FALSE))
        result= 1;
    }
    my_dbopt_cleanup();
  }
  if (options & REFRESH_HOSTS)
    hostname_cache_refresh();
  if (thd && (options & REFRESH_STATUS))
    refresh_status(thd);
  if (options & REFRESH_THREADS)
    flush_thread_cache();
#ifdef HAVE_REPLICATION
  if (options & REFRESH_MASTER)
  {
    DBUG_ASSERT(thd);
    tmp_write_to_binlog= 0;
    if (reset_master(thd))
    {
      result=1;
    }
  }
#endif
#ifdef OPENSSL
   if (options & REFRESH_DES_KEY_FILE)
   {
     if (des_key_file && load_des_key_file(des_key_file))
         result= 1;
   }
#endif
#ifdef HAVE_REPLICATION
 if (options & REFRESH_SLAVE)
 {
   tmp_write_to_binlog= 0;
   pthread_mutex_lock(&LOCK_active_mi);
   if (reset_slave(thd, active_mi))
     result=1;
   pthread_mutex_unlock(&LOCK_active_mi);
 }
#endif
 if (options & REFRESH_USER_RESOURCES)
   reset_mqh((LEX_USER *) NULL, 0);             /* purecov: inspected */
 *write_to_binlog= tmp_write_to_binlog;
 return result;
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
  THD *tmp;
  uint error=ER_NO_SUCH_THREAD;
  DBUG_ENTER("kill_one_thread");
  DBUG_PRINT("enter", ("id=%lu only_kill=%d", id, only_kill_query));
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->thread_id == id)
    {
      pthread_mutex_lock(&tmp->LOCK_thd_data);	// Lock from delete
      break;
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
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
      tmp->awake(only_kill_query ? THD::KILL_QUERY : THD::KILL_CONNECTION);
      error=0;
    }
    else
      error=ER_KILL_DENIED_ERROR;
    pthread_mutex_unlock(&tmp->LOCK_thd_data);
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

void sql_kill(THD *thd, ulong id, bool only_kill_query)
{
  uint error;
  if (!(error= kill_one_thread(thd, id, only_kill_query)))
    my_ok(thd);
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
	    min(lip->yylval->symbol.length, sizeof(command)-1));
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
                           &table->grant.privilege, 0, 1,
                           test(table->schema_table)) ||
              check_grant(thd, UPDATE_ACL, table, 0, 1, 1)) &&
             (check_access(thd, SELECT_ACL, table->db,
                           &table->grant.privilege, 0, 0,
                           test(table->schema_table)) ||
              check_grant(thd, SELECT_ACL, table, 0, 1, 0)))
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
			 &table->grant.privilege, 0, 0,
                         test(table->schema_table)) ||
	    check_grant(thd, SELECT_ACL, table, 0, 1, 0))
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
  TABLE_LIST *aux_tables=
    (TABLE_LIST *)thd->lex->auxiliary_table_list.first;
  TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
  DBUG_ENTER("multi_delete_precheck");

  /* sql_yacc guarantees that tables and aux_tables are not zero */
  DBUG_ASSERT(aux_tables != 0);
  if (check_table_access(thd, SELECT_ACL, tables, UINT_MAX, FALSE))
    DBUG_RETURN(TRUE);

  /*
    Since aux_tables list is not part of LEX::query_tables list we
    have to juggle with LEX::query_tables_own_last value to be able
    call check_table_access() safely.
  */
  thd->lex->query_tables_own_last= 0;
  if (check_table_access(thd, DELETE_ACL, aux_tables, UINT_MAX, FALSE))
  {
    thd->lex->query_tables_own_last= save_query_tables_own_last;
    DBUG_RETURN(TRUE);
  }
  thd->lex->query_tables_own_last= save_query_tables_own_last;

  if ((thd->options & OPTION_SAFE_UPDATES) && !select_lex->where)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
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
  TABLE_LIST *tables= (TABLE_LIST*)lex->select_lex.table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("multi_delete_set_locks_and_link_aux_tables");

  lex->table_count= 0;

  for (target_tbl= (TABLE_LIST *)lex->auxiliary_table_list.first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    lex->table_count++;
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk;
    for (walk= tables; walk; walk= walk->next_local)
    {
      if (!my_strcasecmp(table_alias_charset,
			 target_tbl->alias, walk->alias) &&
	  !strcmp(walk->db, target_tbl->db))
	break;
    }
    if (!walk)
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0),
               target_tbl->table_name, "MULTI DELETE");
      DBUG_RETURN(TRUE);
    }
    if (!walk->derived)
    {
      target_tbl->table_name= walk->table_name;
      target_tbl->table_name_length= walk->table_name_length;
    }
    walk->updating= target_tbl->updating;
    walk->lock_type= target_tbl->lock_type;
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
    @brief  Check privileges for SHOW CREATE TABLE statement.

    @param  thd    Thread context
    @param  table  Target table

    @retval TRUE  Failure
    @retval FALSE Success
*/

static bool check_show_create_table_access(THD *thd, TABLE_LIST *table)
{
  return check_access(thd, SELECT_ACL | EXTRA_ACL, table->db,
                      &table->grant.privilege, 0, 0,
                      test(table->schema_table)) ||
         check_grant(thd, SELECT_ACL, table, 2, UINT_MAX, 0);
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

  want_priv= ((lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) ?
              CREATE_TMP_ACL : CREATE_ACL) |
             (select_lex->item_list.elements ? INSERT_ACL : 0);

  if (check_access(thd, want_priv, create_table->db,
		   &create_table->grant.privilege, 0, 0,
                   test(create_table->schema_table)) ||
      check_merge_table_access(thd, create_table->db,
			       (TABLE_LIST *)
			       lex->create_info.merge_list.first))
    goto err;
  if (want_priv != CREATE_TMP_ACL &&
      check_grant(thd, want_priv, create_table, 0, 1, 0))
    goto err;

  if (select_lex->item_list.elements)
  {
    /* Check permissions for used tables in CREATE TABLE ... SELECT */

#ifdef NOT_NECESSARY_TO_CHECK_CREATE_TABLE_EXIST_WHEN_PREPARING_STATEMENT
    /* This code throws an ill error for CREATE TABLE t1 SELECT * FROM t1 */
    /*
      Only do the check for PS, because we on execute we have to check that
      against the opened tables to ensure we don't use a table that is part
      of the view (which can only be done after the table has been opened).
    */
    if (thd->stmt_arena->is_stmt_prepare_or_first_sp_execute())
    {
      /*
        For temporary tables we don't have to check if the created table exists
      */
      if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
          find_table_in_global_list(tables, create_table->db,
                                    create_table->table_name))
      {
	error= FALSE;
        goto err;
      }
    }
#endif
    if (tables && check_table_access(thd, SELECT_ACL, tables, UINT_MAX, FALSE))
      goto err;
  }
  else if (lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE)
  {
    if (check_show_create_table_access(thd, tables))
      goto err;
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
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
    return new Item_func_ne(arg, new Item_int((char*) "0", 0, 1));
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

  get_default_definer(thd, definer);

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
    return create_default_definer(thd);

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
                              uint max_char_length, CHARSET_INFO *cs,
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

  MYSQL_QUERY_PARSE_START(thd->query);
  /* Backup creation context. */

  Object_creation_ctx *backup_ctx= NULL;

  if (creation_ctx)
    backup_ctx= creation_ctx->set_n_backup(thd);

  /* Set parser state. */

  thd->m_parser_state= parser_state;

  /* Parse the query. */

  bool mysql_parse_status= MYSQLparse(thd) != 0;

  /* Check that if MYSQLparse() failed, thd->is_error() is set. */

  DBUG_ASSERT(!mysql_parse_status ||
              (mysql_parse_status && thd->is_error()));

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

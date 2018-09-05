/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/sql_class.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <utility>

#include "binary_log_types.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/components/services/psi_error_bits.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_error.h"
#include "mysql/psi/mysql_ps.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys_err.h"  // EE_OUTOFMEMORY
#include "pfs_statement_provider.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h"
#include "sql/check_stack.h"
#include "sql/conn_handler/connection_handler_manager.h"  // Connection_handler_manager
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"  // Dictionary_client
#include "sql/dd/dd_kill_immunizer.h"        // dd:DD_kill_immunizer
#include "sql/debug_sync.h"                  // DEBUG_SYNC
#include "sql/derror.h"                      // ER_THD
#include "sql/error_handler.h"               // Internal_error_handler
#include "sql/item_func.h"                   // user_var_entry
#include "sql/lock.h"                        // mysql_lock_abort_for_thread
#include "sql/locking_service.h"  // release_all_locking_service_locks
#include "sql/log_event.h"
#include "sql/mdl_context_backup.h"  // MDL context backup for XA
#include "sql/mysqld.h"              // global_system_variables ...
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/parse_location.h"
#include "sql/psi_memory_key.h"
#include "sql/query_result.h"
#include "sql/rpl_rli.h"    // Relay_log_info
#include "sql/rpl_slave.h"  // rpl_master_erroneous_autoinc
#include "sql/rpl_transaction_write_set_ctx.h"
#include "sql/sp_cache.h"         // sp_cache_clear
#include "sql/sql_audit.h"        // mysql_audit_free_thd
#include "sql/sql_backup_lock.h"  // release_backup_lock
#include "sql/sql_base.h"         // close_temporary_tables
#include "sql/sql_callback.h"     // MYSQL_CALLBACK
#include "sql/sql_handler.h"      // mysql_ha_cleanup
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"    // is_update_query
#include "sql/sql_plugin.h"   // plugin_thdvar_init
#include "sql/sql_prepare.h"  // Prepared_statement
#include "sql/sql_profile.h"
#include "sql/sql_time.h"   // my_timeval_trunc
#include "sql/sql_timer.h"  // thd_timer_destroy
#include "sql/table.h"
#include "sql/tc_log.h"
#include "sql/thr_malloc.h"
#include "sql/transaction.h"  // trans_rollback
#include "sql/transaction_info.h"
#include "sql/xa.h"
#include "thr_mutex.h"

using std::max;
using std::min;
using std::unique_ptr;

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char empty_c_string[1] = {0}; /* used for not defined db */

LEX_STRING EMPTY_STR = {(char *)"", 0};
LEX_STRING NULL_STR = {NULL, 0};
LEX_CSTRING EMPTY_CSTR = {"", 0};
LEX_CSTRING NULL_CSTR = {NULL, 0};

const char *const THD::DEFAULT_WHERE = "field list";
extern PSI_stage_info stage_waiting_for_disk_space;

void THD::Transaction_state::backup(THD *thd) {
  this->m_sql_command = thd->lex->sql_command;
  this->m_trx = thd->get_transaction();

  thd->backup_ha_data(&this->m_ha_data);

  this->m_tx_isolation = thd->tx_isolation;
  this->m_tx_read_only = thd->tx_read_only;
  this->m_thd_option_bits = thd->variables.option_bits;
  this->m_sql_mode = thd->variables.sql_mode;
  this->m_transaction_psi = thd->m_transaction_psi;
  this->m_server_status = thd->server_status;
  this->m_in_lock_tables = thd->in_lock_tables;
  this->m_time_zone_used = thd->time_zone_used;
  this->m_transaction_rollback_request = thd->transaction_rollback_request;
}

void THD::Transaction_state::restore(THD *thd) {
  thd->set_transaction(this->m_trx);

  thd->restore_ha_data(this->m_ha_data);

  thd->tx_isolation = this->m_tx_isolation;
  thd->variables.sql_mode = this->m_sql_mode;
  thd->tx_read_only = this->m_tx_read_only;
  thd->variables.option_bits = this->m_thd_option_bits;

  thd->m_transaction_psi = this->m_transaction_psi;
  thd->server_status = this->m_server_status;
  thd->lex->sql_command = this->m_sql_command;
  thd->in_lock_tables = this->m_in_lock_tables;
  thd->time_zone_used = this->m_time_zone_used;
  thd->transaction_rollback_request = this->m_transaction_rollback_request;
}

THD::Attachable_trx::Attachable_trx(THD *thd, Attachable_trx *prev_trx)
    : m_thd(thd),
      m_reset_lex(RESET_LEX),
      m_prev_attachable_trx(prev_trx),
      m_trx_state() {
  // Save the transaction state.

  m_trx_state.backup(m_thd);

  // Save and reset query-tables-list and reset the sql-command.
  //
  // NOTE: ha_innobase::store_lock() takes the current sql-command into account.
  // It must be SQLCOM_SELECT.
  //
  // Do NOT reset LEX if we're running tests. LEX is used by SELECT statements.

  bool reset = (m_reset_lex == RESET_LEX ? true : false);
  if (DBUG_EVALUATE_IF("use_attachable_trx", false, reset)) {
    m_thd->lex->reset_n_backup_query_tables_list(
        m_trx_state.m_query_tables_list);
    m_thd->lex->sql_command = SQLCOM_SELECT;
  }

  // Save and reset open-tables.

  m_thd->reset_n_backup_open_tables_state(&m_trx_state.m_open_tables_state,
                                          Open_tables_state::SYSTEM_TABLES);

  // Reset transaction state.

  m_thd->m_transaction.release();  // it's been backed up.
  m_thd->m_transaction.reset(new Transaction_ctx());

  // Prepare for a new attachable transaction for read-only DD-transaction.

  // LOCK_thd_data must be locked to prevent e.g. KILL CONNECTION from
  // reading ha_data after clear() but before resize().
  mysql_mutex_lock(&m_thd->LOCK_thd_data);
  m_thd->ha_data.clear();
  m_thd->ha_data.resize(m_thd->ha_data.capacity());
  mysql_mutex_unlock(&m_thd->LOCK_thd_data);

  // The attachable transaction must used READ COMMITTED isolation level.

  m_thd->tx_isolation = ISO_READ_COMMITTED;

  // The attachable transaction must be read-only.

  m_thd->tx_read_only = true;

  // The attachable transaction must be AUTOCOMMIT.

  m_thd->variables.option_bits |= OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits &= ~OPTION_NOT_AUTOCOMMIT;
  m_thd->variables.option_bits &= ~OPTION_BEGIN;

  // Nothing should be binlogged from attachable transactions and disabling
  // the binary log allows skipping some code related to figuring out what
  // log format should be used.
  m_thd->variables.option_bits &= ~OPTION_BIN_LOG;

  // Possible parent's involvement to multi-statement transaction is masked

  m_thd->server_status &= ~SERVER_STATUS_IN_TRANS;
  m_thd->server_status &= ~SERVER_STATUS_IN_TRANS_READONLY;

  // Reset SQL_MODE during system operations.

  m_thd->variables.sql_mode = 0;

  // Reset transaction instrumentation.

  m_thd->m_transaction_psi = NULL;

  // Reset THD::in_lock_tables so InnoDB won't start acquiring table locks.
  m_thd->in_lock_tables = false;

  // Reset @@session.time_zone usage indicator for consistency.
  m_thd->time_zone_used = false;

  /*
    InnoDB can ask to start attachable transaction while rolling back
    the regular transaction. Reset rollback request flag to avoid it
    influencing attachable transaction we are initiating.
  */
  m_thd->transaction_rollback_request = false;
}

THD::Attachable_trx::~Attachable_trx() {
  // Ensure that the SE didn't request rollback in the attachable transaction.
  // Having THD::transaction_rollback_request set most likely means that we've
  // experienced some sort of deadlock/timeout while processing the attachable
  // transaction. That is not possible by the definition of an attachable
  // transaction.
  DBUG_ASSERT(!m_thd->transaction_rollback_request);

  // Commit the attachable transaction before discarding transaction state.
  // This is mostly needed to properly reset transaction state in SE.
  // Note: We can't rely on InnoDB hack which auto-magically commits InnoDB
  // transaction when the last table for a statement in auto-commit mode is
  // unlocked. Apparently it doesn't work correctly in some corner cases
  // (for example, when statement is killed just after tables are locked but
  // before any other operations on the table happes). We try not to rely on
  // it in other places on SQL-layer as well.
  trans_commit_attachable(m_thd);

  // Close all the tables that are open till now.

  close_thread_tables(m_thd);

  // Cleanup connection specific state which was created for attachable
  // transaction (for InnoDB removes cached transaction object).
  //
  // Note that we need to call handlerton::close_connection for all SEs
  // and not only SEs which participated in attachable transaction since
  // connection specific state can be created when TABLE object is simply
  // expelled from the Table Cache (e.g. this happens for MyISAM).
  ha_close_connection(m_thd);

  // Restore the transaction state.

  m_trx_state.restore(m_thd);

  m_thd->restore_backup_open_tables_state(&m_trx_state.m_open_tables_state);

  bool reset = (m_reset_lex == RESET_LEX ? true : false);
  if (DBUG_EVALUATE_IF("use_attachable_trx", false, reset)) {
    m_thd->lex->restore_backup_query_tables_list(
        m_trx_state.m_query_tables_list);
  }
}

THD::Attachable_trx_rw::Attachable_trx_rw(THD *thd)
    : Attachable_trx(thd, nullptr) {
  m_thd->tx_read_only = false;
  m_thd->lex->sql_command = SQLCOM_END;
  thd->get_transaction()->xid_state()->set_state(XID_STATE::XA_NOTR);
}

void THD::enter_stage(const PSI_stage_info *new_stage,
                      PSI_stage_info *old_stage,
                      const char *calling_func MY_ATTRIBUTE((unused)),
                      const char *calling_file,
                      const unsigned int calling_line) {
  DBUG_PRINT("THD::enter_stage",
             ("'%s' %s:%d", new_stage ? new_stage->m_name : "", calling_file,
              calling_line));

  if (old_stage != NULL) {
    old_stage->m_key = m_current_stage_key;
    old_stage->m_name = proc_info;
  }

  if (new_stage != NULL) {
    const char *msg = new_stage->m_name;

#if defined(ENABLED_PROFILING)
    profiling->status_change(msg, calling_func, calling_file, calling_line);
#endif

    m_current_stage_key = new_stage->m_key;
    proc_info = msg;

    m_stage_progress_psi =
        MYSQL_SET_STAGE(m_current_stage_key, calling_file, calling_line);
  } else {
    m_stage_progress_psi = NULL;
  }

  return;
}

void Open_tables_state::set_open_tables_state(Open_tables_state *state) {
  this->open_tables = state->open_tables;

  this->temporary_tables = state->temporary_tables;

  this->lock = state->lock;
  this->extra_lock = state->extra_lock;

  this->locked_tables_mode = state->locked_tables_mode;

  this->state_flags = state->state_flags;

  this->m_reprepare_observers = state->m_reprepare_observers;
}

void Open_tables_state::reset_open_tables_state() {
  open_tables = NULL;
  temporary_tables = NULL;
  lock = NULL;
  extra_lock = NULL;
  locked_tables_mode = LTM_NONE;
  state_flags = 0U;
  reset_reprepare_observers();
}

THD::THD(bool enable_plugins)
    : Query_arena(&main_mem_root, STMT_REGULAR_EXECUTION),
      mark_used_columns(MARK_COLUMNS_READ),
      want_privilege(0),
      main_lex(new LEX),
      lex(main_lex.get()),
      m_dd_client(new dd::cache::Dictionary_client(this)),
      m_query_string(NULL_CSTR),
      m_db(NULL_CSTR),
      rli_fake(0),
      rli_slave(NULL),
      initial_status_var(NULL),
      status_var_aggregated(false),
      m_current_query_cost(0),
      m_current_query_partial_plans(0),
      query_plan(this),
      m_current_stage_key(0),
      current_mutex(NULL),
      current_cond(NULL),
      in_sub_stmt(0),
      fill_status_recursion_level(0),
      fill_variables_recursion_level(0),
      ha_data(PSI_NOT_INSTRUMENTED, ha_data.initial_capacity),
      binlog_row_event_extra_data(NULL),
      skip_readonly_check(false),
      binlog_unsafe_warning_flags(0),
      binlog_table_maps(0),
      binlog_accessed_db_names(NULL),
      m_trans_log_file(NULL),
      m_trans_fixed_log_file(NULL),
      m_trans_end_pos(0),
      m_transaction(new Transaction_ctx()),
      m_attachable_trx(NULL),
      table_map_for_update(0),
      m_examined_row_count(0),
#if defined(ENABLED_PROFILING)
      profiling(new PROFILING),
#endif
      m_stage_progress_psi(NULL),
      m_digest(NULL),
      m_statement_psi(NULL),
      m_transaction_psi(NULL),
      m_idle_psi(NULL),
      m_server_idle(false),
      user_var_events(key_memory_user_var_entry),
      next_to_commit(NULL),
      binlog_need_explicit_defaults_ts(false),
      kill_immunizer(NULL),
      m_is_fatal_error(false),
      transaction_rollback_request(0),
      is_fatal_sub_stmt_error(false),
      rand_used(0),
      time_zone_used(0),
      in_lock_tables(0),
      derived_tables_processing(false),
      parsing_system_view(false),
      sp_runtime_ctx(NULL),
      m_parser_state(NULL),
      work_part_info(NULL),
      // No need to instrument, highly unlikely to have that many plugins.
      audit_class_plugins(PSI_NOT_INSTRUMENTED),
      audit_class_mask(PSI_NOT_INSTRUMENTED),
#if defined(ENABLED_DEBUG_SYNC)
      debug_sync_control(0),
#endif /* defined(ENABLED_DEBUG_SYNC) */
      m_enable_plugins(enable_plugins),
#ifdef HAVE_GTID_NEXT_LIST
      owned_gtid_set(global_sid_map),
#endif
      skip_gtid_rollback(false),
      is_commit_in_middle_of_statement(false),
      has_gtid_consistency_violation(false),
      main_da(false),
      m_parser_da(false),
      m_query_rewrite_plugin_da(false),
      m_query_rewrite_plugin_da_ptr(&m_query_rewrite_plugin_da),
      m_stmt_da(&main_da),
      duplicate_slave_id(false),
      is_a_srv_session_thd(false),
      m_is_plugin_fake_ddl(false) {
  main_lex->reset();
  set_psi(NULL);
  mdl_context.init(this);
  init_sql_alloc(key_memory_thd_main_mem_root, &main_mem_root,
                 global_system_variables.query_alloc_block_size,
                 global_system_variables.query_prealloc_size);
  stmt_arena = this;
  thread_stack = 0;
  m_catalog.str = "std";
  m_catalog.length = 3;
  m_security_ctx = &m_main_security_ctx;
  password = 0;
  query_start_usec_used = false;
  check_for_truncated_fields = CHECK_FIELD_IGNORE;
  killed = NOT_KILLED;
  col_access = 0;
  is_slave_error = thread_specific_used = false;
  tmp_table = 0;
  num_truncated_fields = 0L;
  m_sent_row_count = 0L;
  current_found_rows = 0;
  previous_found_rows = 0;
  is_operating_gtid_table_implicitly = false;
  is_operating_substatement_implicitly = false;
  m_row_count_func = -1;
  statement_id_counter = 0UL;
  // Must be reset to handle error with THD's created for init of mysqld
  lex->thd = NULL;
  lex->set_current_select(0);
  utime_after_lock = 0L;
  current_linfo = 0;
  slave_thread = 0;
  memset(&variables, 0, sizeof(variables));
  m_thread_id = Global_THD_manager::reserved_thread_id;
  file_id = 0;
  query_id = 0;
  query_name_consts = 0;
  db_charset = global_system_variables.collation_database;
  is_killable = false;
  binlog_evt_union.do_union = false;
  enable_slow_log = 0;
  commit_error = CE_NONE;
  durability_property = HA_REGULAR_DURABILITY;
#ifndef DBUG_OFF
  dbug_sentry = THD_SENTRY_MAGIC;
#endif
  mysql_audit_init_thd(this);
  net.vio = 0;
  system_thread = NON_SYSTEM_THREAD;
  cleanup_done = 0;
  m_release_resources_done = false;
  peer_port = 0;  // For SHOW PROCESSLIST
  get_transaction()->m_flags.enabled = true;
  m_resource_group_ctx.m_cur_resource_group = nullptr;
  m_resource_group_ctx.m_switch_resource_group_str[0] = '\0';
  m_resource_group_ctx.m_warn = 0;

  mysql_mutex_init(key_LOCK_thd_data, &LOCK_thd_data, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_query, &LOCK_thd_query, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_sysvar, &LOCK_thd_sysvar, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_protocol, &LOCK_thd_protocol,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_query_plan, &LOCK_query_plan, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_current_cond, &LOCK_current_cond,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_thr_lock, &COND_thr_lock);

  /* Variables with default values */
  proc_info = "login";
  where = THD::DEFAULT_WHERE;
  server_id = ::server_id;
  unmasked_server_id = server_id;
  set_command(COM_CONNECT);
  *scramble = '\0';

  /* Call to init() below requires fully initialized Open_tables_state. */
  reset_open_tables_state();

  init();
#if defined(ENABLED_PROFILING)
  profiling->set_thd(this);
#endif
  m_user_connect = NULL;
  user_vars.clear();

  sp_proc_cache = NULL;
  sp_func_cache = NULL;

  /* Protocol */
  m_protocol = &protocol_text;  // Default protocol
  protocol_text.init(this);
  protocol_binary.init(this);
  protocol_text.set_client_capabilities(0);  // minimalistic client

  substitute_null_with_insert_id = false;

  /*
    Make sure thr_lock_info_init() is called for threads which do not get
    assigned a proper thread_id value but keep using reserved_thread_id.
  */
  thr_lock_info_init(&lock_info, m_thread_id, &COND_thr_lock);

  m_internal_handler = NULL;
  m_binlog_invoker = false;
  memset(&m_invoker_user, 0, sizeof(m_invoker_user));
  memset(&m_invoker_host, 0, sizeof(m_invoker_host));

  binlog_next_event_pos.file_name = NULL;
  binlog_next_event_pos.pos = 0;

  timer = NULL;
  timer_cache = NULL;

  m_token_array = NULL;
  if (max_digest_length > 0) {
    m_token_array = (unsigned char *)my_malloc(PSI_INSTRUMENT_ME,
                                               max_digest_length, MYF(MY_WME));
  }
#ifndef DBUG_OFF
  debug_binlog_xid_last.reset();
#endif
}

void THD::set_transaction(Transaction_ctx *transaction_ctx) {
  DBUG_ASSERT(is_attachable_ro_transaction_active());

  delete m_transaction.release();
  m_transaction.reset(transaction_ctx);
}

bool THD::set_db(const LEX_CSTRING &new_db) {
  bool result;
  /*
    Acquiring mutex LOCK_thd_data as we either free the memory allocated
    for the database and reallocating the memory for the new db or memcpy
    the new_db to the db.
  */
  mysql_mutex_lock(&LOCK_thd_data);
  /* Do not reallocate memory if current chunk is big enough. */
  if (m_db.str && new_db.str && m_db.length >= new_db.length)
    memcpy(const_cast<char *>(m_db.str), new_db.str, new_db.length + 1);
  else {
    my_free(const_cast<char *>(m_db.str));
    m_db = NULL_CSTR;
    if (new_db.str)
      m_db.str = my_strndup(key_memory_THD_db, new_db.str, new_db.length,
                            MYF(MY_WME | ME_FATALERROR));
  }
  m_db.length = m_db.str ? new_db.length : 0;
  mysql_mutex_unlock(&LOCK_thd_data);
  result = new_db.str && !m_db.str;
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (!result)
    PSI_THREAD_CALL(set_thread_db)(new_db.str, static_cast<int>(new_db.length));
#endif
  return result;
}

void THD::push_internal_handler(Internal_error_handler *handler) {
  if (m_internal_handler) {
    handler->m_prev_internal_handler = m_internal_handler;
    m_internal_handler = handler;
  } else
    m_internal_handler = handler;
}

bool THD::handle_condition(uint sql_errno, const char *sqlstate,
                           Sql_condition::enum_severity_level *level,
                           const char *msg) {
  if (!m_internal_handler) return false;

  for (Internal_error_handler *error_handler = m_internal_handler;
       error_handler; error_handler = error_handler->m_prev_internal_handler) {
    if (error_handler->handle_condition(this, sql_errno, sqlstate, level, msg))
      return true;
  }
  return false;
}

Internal_error_handler *THD::pop_internal_handler() {
  DBUG_ASSERT(m_internal_handler != NULL);
  Internal_error_handler *popped_handler = m_internal_handler;
  m_internal_handler = m_internal_handler->m_prev_internal_handler;
  return popped_handler;
}

void THD::raise_error(uint sql_errno) {
  const char *msg = ER_THD(this, sql_errno);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_ERROR, msg);
}

void THD::raise_error_printf(uint sql_errno, ...) {
  va_list args;
  char ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_error_printf");
  DBUG_PRINT("my", ("nr: %d  errno: %d", sql_errno, errno));
  const char *format = ER_THD(this, sql_errno);
  va_start(args, sql_errno);
  vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_ERROR, ebuff);
  DBUG_VOID_RETURN;
}

void THD::raise_warning(uint sql_errno) {
  const char *msg = ER_THD(this, sql_errno);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_WARNING, msg);
}

void THD::raise_warning_printf(uint sql_errno, ...) {
  va_list args;
  char ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_warning_printf");
  DBUG_PRINT("enter", ("warning: %u", sql_errno));
  const char *format = ER_THD(this, sql_errno);
  va_start(args, sql_errno);
  vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_WARNING, ebuff);
  DBUG_VOID_RETURN;
}

void THD::raise_note(uint sql_errno) {
  DBUG_ENTER("THD::raise_note");
  DBUG_PRINT("enter", ("code: %d", sql_errno));
  if (!(variables.option_bits & OPTION_SQL_NOTES)) DBUG_VOID_RETURN;
  const char *msg = ER_THD(this, sql_errno);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_NOTE, msg);
  DBUG_VOID_RETURN;
}

void THD::raise_note_printf(uint sql_errno, ...) {
  va_list args;
  char ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_note_printf");
  DBUG_PRINT("enter", ("code: %u", sql_errno));
  if (!(variables.option_bits & OPTION_SQL_NOTES)) DBUG_VOID_RETURN;
  const char *format = ER_THD(this, sql_errno);
  va_start(args, sql_errno);
  vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void)raise_condition(sql_errno, NULL, Sql_condition::SL_NOTE, ebuff);
  DBUG_VOID_RETURN;
}

struct timeval THD::query_start_timeval_trunc(uint decimals) {
  struct timeval tv;
  tv.tv_sec = start_time.tv_sec;
  if (decimals) {
    tv.tv_usec = start_time.tv_usec;
    my_timeval_trunc(&tv, decimals);
    query_start_usec_used = true;
  } else {
    tv.tv_usec = 0;
  }
  return tv;
}

Sql_condition *THD::raise_condition(uint sql_errno, const char *sqlstate,
                                    Sql_condition::enum_severity_level level,
                                    const char *msg, bool fatal_error) {
  DBUG_ENTER("THD::raise_condition");

  if (!(variables.option_bits & OPTION_SQL_NOTES) &&
      (level == Sql_condition::SL_NOTE))
    DBUG_RETURN(NULL);

  DBUG_ASSERT(sql_errno != 0);
  if (sql_errno == 0) /* Safety in release build */
    sql_errno = ER_UNKNOWN_ERROR;
  if (msg == NULL) msg = ER_THD(this, sql_errno);
  if (sqlstate == NULL) sqlstate = mysql_errno_to_sqlstate(sql_errno);

  if (fatal_error) {
    // Only makes sense for errors
    DBUG_ASSERT(level == Sql_condition::SL_ERROR);
    this->fatal_error();
  }

  MYSQL_LOG_ERROR(sql_errno, PSI_ERROR_OPERATION_RAISED);
  if (handle_condition(sql_errno, sqlstate, &level, msg)) DBUG_RETURN(NULL);

  Diagnostics_area *da = get_stmt_da();
  if (level == Sql_condition::SL_ERROR) {
    /*
      Reporting an error invokes audit API call that notifies the error
      to the plugin. Audit API that generate the error adds a protection
      (condition handler) that prevents entering infinite recursion, when
      a plugin signals error, when already handling the error.

      mysql_audit_notify() must therefore be called after handle_condition().
    */
    mysql_audit_notify(this, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_ERROR), sql_errno,
                       msg, strlen(msg));

    is_slave_error = true;  // needed to catch query errors during replication

    if (!da->is_error()) {
      set_row_count_func(-1);
      da->set_error_status(sql_errno, msg, sqlstate);
    }
  }

  /*
    Avoid pushing a condition for fatal out of memory errors as this will
    require memory allocation and therefore might fail. Non fatal out of
    memory errors can occur if raised by SIGNAL/RESIGNAL statement.
  */
  Sql_condition *cond = NULL;
  if (!(is_fatal_error() &&
        (sql_errno == EE_OUTOFMEMORY || sql_errno == ER_OUTOFMEMORY ||
         sql_errno == ER_STD_BAD_ALLOC_ERROR))) {
    cond = da->push_warning(this, sql_errno, sqlstate, level, msg);
  }
  DBUG_RETURN(cond);
}

/*
  Init common variables that has to be reset on start and on cleanup_connection
*/

void THD::init(void) {
  plugin_thdvar_init(this, m_enable_plugins);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id = m_thread_id;

  /*
    NOTE: reset_connection command will reset the THD to its default state.
    All system variables whose scope is SESSION ONLY should be set to their
    default values here.
  */
  reset_first_successful_insert_id();
  user_time.tv_sec = user_time.tv_usec = 0;
  start_time.tv_sec = start_time.tv_usec = 0;
  set_time();
  auto_inc_intervals_forced.empty();
  {
    ulong tmp;
    tmp = sql_rnd_with_mutex();
    randominit(&rand, tmp + (ulong)&rand,
               tmp + (ulong)::atomic_global_query_id);
  }

  server_status = SERVER_STATUS_AUTOCOMMIT;
  if (variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
    server_status |= SERVER_STATUS_NO_BACKSLASH_ESCAPES;

  get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);
  get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::STMT);
  open_options = ha_open_options;
  update_lock_default =
      (variables.low_priority_updates ? TL_WRITE_LOW_PRIORITY : TL_WRITE);
  insert_lock_default =
      (variables.low_priority_updates ? TL_WRITE_LOW_PRIORITY
                                      : TL_WRITE_CONCURRENT_INSERT);
  tx_isolation = (enum_tx_isolation)variables.transaction_isolation;
  tx_read_only = variables.transaction_read_only;
  tx_priority = 0;
  thd_tx_priority = 0;
  update_charset();
  reset_current_stmt_binlog_format_row();
  reset_binlog_local_stmt_filter();
  memset(&status_var, 0, sizeof(status_var));
  binlog_row_event_extra_data = 0;

  if (variables.sql_log_bin)
    variables.option_bits |= OPTION_BIN_LOG;
  else
    variables.option_bits &= ~OPTION_BIN_LOG;

#if defined(ENABLED_DEBUG_SYNC)
  /* Initialize the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_init_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */

  /* Initialize session_tracker and create all tracker objects */
  session_tracker.init(this->charset());
  session_tracker.enable(this);

  owned_gtid.clear();
  owned_sid.clear();
  owned_gtid.dbug_print(NULL, "set owned_gtid (clear) in THD::init");

  // This will clear the writeset session history.
  rpl_thd_ctx.dependency_tracker_ctx().set_last_session_sequence_number(0);
}

void THD::init_query_mem_roots() {
  reset_root_defaults(mem_root, variables.query_alloc_block_size,
                      variables.query_prealloc_size);
  get_transaction()->init_mem_root_defaults(variables.trans_alloc_block_size,
                                            variables.trans_prealloc_size);
}

void THD::set_new_thread_id() {
  m_thread_id = Global_THD_manager::get_instance()->get_new_thread_id();
  variables.pseudo_thread_id = m_thread_id;
  thr_lock_info_init(&lock_info, m_thread_id, &COND_thr_lock);
}

/*
  Do what's needed when one invokes change user

  SYNOPSIS
    cleanup_connection()

  IMPLEMENTATION
    Reset all resources that are connection specific
*/

void THD::cleanup_connection(void) {
  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &status_var, true);
  mysql_mutex_unlock(&LOCK_status);

  cleanup();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_end_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */
  killed = NOT_KILLED;
  cleanup_done = 0;
  init();
  stmt_map.reset();
  user_vars.clear();
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

  clear_error();
  // clear the warnings
  get_stmt_da()->reset_condition_info(this);
  // clear profiling information
#if defined(ENABLED_PROFILING)
  profiling->cleanup();
#endif

#ifndef DBUG_OFF
  /* DEBUG code only (begin) */
  bool check_cleanup = false;
  DBUG_EXECUTE_IF("debug_test_cleanup_connection", check_cleanup = true;);
  if (check_cleanup) {
    /* isolation level should be default */
    DBUG_ASSERT(variables.transaction_isolation == ISO_REPEATABLE_READ);
    /* check autocommit is ON by default */
    DBUG_ASSERT(server_status == SERVER_STATUS_AUTOCOMMIT);
    /* check prepared stmts are cleaned up */
    DBUG_ASSERT(prepared_stmt_count == 0);
    /* check diagnostic area is cleaned up */
    DBUG_ASSERT(get_stmt_da()->status() == Diagnostics_area::DA_EMPTY);
    /* check if temp tables are deleted */
    DBUG_ASSERT(temporary_tables == NULL);
    /* check if tables are unlocked */
    DBUG_ASSERT(locked_tables_list.locked_tables() == NULL);
  }
    /* DEBUG code only (end) */
#endif
}

/*
  Do what's needed when one invokes change user.
  Also used during THD::release_resources, i.e. prior to THD destruction.
*/
void THD::cleanup(void) {
  Transaction_ctx *trn_ctx = get_transaction();
  XID_STATE *xs = trn_ctx->xid_state();

  DBUG_ENTER("THD::cleanup");
  DBUG_ASSERT(cleanup_done == 0);
  DEBUG_SYNC(this, "thd_cleanup_start");

  killed = KILL_CONNECTION;
  if (trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED)) {
    /*
      Return error is not an option as XA is in prepared state and
      connection is gone. Log the error and continue.
    */
    if (MDL_context_backup_manager::instance().create_backup(
            &mdl_context, xs->get_xid()->key(), xs->get_xid()->key_length())) {
      LogErr(ERROR_LEVEL, ER_XA_CANT_CREATE_MDL_BACKUP);
    }
    transaction_cache_detach(trn_ctx);
  } else {
    xs->set_state(XID_STATE::XA_NOTR);
    trans_rollback(this);
    transaction_cache_delete(trn_ctx);
  }

  locked_tables_list.unlock_locked_tables(this);
  mysql_ha_cleanup(this);

  DBUG_ASSERT(open_tables == NULL);
  /*
    If the thread was in the middle of an ongoing transaction (rolled
    back a few lines above) or under LOCK TABLES (unlocked the tables
    and left the mode a few lines above), there will be outstanding
    metadata locks. Release them.
  */
  mdl_context.release_transactional_locks();

  /* Release the global read lock, if acquired. */
  if (global_read_lock.is_acquired())
    global_read_lock.unlock_global_read_lock(this);

  mysql_ull_cleanup(this);
  /*
    All locking service locks must be released on disconnect.
  */
  release_all_locking_service_locks(this);

  /*
    If Backup Lock was acquired it must be released on disconnect.
  */
  release_backup_lock(this);

  /* All metadata locks must have been released by now. */
  DBUG_ASSERT(!mdl_context.has_locks());

  /* Protects user_vars. */
  mysql_mutex_lock(&LOCK_thd_data);
  user_vars.clear();
  mysql_mutex_unlock(&LOCK_thd_data);

  /*
    When we call drop table for temporary tables, the
    user_var_events container is not cleared this might
    cause error if the container was filled before the
    drop table command is called.
    So call this before calling close_temporary_tables.
  */
  user_var_events.clear();
  close_temporary_tables(this);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

  /*
    Actions above might generate events for the binary log, so we
    commit the current transaction coordinator after executing cleanup
    actions.
   */
  if (tc_log && !trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED))
    tc_log->commit(this, true);

  /*
    Destroy trackers only after finishing manipulations with transaction
    state to avoid issues with Transaction_state_tracker.
  */
  session_tracker.deinit();

  /*
    If we have a Security_context, make sure it is "logged out"
  */

  cleanup_done = 1;
  DBUG_VOID_RETURN;
}

/**
  Release most resources, prior to THD destruction.
 */
void THD::release_resources() {
  DBUG_ASSERT(m_release_resources_done == false);

  Global_THD_manager::get_instance()->release_thread_id(m_thread_id);

  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &status_var, false);
  /*
    Status queries after this point should not aggregate THD::status_var
    since the values has been added to global_status_var.
    The status values are not reset so that they can still be read
    by performance schema.
  */
  status_var_aggregated = true;

  mysql_mutex_unlock(&LOCK_status);

  /* Ensure that no one is using THD */
  mysql_mutex_lock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_query_plan);

  /* Close connection */
  if (is_classic_protocol() && get_protocol_classic()->get_vio()) {
    vio_delete(get_protocol_classic()->get_vio());
    get_protocol_classic()->end_net();
  }

  /* modification plan for UPDATE/DELETE should be freed. */
  DBUG_ASSERT(query_plan.get_modification_plan() == NULL);
  mysql_mutex_unlock(&LOCK_query_plan);
  mysql_mutex_unlock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_thd_query);
  mysql_mutex_unlock(&LOCK_thd_query);

  stmt_map.reset(); /* close all prepared statements */
  if (!cleanup_done) cleanup();

  mdl_context.destroy();
  ha_close_connection(this);

  /*
    Debug sync system must be closed after ha_close_connection, because
    DEBUG_SYNC is used in InnoDB connection handlerton close.
  */
#if defined(ENABLED_DEBUG_SYNC)
  /* End the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_end_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */

  plugin_thdvar_cleanup(this, m_enable_plugins);

  DBUG_ASSERT(timer == NULL);

  if (timer_cache) thd_timer_destroy(timer_cache);

  if (rli_fake) {
    rli_fake->end_info();
    delete rli_fake;
    rli_fake = NULL;
  }
  mysql_audit_free_thd(this);

  if (current_thd == this) restore_globals();

  m_release_resources_done = true;
}

THD::~THD() {
  THD_CHECK_SENTRY(this);
  DBUG_ENTER("~THD()");
  DBUG_PRINT("info", ("THD dtor, this %p", this));

  if (!m_release_resources_done) release_resources();

  clear_next_event_pos();

  /* Ensure that no one is using THD */
  mysql_mutex_lock(&LOCK_thd_data);
  mysql_mutex_unlock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_thd_query);
  mysql_mutex_unlock(&LOCK_thd_query);

  DBUG_ASSERT(!m_attachable_trx);

  my_free(const_cast<char *>(m_db.str));
  m_db = NULL_CSTR;
  get_transaction()->free_memory(MYF(0));
  mysql_mutex_destroy(&LOCK_query_plan);
  mysql_mutex_destroy(&LOCK_thd_data);
  mysql_mutex_destroy(&LOCK_thd_query);
  mysql_mutex_destroy(&LOCK_thd_sysvar);
  mysql_mutex_destroy(&LOCK_thd_protocol);
  mysql_mutex_destroy(&LOCK_current_cond);
  mysql_cond_destroy(&COND_thr_lock);
#ifndef DBUG_OFF
  dbug_sentry = THD_SENTRY_GONE;
#endif

  if (variables.gtid_next_list.gtid_set != NULL) {
#ifdef HAVE_GTID_NEXT_LIST
    delete variables.gtid_next_list.gtid_set;
    variables.gtid_next_list.gtid_set = NULL;
    variables.gtid_next_list.is_non_null = false;
#else
    DBUG_ASSERT(0);
#endif
  }
  if (rli_slave) rli_slave->cleanup_after_session();

  free_root(&main_mem_root, MYF(0));

  if (m_token_array != NULL) {
    my_free(m_token_array);
  }
  DBUG_VOID_RETURN;
}

/**
  Awake a thread.

  @param[in]  state_to_set    value for THD::killed

  This is normally called from another thread's THD object.

  @note Do always call this while holding LOCK_thd_data.
*/

void THD::awake(THD::killed_state state_to_set) {
  DBUG_ENTER("THD::awake");
  DBUG_PRINT("enter", ("this: %p current_thd: %p", this, current_thd));
  THD_CHECK_SENTRY(this);
  mysql_mutex_assert_owner(&LOCK_thd_data);

  /* Shutdown clone vio always, to wake up clone waiting for remote. */
  shutdown_clone_vio();

  /*
    If THD is in kill immune mode (i.e. operation on new DD tables is in
    progress) then just save state_to_set with THD::kill_immunizer object.

    While exiting kill immune mode, awake() is called again with the killed
    state saved in THD::kill_immunizer object.
  */
  if (kill_immunizer && kill_immunizer->is_active()) {
    kill_immunizer->save_killed_state(state_to_set);
    DBUG_VOID_RETURN;
  }

  /*
    Set killed flag if the connection is being killed (state_to_set
    is KILL_CONNECTION) or the connection is processing a query
    (state_to_set is KILL_QUERY and m_server_idle flag is not set).
    If the connection is idle and state_to_set is KILL QUERY, the
    the killed flag is not set so that it doesn't affect the next
    command incorrectly.
  */
  if (this->m_server_idle && state_to_set == KILL_QUERY) { /* nothing */
  } else {
    killed = state_to_set;
  }

  if (state_to_set != THD::KILL_QUERY && state_to_set != THD::KILL_TIMEOUT) {
    if (this != current_thd || kill_immunizer) {
      DBUG_ASSERT(!kill_immunizer || !kill_immunizer->is_active());

      /*
        Before sending a signal, let's close the socket of the thread
        that is being killed ("this", which is not the current thread).
        This is to make sure it does not block if the signal is lost.
        This needs to be done only on platforms where signals are not
        a reliable interruption mechanism.

        Note that the downside of this mechanism is that we could close
        the connection while "this" target thread is in the middle of
        sending a result to the application, thus violating the client-
        server protocol.

        On the other hand, without closing the socket we have a race
        condition. If "this" target thread passes the check of
        thd->killed, and then the current thread runs through
        THD::awake(), sets the 'killed' flag and completes the
        signaling, and then the target thread runs into read(), it will
        block on the socket. As a result of the discussions around
        Bug#37780, it has been decided that we accept the race
        condition. A second KILL awakes the target from read().

        If we are killing ourselves, we know that we are not blocked.
        We also know that we will check thd->killed before we go for
        reading the next statement.
      */

      shutdown_active_vio();
    }

    /* Send an event to the scheduler that a thread should be killed. */
    if (!slave_thread)
      MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                     post_kill_notification, (this));
  }

  /* Interrupt target waiting inside a storage engine. */
  if (state_to_set != THD::NOT_KILLED) ha_kill_connection(this);

  if (state_to_set == THD::KILL_TIMEOUT)
    status_var.max_execution_time_exceeded++;

  /* Broadcast a condition to kick the target if it is waiting on it. */
  if (is_killable) {
    mysql_mutex_lock(&LOCK_current_cond);
    /*
      This broadcast could be up in the air if the victim thread
      exits the cond in the time between read and broadcast, but that is
      ok since all we want to do is to make the victim thread get out
      of waiting on current_cond.
      If we see a non-zero current_cond: it cannot be an old value (because
      then exit_cond() should have run and it can't because we have mutex); so
      it is the true value but maybe current_mutex is not yet non-zero (we're
      in the middle of enter_cond() and there is a "memory order
      inversion"). So we test the mutex too to not lock 0.

      Note that there is a small chance we fail to kill. If victim has locked
      current_mutex, but hasn't yet entered enter_cond() (which means that
      current_cond and current_mutex are 0), then the victim will not get
      a signal and it may wait "forever" on the cond (until
      we issue a second KILL or the status it's waiting for happens).
      It's true that we have set its thd->killed but it may not
      see it immediately and so may have time to reach the cond_wait().

      However, where possible, we test for killed once again after
      enter_cond(). This should make the signaling as safe as possible.
      However, there is still a small chance of failure on platforms with
      instruction or memory write reordering.
    */
    if (current_cond.load() && current_mutex.load()) {
      DBUG_EXECUTE_IF("before_dump_thread_acquires_current_mutex", {
        const char act[] =
            "now signal dump_thread_signal wait_for go_dump_thread";
        DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      };);
      mysql_mutex_lock(current_mutex);
      mysql_cond_broadcast(current_cond);
      mysql_mutex_unlock(current_mutex);
    }
    mysql_mutex_unlock(&LOCK_current_cond);
  }
  DBUG_VOID_RETURN;
}

/**
  Close the Vio associated this session.

  @remark LOCK_thd_data is taken due to the fact that
          the Vio might be disassociated concurrently.
*/

void THD::disconnect(bool server_shutdown) {
  Vio *vio = NULL;

  mysql_mutex_lock(&LOCK_thd_data);

  /* Shutdown clone vio always, to wake up clone waiting for remote. */
  shutdown_clone_vio();

  /*
    If thread is in kill immune mode (i.e. operation on new DD tables
    is in progress) then just save state_to_set with THD::kill_immunizer
    object.

    While exiting kill immune mode, awake() is called again with the killed
    state saved in THD::kill_immunizer object.

    active_vio is aleady associated to the thread when it is in the kill
    immune mode. THD::awake() closes the active_vio.
   */
  if (kill_immunizer != nullptr)
    kill_immunizer->save_killed_state(THD::KILL_CONNECTION);
  else {
    killed = THD::KILL_CONNECTION;

    /*
      Since a active vio might might have not been set yet, in
      any case save a reference to avoid closing a inexistent
      one or closing the vio twice if there is a active one.
    */
    vio = active_vio;
    shutdown_active_vio();

    /* Disconnect even if a active vio is not associated. */
    if (is_classic_protocol() && get_protocol_classic()->get_vio() != vio &&
        get_protocol_classic()->connection_alive()) {
      m_protocol->shutdown(server_shutdown);
    }
  }

  mysql_mutex_unlock(&LOCK_thd_data);
}

void THD::notify_shared_lock(MDL_context_owner *ctx_in_use,
                             bool needs_thr_lock_abort) {
  THD *in_use = ctx_in_use->get_thd();

  if (needs_thr_lock_abort) {
    mysql_mutex_lock(&in_use->LOCK_thd_data);
    for (TABLE *thd_table = in_use->open_tables; thd_table;
         thd_table = thd_table->next) {
      /*
        Check for TABLE::needs_reopen() is needed since in some places we call
        handler::close() for table instance (and set TABLE::db_stat to 0)
        and do not remove such instances from the THD::open_tables
        for some time, during which other thread can see those instances
        (e.g. see partitioning code).
      */
      if (!thd_table->needs_reopen())
        mysql_lock_abort_for_thread(this, thd_table);
    }
    mysql_mutex_unlock(&in_use->LOCK_thd_data);
  }
}

/*
  Remember the location of thread info, the structure needed for
  sql_alloc() and the structure for the net buffer
*/

void THD::store_globals() {
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  DBUG_ASSERT(thread_stack);

  current_thd = this;
  THR_MALLOC = &mem_root;
  /*
    is_killable is concurrently readable by a killer thread.
    It is protected by LOCK_thd_data, it is not needed to lock while the
    value is changing from false not true. If the kill thread reads
    true we need to ensure that the thread doesn't proceed to assign
    another thread to the same TLS reference.
  */
  is_killable = true;
#ifndef DBUG_OFF
  /*
    Let mysqld define the thread id (not mysys)
    This allows us to move THD to different threads if needed.
  */
  set_my_thread_var_id(m_thread_id);
#endif
  real_id = my_thread_self();  // For debugging
}

/*
  Remove the thread specific info (THD and mem_root pointer) stored during
  store_global call for this thread.
*/
void THD::restore_globals() {
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  DBUG_ASSERT(thread_stack);

  /* Undocking the thread specific data. */
  current_thd = nullptr;
  THR_MALLOC = nullptr;
}

/*
  Cleanup after query.

  SYNOPSIS
    THD::cleanup_after_query()

  DESCRIPTION
    This function is used to reset thread data to its default state.

  NOTE
    This function is not suitable for setting thread data to some
    non-default values, as there is only one replication thread, so
    different master threads may overwrite data of each other on
    slave.
*/

void THD::cleanup_after_query() {
  /*
    Reset rand_used so that detection of calls to rand() will save random
    seeds if needed by the slave.

    Do not reset rand_used if inside a stored function or trigger because
    only the call to these operations is logged. Thus only the calling
    statement needs to detect rand() calls made by its substatements. These
    substatements must not set rand_used to 0 because it would remove the
    detection of rand() by the calling statement.
  */
  if (!in_sub_stmt) /* stored functions and triggers are a special case */
  {
    /* Forget those values, for next binlogger: */
    stmt_depends_on_first_successful_insert_id_in_prev_stmt = 0;
    auto_inc_intervals_in_cur_stmt_for_binlog.empty();
    rand_used = 0;
    binlog_accessed_db_names = NULL;

    /*
      Clean possible unused INSERT_ID events by current statement.
      is_update_query() is needed to ignore SET statements:
        Statements that don't update anything directly and don't
        used stored functions. This is mostly necessary to ignore
        statements in binlog between SET INSERT_ID and DML statement
        which is intended to consume its event (there can be other
        SET statements between them).
    */
    if ((rli_slave || rli_fake) && is_update_query(lex->sql_command))
      auto_inc_intervals_forced.empty();
  }

  /*
    In case of stored procedures, stored functions, triggers and events
    m_trans_fixed_log_file will not be set to NULL. The memory will be reused.
  */
  if (!sp_runtime_ctx) m_trans_fixed_log_file = NULL;

  /*
    Forget the binlog stmt filter for the next query.
    There are some code paths that:
    - do not call THD::decide_logging_format()
    - do call THD::binlog_query(),
    making this reset necessary.
  */
  reset_binlog_local_stmt_filter();
  if (first_successful_insert_id_in_cur_stmt > 0) {
    /* set what LAST_INSERT_ID() will return */
    first_successful_insert_id_in_prev_stmt =
        first_successful_insert_id_in_cur_stmt;
    first_successful_insert_id_in_cur_stmt = 0;
    substitute_null_with_insert_id = true;
  }
  arg_of_last_insert_id_function = 0;
  /* Hack for cleaning up view security contexts */
  List_iterator<Security_context> it(m_view_ctx_list);
  while (Security_context *ctx = it++) {
    ctx->logout();
  }
  m_view_ctx_list.empty();
  /* Free Items that were created during this execution */
  free_items();
  /* Reset where. */
  where = THD::DEFAULT_WHERE;
  /* reset table map for multi-table update */
  table_map_for_update = 0;
  m_binlog_invoker = false;
  /* reset replication info structure */
  if (lex) {
    lex->mi.repl_ignore_server_ids.clear();
  }
  if (rli_slave) rli_slave->cleanup_after_query();
  // Set the default "cute" mode for the execution environment:
  check_for_truncated_fields = CHECK_FIELD_IGNORE;
}

LEX_CSTRING *make_lex_string_root(MEM_ROOT *mem_root, LEX_CSTRING *lex_str,
                                  const char *str, size_t length,
                                  bool allocate_lex_string) {
  if (allocate_lex_string)
    if (!(lex_str = (LEX_CSTRING *)alloc_root(mem_root, sizeof(LEX_CSTRING))))
      return 0;
  if (!(lex_str->str = strmake_root(mem_root, str, length))) return 0;
  lex_str->length = length;
  return lex_str;
}

LEX_STRING *make_lex_string_root(MEM_ROOT *mem_root, LEX_STRING *lex_str,
                                 const char *str, size_t length,
                                 bool allocate_lex_string) {
  if (allocate_lex_string)
    if (!(lex_str = (LEX_STRING *)alloc_root(mem_root, sizeof(LEX_STRING))))
      return 0;
  if (!(lex_str->str = strmake_root(mem_root, str, length))) return 0;
  lex_str->length = length;
  return lex_str;
}

LEX_CSTRING *THD::make_lex_string(LEX_CSTRING *lex_str, const char *str,
                                  size_t length, bool allocate_lex_string) {
  return make_lex_string_root(mem_root, lex_str, str, length,
                              allocate_lex_string);
}

/**
  Create a LEX_STRING in this connection.

  @param lex_str  pointer to LEX_STRING object to be initialized
  @param str      initializer to be copied into lex_str
  @param length   length of str, in bytes
  @param allocate_lex_string  if true, allocate new LEX_STRING object,
                              instead of using lex_str value
  @return  NULL on failure, or pointer to the LEX_STRING object
*/
LEX_STRING *THD::make_lex_string(LEX_STRING *lex_str, const char *str,
                                 size_t length, bool allocate_lex_string) {
  return make_lex_string_root(mem_root, lex_str, str, length,
                              allocate_lex_string);
}

/*
  Convert a string to another character set

  @param to             Store new allocated string here
  @param to_cs          New character set for allocated string
  @param from           String to convert
  @param from_length    Length of string to convert
  @param from_cs        Original character set

  @note to will be 0-terminated to make it easy to pass to system funcs

  @retval false ok
  @retval true  End of memory.
                In this case to->str will point to 0 and to->length will be 0.
*/

bool THD::convert_string(LEX_STRING *to, const CHARSET_INFO *to_cs,
                         const char *from, size_t from_length,
                         const CHARSET_INFO *from_cs) {
  DBUG_ENTER("convert_string");
  size_t new_length = to_cs->mbmaxlen * from_length;
  uint errors = 0;
  if (!(to->str = (char *)alloc(new_length + 1))) {
    to->length = 0;  // Safety fix
    DBUG_RETURN(1);  // EOM
  }
  to->length = copy_and_convert(to->str, new_length, to_cs, from, from_length,
                                from_cs, &errors);
  to->str[to->length] = 0;  // Safety
  if (errors != 0) {
    char printable_buff[32];
    convert_to_printable(printable_buff, sizeof(printable_buff), from,
                         from_length, from_cs, 6);
    push_warning_printf(this, Sql_condition::SL_WARNING,
                        ER_INVALID_CHARACTER_STRING,
                        ER_THD(this, ER_INVALID_CHARACTER_STRING),
                        from_cs->csname, printable_buff);
  }

  DBUG_RETURN(0);
}

/*
  Convert string from source character set to target character set inplace.

  SYNOPSIS
    THD::convert_string

  DESCRIPTION
    Convert string using convert_buffer - buffer for character set
    conversion shared between all protocols.

  RETURN
    0   ok
   !0   out of memory
*/

bool THD::convert_string(String *s, const CHARSET_INFO *from_cs,
                         const CHARSET_INFO *to_cs) {
  uint dummy_errors;
  if (convert_buffer.copy(s->ptr(), s->length(), from_cs, to_cs, &dummy_errors))
    return true;
  /* If convert_buffer >> s copying is more efficient long term */
  if (convert_buffer.alloced_length() >= convert_buffer.length() * 2 ||
      !s->is_alloced()) {
    return s->copy(convert_buffer);
  }
  s->swap(convert_buffer);
  return false;
}

/*
  Update some cache variables when character set changes
*/

void THD::update_charset() {
  size_t not_used;
  charset_is_system_charset = !String::needs_conversion(
      0, variables.character_set_client, system_charset_info, &not_used);
  charset_is_collation_connection =
      !String::needs_conversion(0, variables.character_set_client,
                                variables.collation_connection, &not_used);
  charset_is_character_set_filesystem =
      !String::needs_conversion(0, variables.character_set_client,
                                variables.character_set_filesystem, &not_used);
}

int THD::send_explain_fields(Query_result *result) {
  List<Item> field_list;
  Item *item;
  CHARSET_INFO *cs = system_charset_info;
  field_list.push_back(new Item_return_int("id", 3, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(item =
                           new Item_empty_string("table", NAME_CHAR_LEN, cs));
  item->maybe_null = 1;
  /* Maximum length of string that make_used_partitions_str() can produce */
  item = new Item_empty_string("partitions", MAX_PARTITIONS * (1 + FN_LEN), cs);
  field_list.push_back(item);
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("type", 10, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string(
                           "possible_keys", NAME_CHAR_LEN * MAX_KEY, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("key", NAME_CHAR_LEN, cs));
  item->maybe_null = 1;
  field_list.push_back(
      item = new Item_empty_string("key_len", NAME_CHAR_LEN * MAX_KEY));
  item->maybe_null = 1;
  field_list.push_back(
      item = new Item_empty_string("ref", NAME_CHAR_LEN * MAX_REF_PARTS, cs));
  item->maybe_null = 1;
  field_list.push_back(
      item = new Item_return_int("rows", 10, MYSQL_TYPE_LONGLONG));
  item->maybe_null = 1;
  field_list.push_back(
      item = new Item_float(NAME_STRING("filtered"), 0.1234, 2, 4));
  item->maybe_null = 1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  item->maybe_null = 1;
  return (result->send_result_set_metadata(
      field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF));
}

enum_vio_type THD::get_vio_type() {
  DBUG_ENTER("THD::get_vio_type");
  DBUG_RETURN(get_protocol()->connection_type());
}

void THD::shutdown_active_vio() {
  DBUG_ENTER("shutdown_active_vio");
  mysql_mutex_assert_owner(&LOCK_thd_data);
  if (active_vio) {
    vio_shutdown(active_vio);
    active_vio = 0;
    m_SSL = NULL;
  }
  DBUG_VOID_RETURN;
}

void THD::shutdown_clone_vio() {
  DBUG_ENTER("shutdown_clone_vio");
  mysql_mutex_assert_owner(&LOCK_thd_data);
  if (clone_vio != nullptr) {
    vio_shutdown(clone_vio);
    clone_vio = nullptr;
  }
  DBUG_VOID_RETURN;
}

/*
  Register an item tree tree transformation, performed by the query
  optimizer.
*/

void THD::nocheck_register_item_tree_change(Item **place, Item *new_value) {
  Item_change_record *change;
  /*
    Now we use one node per change, which adds some memory overhead,
    but still is rather fast as we use alloc_root for allocations.
    A list of item tree changes of an average query should be short.
  */
  void *change_mem = alloc_root(mem_root, sizeof(*change));
  if (change_mem == 0) {
    /*
      OOM, thd->fatal_error() is called by the error handler of the
      memroot. Just return.
    */
    return;
  }
  change = new (change_mem) Item_change_record(place, new_value);
  change_list.push_front(change);
}

void THD::replace_rollback_place(Item **new_place) {
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  while ((change = it++)) {
    if (change->new_value == *new_place) {
      DBUG_PRINT("info", ("replace_rollback_place new_value %p place %p",
                          *new_place, new_place));
      change->place = new_place;
      break;
    }
  }
}

void THD::rollback_item_tree_changes() {
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  DBUG_ENTER("rollback_item_tree_changes");

  while ((change = it++)) {
    DBUG_PRINT("info", ("rollback_item_tree_changes "
                        "place %p curr_value %p old_value %p",
                        change->place, *change->place, change->old_value));
    *change->place = change->old_value;
  }
  /* We can forget about changes memory: it's allocated in runtime memroot */
  change_list.empty();
  DBUG_VOID_RETURN;
}

void Query_arena::add_item(Item *item) {
  item->next = m_item_list;
  m_item_list = item;
}

void Query_arena::free_items() {
  Item *next;
  DBUG_ENTER("Query_arena::free_items");
  /* This works because items are allocated with sql_alloc() */
  for (; m_item_list; m_item_list = next) {
    next = m_item_list->next;
    m_item_list->delete_self();
  }
  /* Postcondition: free_list is 0 */
  DBUG_VOID_RETURN;
}

void Query_arena::set_query_arena(const Query_arena &set) {
  mem_root = set.mem_root;
  set_item_list(set.item_list());
  state = set.state;
}

void Query_arena::swap_query_arena(const Query_arena &source,
                                   Query_arena *backup) {
  backup->set_query_arena(*this);
  set_query_arena(source);
}

void THD::end_statement() {
  DBUG_ENTER("end_statement");
  /* Cleanup SQL processing state to reuse this statement in next query. */
  lex_end(lex);
  //@todo Check lifetime of Query_result objects.
  // delete lex->result;
  lex->result = NULL;  // Prepare for next statement
  /* Note that item list is freed in cleanup_after_query() */

  /*
    Don't free mem_root, as mem_root is freed in the end of dispatch_command
    (once for any command).
  */
  DBUG_VOID_RETURN;
}

Prepared_statement_map::Prepared_statement_map()
    : st_hash(key_memory_prepared_statement_map),
      names_hash(system_charset_info, key_memory_prepared_statement_map),
      m_last_found_statement(NULL) {}

int Prepared_statement_map::insert(Prepared_statement *statement) {
  st_hash.emplace(statement->id, unique_ptr<Prepared_statement>(statement));
  if (statement->name().str) {
    names_hash.emplace(to_string(statement->name()), statement);
  }
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  /*
    We don't check that prepared_stmt_count is <= max_prepared_stmt_count
    because we would like to allow to lower the total limit
    of prepared statements below the current count. In that case
    no new statements can be added until prepared_stmt_count drops below
    the limit.
  */
  if (prepared_stmt_count >= max_prepared_stmt_count) {
    mysql_mutex_unlock(&LOCK_prepared_stmt_count);
    my_error(ER_MAX_PREPARED_STMT_COUNT_REACHED, MYF(0),
             max_prepared_stmt_count);
    goto err_max;
  }
  prepared_stmt_count++;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);

  m_last_found_statement = statement;
  return 0;

err_max:
  if (statement->name().str) names_hash.erase(to_string(statement->name()));
  st_hash.erase(statement->id);
  return 1;
}

Prepared_statement *Prepared_statement_map::find_by_name(
    const LEX_CSTRING &name) {
  return find_or_nullptr(names_hash, to_string(name));
}

Prepared_statement *Prepared_statement_map::find(ulong id) {
  if (m_last_found_statement == NULL || id != m_last_found_statement->id) {
    Prepared_statement *stmt = find_or_nullptr(st_hash, id);
    if (stmt && stmt->name().str) return NULL;
    m_last_found_statement = stmt;
  }
  return m_last_found_statement;
}

void Prepared_statement_map::erase(Prepared_statement *statement) {
  if (statement == m_last_found_statement) m_last_found_statement = NULL;
  if (statement->name().str) names_hash.erase(to_string(statement->name()));

  st_hash.erase(statement->id);
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  DBUG_ASSERT(prepared_stmt_count > 0);
  prepared_stmt_count--;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);
}

void Prepared_statement_map::claim_memory_ownership() {
  for (const auto &key_and_value : st_hash) {
    my_claim(key_and_value.second.get());
  }
}

void Prepared_statement_map::reset() {
  if (!st_hash.empty()) {
#ifdef HAVE_PSI_PS_INTERFACE
    for (auto &key_and_value : st_hash) {
      Prepared_statement *stmt = key_and_value.second.get();
      MYSQL_DESTROY_PS(stmt->get_PS_prepared_stmt());
    }
#endif
    mysql_mutex_lock(&LOCK_prepared_stmt_count);
    DBUG_ASSERT(prepared_stmt_count >= st_hash.size());
    prepared_stmt_count -= st_hash.size();
    mysql_mutex_unlock(&LOCK_prepared_stmt_count);
  }
  names_hash.clear();
  st_hash.clear();
  m_last_found_statement = NULL;
}

Prepared_statement_map::~Prepared_statement_map() {
  /*
    We do not want to grab the global LOCK_prepared_stmt_count mutex here.
    reset() should already have been called to maintain prepared_stmt_count.
   */
  DBUG_ASSERT(st_hash.empty());
}

void THD::send_kill_message() const {
  int err = killed;
  if (err && !get_stmt_da()->is_set()) {
    if ((err == KILL_CONNECTION) && !connection_events_loop_aborted())
      err = KILL_QUERY;
    /*
      KILL is fatal because:
      - if a condition handler was allowed to trap and ignore a KILL, one
      could create routines which the DBA could not kill
      - INSERT/UPDATE IGNORE should fail: if KILL arrives during
      JOIN::optimize(), statement cannot possibly run as its caller expected
      => "OK" would be misleading the caller.
    */
    my_error(err, MYF(ME_FATALERROR));
  }
}

/****************************************************************************
  Handling of open and locked tables states.

  This is used when we want to open/lock (and then close) some tables when
  we already have a set of tables open and locked. We use these methods for
  access to mysql.proc table to find definitions of stored routines.
****************************************************************************/

void THD::reset_n_backup_open_tables_state(Open_tables_backup *backup,
                                           uint add_state_flags) {
  DBUG_ENTER("reset_n_backup_open_tables_state");
  backup->set_open_tables_state(this);
  backup->mdl_system_tables_svp = mdl_context.mdl_savepoint();
  reset_open_tables_state();
  state_flags |= (Open_tables_state::BACKUPS_AVAIL | add_state_flags);
  DBUG_VOID_RETURN;
}

void THD::restore_backup_open_tables_state(Open_tables_backup *backup) {
  DBUG_ENTER("restore_backup_open_tables_state");
  mdl_context.rollback_to_savepoint(backup->mdl_system_tables_svp);
  /*
    Before we will throw away current open tables state we want
    to be sure that it was properly cleaned up.
  */
  DBUG_ASSERT(open_tables == 0 && temporary_tables == 0 && lock == 0 &&
              locked_tables_mode == LTM_NONE &&
              get_reprepare_observer() == NULL);

  set_open_tables_state(backup);
  DBUG_VOID_RETURN;
}

void THD::begin_attachable_ro_transaction() {
  m_attachable_trx = new Attachable_trx(this, m_attachable_trx);
}

void THD::end_attachable_transaction() {
  Attachable_trx *prev_trx = m_attachable_trx->get_prev_attachable_trx();
  delete m_attachable_trx;
  // Restore attachable transaction which was active before we started
  // the one which just has ended. NULL in most cases.
  m_attachable_trx = prev_trx;
}

void THD::begin_attachable_rw_transaction() {
  DBUG_ASSERT(!m_attachable_trx);

  m_attachable_trx = new Attachable_trx_rw(this);
}

/****************************************************************************
  Handling of statement states in functions and triggers.

  This is used to ensure that the function/trigger gets a clean state
  to work with and does not cause any side effects of the calling statement.

  It also allows most stored functions and triggers to replicate even
  if they are used items that would normally be stored in the binary
  replication (like last_insert_id() etc...)

  The following things is done
  - Disable binary logging for the duration of the statement
  - Disable multi-result-sets for the duration of the statement
  - Value of last_insert_id() is saved and restored
  - Value set by 'SET INSERT_ID=#' is reset and restored
  - Value for found_rows() is reset and restored
  - examined_row_count is added to the total
  - num_truncated_fields is added to the total
  - new savepoint level is created and destroyed

  NOTES:
    Seed for random() is saved for the first! usage of RAND()
    We reset examined_row_count and num_truncated_fields and add these to the
    result to ensure that if we have a bug that would reset these within
    a function, we are not loosing any rows from the main statement.

    We do not reset value of last_insert_id().
****************************************************************************/

void THD::reset_sub_statement_state(Sub_statement_state *backup,
                                    uint new_state) {
  /* BUG#33029, if we are replicating from a buggy master, reset
     auto_inc_intervals_forced to prevent substatement
     (triggers/functions) from using erroneous INSERT_ID value
   */
  if (rpl_master_erroneous_autoinc(this)) {
    DBUG_ASSERT(backup->auto_inc_intervals_forced.nb_elements() == 0);
    auto_inc_intervals_forced.swap(&backup->auto_inc_intervals_forced);
  }

  backup->option_bits = variables.option_bits;
  backup->check_for_truncated_fields = check_for_truncated_fields;
  backup->in_sub_stmt = in_sub_stmt;
  backup->enable_slow_log = enable_slow_log;
  backup->current_found_rows = current_found_rows;
  backup->previous_found_rows = previous_found_rows;
  backup->examined_row_count = m_examined_row_count;
  backup->sent_row_count = m_sent_row_count;
  backup->num_truncated_fields = num_truncated_fields;
  backup->client_capabilities = m_protocol->get_client_capabilities();
  backup->savepoints = get_transaction()->m_savepoints;
  backup->first_successful_insert_id_in_prev_stmt =
      first_successful_insert_id_in_prev_stmt;
  backup->first_successful_insert_id_in_cur_stmt =
      first_successful_insert_id_in_cur_stmt;

  if ((!lex->requires_prelocking() || is_update_query(lex->sql_command)) &&
      !is_current_stmt_binlog_format_row()) {
    variables.option_bits &= ~OPTION_BIN_LOG;
  }

  if ((backup->option_bits & OPTION_BIN_LOG) &&
      is_update_query(lex->sql_command) && !is_current_stmt_binlog_format_row())
    mysql_bin_log.start_union_events(this, this->query_id);

  /* Disable result sets */
  if (is_classic_protocol())
    get_protocol_classic()->remove_client_capability(CLIENT_MULTI_RESULTS);
  in_sub_stmt |= new_state;
  m_examined_row_count = 0;
  m_sent_row_count = 0;
  num_truncated_fields = 0;
  get_transaction()->m_savepoints = 0;
  first_successful_insert_id_in_cur_stmt = 0;

  /* Reset savepoint on transaction write set */
  if (is_current_stmt_binlog_row_enabled_with_write_set_extraction()) {
    get_transaction()->get_transaction_write_set_ctx()->reset_savepoint_list();
  }
}

void THD::restore_sub_statement_state(Sub_statement_state *backup) {
  DBUG_ENTER("THD::restore_sub_statement_state");
  /* BUG#33029, if we are replicating from a buggy master, restore
     auto_inc_intervals_forced so that the top statement can use the
     INSERT_ID value set before this statement.
   */
  if (rpl_master_erroneous_autoinc(this)) {
    backup->auto_inc_intervals_forced.swap(&auto_inc_intervals_forced);
    DBUG_ASSERT(backup->auto_inc_intervals_forced.nb_elements() == 0);
  }

  /*
    To save resources we want to release savepoints which were created
    during execution of function or trigger before leaving their savepoint
    level. It is enough to release first savepoint set on this level since
    all later savepoints will be released automatically.
  */
  if (get_transaction()->m_savepoints) {
    SAVEPOINT *sv;
    for (sv = get_transaction()->m_savepoints; sv->prev; sv = sv->prev) {
    }
    /* ha_release_savepoint() never returns error. */
    (void)ha_release_savepoint(this, sv);
  }
  check_for_truncated_fields = backup->check_for_truncated_fields;
  get_transaction()->m_savepoints = backup->savepoints;
  variables.option_bits = backup->option_bits;
  in_sub_stmt = backup->in_sub_stmt;
  enable_slow_log = backup->enable_slow_log;
  first_successful_insert_id_in_prev_stmt =
      backup->first_successful_insert_id_in_prev_stmt;
  first_successful_insert_id_in_cur_stmt =
      backup->first_successful_insert_id_in_cur_stmt;
  current_found_rows = backup->current_found_rows;
  previous_found_rows = backup->previous_found_rows;
  set_sent_row_count(backup->sent_row_count);
  if (is_classic_protocol())
    get_protocol_classic()->set_client_capabilities(
        backup->client_capabilities);

  /*
    If we've left sub-statement mode, reset the fatal error flag.
    Otherwise keep the current value, to propagate it up the sub-statement
    stack.

    NOTE: is_fatal_sub_stmt_error can be set only if we've been in the
    sub-statement mode.
  */

  if (!in_sub_stmt) is_fatal_sub_stmt_error = false;

  if ((variables.option_bits & OPTION_BIN_LOG) &&
      is_update_query(lex->sql_command) && !is_current_stmt_binlog_format_row())
    mysql_bin_log.stop_union_events(this);

  /*
    The below assert mostly serves as reminder that optimization in
    DML_prelocking_strategy::handle_table() relies on the fact
    that stored function/trigger can't change FOREIGN_KEY_CHECKS
    value for the top-level statement which invokes them.
  */
  DBUG_ASSERT((variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS) ==
              (backup->option_bits & OPTION_NO_FOREIGN_KEY_CHECKS));

  /*
    The following is added to the old values as we are interested in the
    total complexity of the query
  */
  inc_examined_row_count(backup->examined_row_count);
  num_truncated_fields += backup->num_truncated_fields;

  /* Restore savepoint on transaction write set */
  if (is_current_stmt_binlog_row_enabled_with_write_set_extraction()) {
    get_transaction()
        ->get_transaction_write_set_ctx()
        ->restore_savepoint_list();
  }

  DBUG_VOID_RETURN;
}

void THD::set_sent_row_count(ha_rows count) {
  m_sent_row_count = count;
  MYSQL_SET_STATEMENT_ROWS_SENT(m_statement_psi, m_sent_row_count);
}

void THD::inc_sent_row_count(ha_rows count) {
  m_sent_row_count += count;
  MYSQL_SET_STATEMENT_ROWS_SENT(m_statement_psi, m_sent_row_count);
}

void THD::inc_examined_row_count(ha_rows count) {
  m_examined_row_count += count;
  MYSQL_SET_STATEMENT_ROWS_EXAMINED(m_statement_psi, m_examined_row_count);
}

void THD::inc_status_created_tmp_disk_tables() {
  status_var.created_tmp_disk_tables++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_created_tmp_disk_tables)(m_statement_psi, 1);
#endif
}

void THD::inc_status_created_tmp_tables() {
  status_var.created_tmp_tables++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_created_tmp_tables)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_full_join() {
  status_var.select_full_join_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_full_join)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_full_range_join() {
  status_var.select_full_range_join_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_full_range_join)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_range() {
  status_var.select_range_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_range)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_range_check() {
  status_var.select_range_check_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_range_check)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_scan() {
  status_var.select_scan_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_scan)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_merge_passes() {
  status_var.filesort_merge_passes++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_merge_passes)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_range() {
  status_var.filesort_range_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_range)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_rows(ha_rows count) {
  status_var.filesort_rows += count;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_rows)
  (m_statement_psi, static_cast<ulong>(count));
#endif
}

void THD::inc_status_sort_scan() {
  status_var.filesort_scan_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_scan)(m_statement_psi, 1);
#endif
}

void THD::set_status_no_index_used() {
  server_status |= SERVER_QUERY_NO_INDEX_USED;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(set_statement_no_index_used)(m_statement_psi);
#endif
}

void THD::set_status_no_good_index_used() {
  server_status |= SERVER_QUERY_NO_GOOD_INDEX_USED;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(set_statement_no_good_index_used)(m_statement_psi);
#endif
}

void THD::set_command(enum enum_server_command command) {
  m_command = command;
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_thread_command)(m_command);
#endif
}

void THD::debug_assert_query_locked() const {
  if (current_thd != this) mysql_mutex_assert_owner(&LOCK_thd_query);
}

void THD::set_query(const LEX_CSTRING &query_arg) {
  DBUG_ASSERT(this == current_thd);
  mysql_mutex_lock(&LOCK_thd_query);
  m_query_string = query_arg;
  mysql_mutex_unlock(&LOCK_thd_query);

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_thread_info)(query_arg.str, query_arg.length);
#endif
}

/**
  Leave explicit LOCK TABLES or prelocked mode and restore value of
  transaction sentinel in MDL subsystem.
*/

void THD::leave_locked_tables_mode() {
  if (locked_tables_mode == LTM_LOCK_TABLES) {
    /*
      When leaving LOCK TABLES mode we have to change the duration of most
      of the metadata locks being held, except for HANDLER and GRL locks,
      to transactional for them to be properly released at UNLOCK TABLES.
    */
    mdl_context.set_transaction_duration_for_all_locks();
    /*
      Make sure we don't release the global read lock and commit blocker
      when leaving LTM.
    */
    global_read_lock.set_explicit_lock_duration(this);
    /*
      Also ensure that we don't release metadata locks for open HANDLERs
      and user-level locks.
    */
    if (!handler_tables_hash.empty()) mysql_ha_set_explicit_lock_duration(this);
    if (!ull_hash.empty()) mysql_ull_set_explicit_lock_duration(this);
  }
  locked_tables_mode = LTM_NONE;
}

void THD::get_definer(LEX_USER *definer) {
  binlog_invoker();
  if (slave_thread && has_invoker()) {
    definer->user = m_invoker_user;
    definer->host = m_invoker_host;
    definer->plugin.str = (char *)"";
    definer->plugin.length = 0;
    definer->auth.str = NULL;
    definer->auth.length = 0;
  } else
    get_default_definer(this, definer);
}

/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.

  @param  all   true <=> rollback main transaction.
*/

void THD::mark_transaction_to_rollback(bool all) {
  /*
    There is no point in setting is_fatal_sub_stmt_error unless
    we are actually in_sub_stmt.
  */
  if (in_sub_stmt) is_fatal_sub_stmt_error = true;

  transaction_rollback_request = all;
}

void THD::set_next_event_pos(const char *_filename, ulonglong _pos) {
  char *&filename = binlog_next_event_pos.file_name;
  if (filename == NULL) {
    /* First time, allocate maximal buffer */
    filename =
        (char *)my_malloc(key_memory_LOG_POS_COORD, FN_REFLEN + 1, MYF(MY_WME));
    if (filename == NULL) return;
  }

  assert(strlen(_filename) <= FN_REFLEN);
  strcpy(filename, _filename);
  filename[FN_REFLEN] = 0;

  binlog_next_event_pos.pos = _pos;
}

void THD::clear_next_event_pos() {
  if (binlog_next_event_pos.file_name != NULL) {
    my_free(binlog_next_event_pos.file_name);
  }
  binlog_next_event_pos.file_name = NULL;
  binlog_next_event_pos.pos = 0;
}

void THD::set_original_commit_timestamp_for_slave_thread() {
  /*
    This function may be called in four cases:

    - From SQL thread while executing Gtid_log_event::do_apply_event

    - From an mts worker thread that executes a Gtid_log_event::do_apply_event.

    - From an mts worker thread that is processing an old binlog that
      is missing Gtid events completely, from gtid_pre_statement_checks().

    - From a normal client thread that is executing output from
      mysqlbinlog when mysqlbinlog is processing an old binlog file
      that is missing Gtid events completely, from
      gtid_pre_statement_checks() for a statement that appears after a
      BINLOG statement containing a Format_description_log_event
      originating from the master.

    Because of the last case, we need to add the following conditions to set
    original_commit_timestamp.
  */
  if (system_thread == SYSTEM_THREAD_SLAVE_SQL ||
      system_thread == SYSTEM_THREAD_SLAVE_WORKER) {
    rli_slave->original_commit_timestamp = variables.original_commit_timestamp;
  }
}

void THD::set_user_connect(USER_CONN *uc) {
  DBUG_ENTER("THD::set_user_connect");

  m_user_connect = uc;

  DBUG_VOID_RETURN;
}

void THD::increment_user_connections_counter() {
  DBUG_ENTER("THD::increment_user_connections_counter");

  m_user_connect->connections++;

  DBUG_VOID_RETURN;
}

void THD::decrement_user_connections_counter() {
  DBUG_ENTER("THD::decrement_user_connections_counter");

  DBUG_ASSERT(m_user_connect->connections > 0);
  m_user_connect->connections--;

  DBUG_VOID_RETURN;
}

void THD::increment_con_per_hour_counter() {
  DBUG_ENTER("THD::increment_con_per_hour_counter");

  m_user_connect->conn_per_hour++;

  DBUG_VOID_RETURN;
}

void THD::increment_updates_counter() {
  DBUG_ENTER("THD::increment_updates_counter");

  m_user_connect->updates++;

  DBUG_VOID_RETURN;
}

void THD::increment_questions_counter() {
  DBUG_ENTER("THD::increment_questions_counter");

  m_user_connect->questions++;

  DBUG_VOID_RETURN;
}

/*
  Reset per-hour user resource limits when it has been more than
  an hour since they were last checked

  SYNOPSIS:
    time_out_user_resource_limits()

  NOTE:
    This assumes that the LOCK_user_conn mutex has been acquired, so it is
    safe to test and modify members of the USER_CONN structure.
*/
void THD::time_out_user_resource_limits() {
  mysql_mutex_assert_owner(&LOCK_user_conn);
  ulonglong check_time = start_utime;
  DBUG_ENTER("time_out_user_resource_limits");

  /* If more than a hour since last check, reset resource checking */
  if (check_time - m_user_connect->reset_utime >= 3600000000LL) {
    m_user_connect->questions = 1;
    m_user_connect->updates = 0;
    m_user_connect->conn_per_hour = 0;
    m_user_connect->reset_utime = check_time;
  }

  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF
void THD::Query_plan::assert_plan_is_locked_if_other() const {
  if (current_thd != thd) mysql_mutex_assert_owner(&thd->LOCK_query_plan);
}
#endif

void THD::Query_plan::set_query_plan(enum_sql_command sql_cmd, LEX *lex_arg,
                                     bool ps) {
  DBUG_ASSERT(current_thd == thd);

  // No need to grab mutex for repeated (SQLCOM_END, NULL, false).
  if (sql_command == sql_cmd && lex == lex_arg && is_ps == ps) {
    return;
  }

  thd->lock_query_plan();
  sql_command = sql_cmd;
  lex = lex_arg;
  is_ps = ps;
  thd->unlock_query_plan();
}

void THD::Query_plan::set_modification_plan(Modification_plan *plan_arg) {
  DBUG_ASSERT(current_thd == thd);
  mysql_mutex_assert_owner(&thd->LOCK_query_plan);
  modification_plan = plan_arg;
}

/**
  Push an error message into MySQL diagnostic area with line number and position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a syntax error is discovered according to the Bison grammar.
  Unlike the syntax_error_at() function, the error position points to the last
  parsed token.

  @note Parse-time only function!

  @param format         Error format message. NULL means ER(ER_SYNTAX_ERROR).
*/
void THD::syntax_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsyntax_error_at(m_parser_state->m_lip.get_tok_start(), format, args);
  va_end(args);
}

/**
  Push an error message into MySQL diagnostic area with line number and position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a syntax error is discovered according to the Bison grammar.
  Unlike the syntax_error_at() function, the error position points to the last
  parsed token.

  @note Parse-time only function!

  @param mysql_errno    Error number to get a format string with ER_THD().
*/
void THD::syntax_error(int mysql_errno, ...) {
  va_list args;
  va_start(args, mysql_errno);
  vsyntax_error_at(m_parser_state->m_lip.get_tok_start(),
                   ER_THD(this, mysql_errno), args);
  va_end(args);
}

/**
  Push a syntax error message into MySQL diagnostic area with line
  and position information.

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.

  @note Parse-time only function!

  @param location       YYSTYPE object: error position.
  @param format         Error format message. NULL means ER(ER_SYNTAX_ERROR).
*/

void THD::syntax_error_at(const YYLTYPE &location, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsyntax_error_at(location, format, args);
  va_end(args);
}

/**
  Push a syntax error message into MySQL diagnostic area with line
  and position information.

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.

  @note Parse-time only function!

  @param location       YYSTYPE object: error position
  @param mysql_errno    Error number to get a format string with ER_THD()
*/
void THD::syntax_error_at(const YYLTYPE &location, int mysql_errno, ...) {
  va_list args;
  va_start(args, mysql_errno);
  vsyntax_error_at(location, ER_THD(this, mysql_errno), args);
  va_end(args);
}

void THD::vsyntax_error_at(const YYLTYPE &location, const char *format,
                           va_list args) {
  vsyntax_error_at(location.raw.start, format, args);
}

/**
  Push a syntax error message into MySQL diagnostic area with line number and
  position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the error stack, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.

  @param pos_in_lexer_raw_buffer        Pointer into LEX::m_buf or NULL.
  @param format                         An error message format string.
  @param args                           Arguments to the format string.
*/

void THD::vsyntax_error_at(const char *pos_in_lexer_raw_buffer,
                           const char *format, va_list args) {
  DBUG_ASSERT(
      pos_in_lexer_raw_buffer == NULL ||
      (pos_in_lexer_raw_buffer >= m_parser_state->m_lip.get_buf() &&
       pos_in_lexer_raw_buffer <= m_parser_state->m_lip.get_end_of_query()));

  char buff[MYSQL_ERRMSG_SIZE];
  if (check_stack_overrun(this, STACK_MIN_SIZE, (uchar *)buff)) return;

  const uint lineno =
      pos_in_lexer_raw_buffer
          ? m_parser_state->m_lip.get_lineno(pos_in_lexer_raw_buffer)
          : 1;
  const char *pos = pos_in_lexer_raw_buffer ? pos_in_lexer_raw_buffer : "";
  ErrConvString err(pos, variables.character_set_client);
  (void)vsnprintf(buff, sizeof(buff), format, args);
  my_printf_error(ER_PARSE_ERROR, ER_THD(this, ER_PARSE_ERROR), MYF(0), buff,
                  err.ptr(), lineno);
}

bool THD::send_result_metadata(List<Item> *list, uint flags) {
  DBUG_ENTER("send_result_metadata");
  List_iterator_fast<Item> it(*list);
  Item *item;
  uchar buff[MAX_FIELD_WIDTH];
  String tmp((char *)buff, sizeof(buff), &my_charset_bin);

  if (m_protocol->start_result_metadata(list->elements, flags,
                                        variables.character_set_results))
    goto err;
  switch (variables.resultset_metadata) {
    case RESULTSET_METADATA_FULL:
      /* Sent metadata. */
      while ((item = it++)) {
        Send_field field;
        item->make_field(&field);
        m_protocol->start_row();
        if (m_protocol->send_field_metadata(&field,
                                            item->charset_for_protocol()))
          goto err;
        if (flags & Protocol::SEND_DEFAULTS) item->send(m_protocol, &tmp);
        if (m_protocol->end_row()) DBUG_RETURN(true);
      }
      break;

    case RESULTSET_METADATA_NONE:
      /* Skip metadata. */
      break;

    default:
      /* Unknown @@resultset_metadata value. */
      DBUG_RETURN(1);
  }

  DBUG_RETURN(m_protocol->end_result_metadata());

err:
  my_error(ER_OUT_OF_RESOURCES, MYF(0)); /* purecov: inspected */
  DBUG_RETURN(1);                        /* purecov: inspected */
}

bool THD::send_result_set_row(List<Item> *row_items) {
  char buffer[MAX_FIELD_WIDTH];
  String str_buffer(buffer, sizeof(buffer), &my_charset_bin);
  List_iterator_fast<Item> it(*row_items);

  DBUG_ENTER("send_result_set_row");

  for (Item *item = it++; item; item = it++) {
    if (item->send(m_protocol, &str_buffer) || is_error()) DBUG_RETURN(true);
    /*
      Reset str_buffer to its original state, as it may have been altered in
      Item::send().
    */
    str_buffer.set(buffer, sizeof(buffer), &my_charset_bin);
  }
  DBUG_RETURN(false);
}

void THD::send_statement_status() {
  DBUG_ENTER("send_statement_status");
  DBUG_ASSERT(!get_stmt_da()->is_sent());
  bool error = false;
  Diagnostics_area *da = get_stmt_da();

  /* Can not be true, but do not take chances in production. */
  if (da->is_sent()) DBUG_VOID_RETURN;

  switch (da->status()) {
    case Diagnostics_area::DA_ERROR:
      /* The query failed, send error to log and abort bootstrap. */
      error = m_protocol->send_error(da->mysql_errno(), da->message_text(),
                                     da->returned_sqlstate());
      break;
    case Diagnostics_area::DA_EOF:
      error =
          m_protocol->send_eof(server_status, da->last_statement_cond_count());
      break;
    case Diagnostics_area::DA_OK:
      error = m_protocol->send_ok(
          server_status, da->last_statement_cond_count(), da->affected_rows(),
          da->last_insert_id(), da->message_text());
      break;
    case Diagnostics_area::DA_DISABLED:
      break;
    case Diagnostics_area::DA_EMPTY:
    default:
      DBUG_ASSERT(0);
      error = m_protocol->send_ok(server_status, 0, 0, 0, nullptr);
      break;
  }
  if (!error) da->set_is_sent(true);
  DBUG_VOID_RETURN;
}

void THD::claim_memory_ownership() {
/*
  Ownership of the THD object is transfered to this thread.
  This happens typically:
  - in the event scheduler,
    when the scheduler thread creates a work item and
    starts a worker thread to run it
  - in the main thread, when the code that accepts a new
    network connection creates a work item and starts a
    connection thread to run it.
  Accounting for memory statistics needs to be told
  that memory allocated by thread X now belongs to thread Y,
  so that statistics by thread/account/user/host are accurate.
  Inspect every piece of memory allocated in THD,
  and call PSI_MEMORY_CALL(memory_claim)().
*/
#ifdef HAVE_PSI_MEMORY_INTERFACE
  claim_root(&main_mem_root);
  my_claim(m_token_array);
  Protocol_classic *p = get_protocol_classic();
  if (p != NULL) p->claim_memory_ownership();
  session_tracker.claim_memory_ownership();
  session_sysvar_res_mgr.claim_memory_ownership();
  for (const auto &key_and_value : user_vars) {
    my_claim(key_and_value.second.get());
  }
#if defined(ENABLED_DEBUG_SYNC)
  debug_sync_claim_memory_ownership(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */
  get_transaction()->claim_memory_ownership();
  stmt_map.claim_memory_ownership();
#endif /* HAVE_PSI_MEMORY_INTERFACE */
}

void THD::rpl_detach_engine_ha_data() {
  Relay_log_info *rli =
      is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);

  DBUG_ASSERT(!rli_fake || !rli_fake->is_engine_ha_data_detached);
  DBUG_ASSERT(!rli_slave || !rli_slave->is_engine_ha_data_detached);

  if (rli) rli->detach_engine_ha_data(this);
}

void THD::rpl_reattach_engine_ha_data() {
  Relay_log_info *rli =
      is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);

  DBUG_ASSERT(!rli_fake || !rli_fake->is_engine_ha_data_detached);
  DBUG_ASSERT(!rli_slave || !rli_slave->is_engine_ha_data_detached);

  if (rli) rli->reattach_engine_ha_data(this);
}

bool THD::rpl_unflag_detached_engine_ha_data() {
  Relay_log_info *rli =
      is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);
  return rli ? rli->unflag_detached_engine_ha_data() : false;
}

/**
  Determine if binlogging is disabled for this session
  @retval 0 if the current statement binlogging is disabled
  (could be because of binlog closed/binlog option
  is set to false).
  @retval 1 if the current statement will be binlogged
*/
bool THD::is_current_stmt_binlog_disabled() const {
  return (!(variables.option_bits & OPTION_BIN_LOG) ||
          !mysql_bin_log.is_open());
}

bool THD::is_current_stmt_binlog_row_enabled_with_write_set_extraction() const {
  return ((variables.transaction_write_set_extraction != HASH_ALGORITHM_OFF) &&
          is_current_stmt_binlog_format_row() &&
          !is_current_stmt_binlog_disabled());
}

bool THD::Query_plan::is_single_table_plan() const {
  assert_plan_is_locked_if_other();
  return lex->m_sql_cmd->is_single_table_plan();
}

const String THD::normalized_query() {
  m_normalized_query.mem_free();
  lex->unit->print(&m_normalized_query, QT_NORMALIZED_FORMAT);
  return m_normalized_query;
}

bool add_item_to_list(THD *thd, Item *item) {
  return thd->lex->select_lex->add_item_to_list(item);
}

void add_order_to_list(THD *thd, ORDER *order) {
  thd->lex->select_lex->add_order_to_list(order);
}

THD::Transaction_state::Transaction_state()
    : m_query_tables_list(new Query_tables_list()),
      m_ha_data(PSI_NOT_INSTRUMENTED, m_ha_data.initial_capacity) {}

THD::Transaction_state::~Transaction_state() { delete m_query_tables_list; }

void THD::change_item_tree(Item **place, Item *new_value) {
  /* TODO: check for OOM condition here */
  if (!stmt_arena->is_regular()) {
    DBUG_PRINT("info", ("change_item_tree place %p old_value %p new_value %p",
                        place, *place, new_value));
    if (new_value)
      new_value->set_runtime_created(); /* Note the change of item tree */
    nocheck_register_item_tree_change(place, new_value);
  }
  *place = new_value;
}

bool THD::notify_hton_pre_acquire_exclusive(const MDL_key *mdl_key,
                                            bool *victimized) {
  return ha_notify_exclusive_mdl(this, mdl_key, HA_NOTIFY_PRE_EVENT,
                                 victimized);
}

void THD::notify_hton_post_release_exclusive(const MDL_key *mdl_key) {
  bool unused_arg;
  ha_notify_exclusive_mdl(this, mdl_key, HA_NOTIFY_POST_EVENT, &unused_arg);
}

void reattach_engine_ha_data_to_thd(THD *thd, const struct handlerton *hton) {
  DBUG_ENTER("reattach_engine_ha_data_to_thd");
  if (hton->replace_native_transaction_in_thd) {
    /* restore the saved original engine transaction's link with thd */
    void **trx_backup = &thd->get_ha_data(hton->slot)->ha_ptr_backup;

    hton->replace_native_transaction_in_thd(thd, *trx_backup, NULL);
    *trx_backup = NULL;
  }
  DBUG_VOID_RETURN;
}

/**
  Call parser to transform statement into a parse tree.
  Then, transform the parse tree further into an AST, ready for resolving.
*/
bool THD::sql_parser() {
  /*
    SQL parser function generated by YACC from sql_yacc.yy.

    In the case of success returns 0, and THD::is_error() is false.
    Otherwise returns 1, or THD::>is_error() is true.

    The second (output) parameter "root" returns the new parse tree.
    It is undefined (unchanged) on error. If "root" is NULL on success,
    then the parser has already called lex->make_sql_cmd() internally.
  */
  extern int MYSQLparse(class THD * thd, class Parse_tree_root * *root);

  Parse_tree_root *root = nullptr;
  if (MYSQLparse(this, &root) || is_error()) {
    return true;
  }
  if (root != nullptr && lex->make_sql_cmd(root)) {
    return true;
  }
  return false;
}

/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/statement/statement.h"

#include <cstddef>
#include <variant>
#include "field_types.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/psi/mysql_ps.h"  // MYSQL_EXECUTE_PS
#include "mysql/strings/m_ctype.h"
#include "mysql_time.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/query_result.h"
#include "sql/sp_cache.h"
#include "sql/sp_head.h"
#include "sql/sp_rcontext.h"
#include "sql/sql_lex.h"    //Parser_state
#include "sql/sql_parse.h"  // alloc_query
#include "sql/sql_prepare.h"
#include "sql/sql_profile.h"
#include "sql/sql_rewrite.h"  // mysql_rewrite_query
#include "sql/statement/utils.h"

int dummy_function_to_ensure_we_are_linked_into_the_server() { return 1; }

/**
 * RAII class to manage the diagnostics area for the statement handler.
 *
 * This class mainly manages diagnostics area(DA) for a statement execution (say
 * STMT2) when an error has already occurred (say for STMT1) and not reported to
 * a user yet. Error state from STMT1 is stored in the DA associated with THD.
 * This class maintains the error state from STMT1 in DA by using temporary
 * diagnostics area for the STMT2's execution.
 *
 * In the destructor, if STMT2 execution is successful then temporary
 * diagnostics are is just popped and error from STMT1 is reported to the user.
 * But if STMT2 execution fails, then error from STMT1 is cleared and error from
 * STMT2 is reported to the user.
 *
 * If DA is clear when instance of this class instatiated then caller's DA used
 * for statement execution.
 *
 * Please note that Statement_handle::copy_warnings() must be invoked after an
 * instance of this class is destructed to copy warnings after statement
 * execution.
 *
 * TODO: Executing a SQL statement when an error has already occurred can
 * primarily happen from error handlers of external routines. The best place to
 * manage DA for it is within the component supporting external routines. Once
 * we have improved DA handling for components, this logic should be moved to
 * the component.
 */
class Diagnostics_area_handler_raii {
 public:
  Diagnostics_area_handler_raii(THD *thd, bool reset_cond_info = false)
      : m_thd(thd), m_stmt_da(false) {
    if (m_thd->is_error()) {
      /*
        If a statement is being executed after an error is occurred, then push
        temporary DA instance for statement execution.
      */
      m_thd->push_diagnostics_area(&m_stmt_da);
    } else {
      // Use caller DA otherwise.
      if (reset_cond_info) m_thd->get_stmt_da()->reset_condition_info(m_thd);
      m_thd->get_stmt_da()->reset_diagnostics_area();
    }
  }

  ~Diagnostics_area_handler_raii() {
    if (m_thd->get_stmt_da() == &m_stmt_da) {
      // Pop m_stmt_da instance from the DA stack.
      m_thd->pop_diagnostics_area();

      /*
        If error is reported for a statement being executed, then clear
        previous errors and copy new error to caller DA.
      */
      if (m_stmt_da.is_error()) {
        // Clear current diagnostics information.
        m_thd->get_stmt_da()->reset_diagnostics_area();
        m_thd->get_stmt_da()->reset_condition_info(m_thd);

        // Copy diagnostics from src_da.
        m_thd->get_stmt_da()->set_error_status(m_stmt_da.mysql_errno(),
                                               m_stmt_da.message_text(),
                                               m_stmt_da.returned_sqlstate());

        // Copy warnings.
        m_thd->get_stmt_da()->copy_sql_conditions_from_da(m_thd, &m_stmt_da);
      }
    } else {
      // Reset caller DA if statement execution is successful.
      if (!m_thd->is_error()) m_thd->get_stmt_da()->reset_diagnostics_area();
    }
  }

 private:
  THD *m_thd;
  Diagnostics_area m_stmt_da;
};

void Statement_handle::send_statement_status() {
  if (!m_use_thd_protocol) {
    m_thd->send_statement_status();
  } else {
    /*
      When result pass-through is enabled, errors are not immediately
      transmitted to the client. Responsibility for error handling falls upon
      upon the Statement Handle user.

      In certain scenarios, an error may occur after a partial result has been
      dispatched to the user. In the context of partial result-set errors, the
      error is handled as below,

        a) If the statement is *not* executed from within a Stored Program, a
           message to close partial result-set is *not* sent to client. Given
           the absence of error handlers in this context, the error is relayed
           to client through the Statement handle user.

        b) Conversely, when the statement is executed from within a Stored
           Program, a message to close partial result-set is sent from server
           to client.
           Please note that, information in Diagnostics Area is retained. The
           Statement handle user is still equipped to handle encountered error.
    */

    // End partial result-set.
    if (m_thd->sp_runtime_ctx != nullptr &&
        m_thd->sp_runtime_ctx->end_partial_result_set) {
      m_thd->get_protocol()->end_partial_result_set();
      push_warning(m_thd, Sql_condition::SL_WARNING,
                   ER_WARN_SP_STATEMENT_PARTIALLY_EXECUTED,
                   ER_THD(m_thd, ER_WARN_SP_STATEMENT_PARTIALLY_EXECUTED));
    }

    // Send statement status when error is not raised.
    if (!m_thd->is_error()) {
      m_thd->send_statement_status();
    }
  }
}

bool Regular_statement_handle::execute() {
  m_is_executed = true;
  LEX_STRING sql_text{const_cast<char *>(m_query.c_str()), m_query.length()};
  Statement_runnable stmt_runnable(sql_text);
  return execute(&stmt_runnable);
}

bool Regular_statement_handle::execute(Server_runnable *server_runnable) {
  DBUG_TRACE;

  if (m_thd->in_sub_stmt ||
      (m_thd->in_loadable_function &&
       DBUG_EVALUATE_IF("skip_statement_execution_within_UDF_check", false,
                        true))) {
    my_error(ER_STMT_EXECUTION_NOT_ALLOWED_WITHIN_SP_OR_TRG_OR_UDF, MYF(0),
             "Regular");
    return true;
  }

  free_old_result(); /* Delete all data from previous execution, if any */

  query_id_t old_query_id = m_thd->query_id;
  m_thd->set_query_id(next_query_id());

  set_thd_protocol();

  const auto saved_secondary_engine = m_thd->secondary_engine_optimization();
  m_thd->set_secondary_engine_optimization(
      Secondary_engine_optimization::PRIMARY_TENTATIVELY);

  bool rc = false;
  {
    Diagnostics_area_handler_raii da_handler(m_thd);

    Prepared_statement stmt(m_thd);
    rc = stmt.execute_server_runnable(m_thd, server_runnable);
    send_statement_status();
  }

  reset_thd_protocol();

  /*
    Protocol_local_v2 makes use of m_current_rset to keep
    track of the last result set, while adding result sets to the end.
    Reset it to point to the first result set instead.
  */
  m_current_rset = m_result_sets;

  /*
    Prepared_statement::execute_server_runnable() sets m_query as query being
    executed for the PFS events. So reset it back to the query invoking this
    method.
  */
  set_query_for_display(m_thd);

  m_thd->set_query_id(old_query_id);

  m_thd->set_secondary_engine_optimization(saved_secondary_engine);

  // This is needed as we use a single DA for all sql-callout queries within the
  // stored program
  copy_warnings();

  DEBUG_SYNC(m_thd, "wait_after_query_execution");

  return rc;
}

void Statement_handle::add_result_set(Result_set *result_set) {
  if (m_result_sets != nullptr) {
    m_current_rset->set_next(result_set);
    /* While appending, use m_current_rset as a pointer to the tail. */
    m_current_rset = result_set;
  } else
    m_current_rset = m_result_sets = result_set;
}

Statement_handle::Statement_handle(THD *thd, const char *query, size_t length)
    : m_query(query, length),
      m_warning_mem_root(key_memory_prepared_statement_main_mem_root,
                         thd->variables.query_alloc_block_size),
      m_diagnostics_area(thd->get_stmt_da()),
      m_thd(thd),
      m_result_sets(nullptr),
      m_current_rset(nullptr),
      m_expected_charset(
          const_cast<CHARSET_INFO *>(thd->variables.character_set_results)),
      m_protocol(thd, this) {}

auto Statement_handle::copy_warnings() -> void {
  m_warnings_count = m_diagnostics_area->warn_count(m_thd) -
                     m_diagnostics_area->error_count(m_thd);

  assert(alloc_root_inited(&m_warning_mem_root));

  m_warning_mem_root.Clear();
  m_warnings = static_cast<Warning *>(
      m_warning_mem_root.Alloc(sizeof(Warning) * m_warnings_count));

  Warning *warning = m_warnings;

  const Sql_condition *condition;
  Diagnostics_area::Sql_condition_iterator it =
      m_diagnostics_area->sql_conditions();

  while ((condition = it++)) {
    if (condition->severity() == Sql_condition::SL_WARNING ||
        condition->severity() == Sql_condition::SL_NOTE) {
      warning->m_code = condition->mysql_errno();
      warning->m_level = condition->severity();
      warning->m_message =
          convert_and_store(&m_warning_mem_root, condition->message_text(),
                            strlen(condition->message_text()),
                            system_charset_info, m_expected_charset);
      ++warning;
    }
  }
}

auto Statement_handle::get_warnings() -> Warning * { return m_warnings; }

void Statement_handle::free_old_result() {
  m_protocol.clear_resultset_mem_root();
  m_current_rset = m_result_sets = nullptr;
}

void Statement_handle::set_result_set(Result_set *result_set) {
  m_current_rset = m_result_sets = result_set;
}

void Statement_handle::set_thd_protocol() {
  assert(m_saved_protocol == nullptr);

  /*
    The result of a statement execution is intercepted by the member m_protocol.
    In nested statement executions, the protocol instance of a previous
    Statement_handle is already in use. Utilizing the same instance of the
    protocol might corrupt its state. To address this, the instance is removed
    from the stack and saved. The saved instance is then pushed back to the
    stack in reset_thd_protocol(). This also ensures that there is only one
    Protocol_local_v2 instance in the stack during nested statement execution.
    Which in turn, helps when pass-through is enabled for one of the statements
    in the nested statement execution. When pass-through is enabled and the
    interceptor protocol is in use, the interceptor protocol instance is popped,
    and the default THD Protocol instance is utilized.
  */
  if (m_thd->get_protocol()->type() == Protocol::PROTOCOL_LOCAL) {
    m_saved_protocol = m_thd->get_protocol();
    m_thd->pop_protocol();

    // Make sure interceptor protocol is not used when pass-through is enabled.
    assert(!is_using_thd_protocol() ||
           (m_thd->get_protocol()->type() != Protocol::PROTOCOL_LOCAL));
  }

  // Push the interceptor protocol if pass-through is *not* enabled.
  if (!is_using_thd_protocol()) m_thd->push_protocol(&m_protocol);
}

void Statement_handle::reset_thd_protocol() {
  if (!is_using_thd_protocol()) m_thd->pop_protocol();
  if (m_saved_protocol != nullptr) m_thd->push_protocol(m_saved_protocol);
  m_saved_protocol = nullptr;
}

/**
  RAII class to set query text to PFS.

  Constructor sets new query text for PFS events. New query text is
  rewritten if needed before setting it for PFS events.

  Destructor restores original query string for PFS events.
*/
class PFS_query_text_handler_raii {
 public:
  PFS_query_text_handler_raii(THD *thd, std::string *new_query) {
    assert(new_query->length() > 0);

    m_thd = thd;

    m_saved_query_string = thd->query();
    thd->set_query(new_query->c_str(), new_query->length());

    if (thd->rewritten_query().length() > 0) {
      m_saved_rewritten_query.copy(
          thd->rewritten_query()); /* purecov: inspected */
    }
    rewrite_query_if_needed(thd);

    m_saved_safe_to_display = thd->safe_to_display();
    /*
      Setting query text for PFS events during executed phase of prepared
      statement. In `Prepared_statement::prepare()` function, the query being
      prepared is initially set for the PFS events, but it is later reset back
      to the invoker query after preparation is complete. However, during the
      execute phase, rewritten query text for a query may not be readily
      available. To address this, setting the query after rewrite for PFS
      events.
    */
    set_query_for_display(thd);
  }

  ~PFS_query_text_handler_raii() {
    m_thd->set_query(m_saved_query_string);

    if (m_saved_rewritten_query.length() > 0) {
      m_thd->swap_rewritten_query(m_saved_rewritten_query);
      m_saved_rewritten_query.mem_free();
    } else {
      m_thd->reset_rewritten_query();
    }

    set_query_for_display(m_thd);
    m_thd->set_safe_display(m_saved_safe_to_display);
  }

 private:
  THD *m_thd{nullptr};
  LEX_CSTRING m_saved_query_string;
  String m_saved_rewritten_query;
  bool m_saved_safe_to_display{false};
};

bool Prepared_statement_handle::internal_prepare() {
  DBUG_TRACE;
  DBUG_PRINT("Prepared_statement_handle", ("Got query %s\n", m_query.c_str()));

  Diagnostics_area_handler_raii da_handler(m_thd, true);

  // Close the current statement and create new
  if (m_stmt) {
    internal_close(); /* purecov: inspected */
  }

  m_stmt = new Prepared_statement(m_thd);
  if (m_stmt == nullptr) return true; /* out of memory */

  m_stmt->set_sql_prepare();

  if (m_thd->stmt_map.insert(m_stmt)) {
    m_stmt = nullptr;
    /* The statement is deleted and an error is set if insert fails */
    return true;
  }

  /*
    Initially, optimize the statement for the primary storage engine.
    If an eligible secondary storage engine is found, the statement
    may be reprepared for the secondary storage engine later. */
  const auto saved_secondary_engine = m_thd->secondary_engine_optimization();
  m_thd->set_secondary_engine_optimization(
      Secondary_engine_optimization::PRIMARY_TENTATIVELY);

  /* Create PS table entry, set query text after rewrite. */
  m_stmt->m_prepared_stmt =
      MYSQL_CREATE_PS(m_stmt, m_stmt->id(), m_thd->m_statement_psi,
                      m_stmt->name().str, m_stmt->name().length, nullptr, 0);

  if (m_stmt->prepare(m_thd, m_query.c_str(), m_query.length(), nullptr)) {
    internal_close();

    m_thd->set_secondary_engine_optimization(saved_secondary_engine);

    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "PREPARE");
    return true;
  } else {
    /* send the boolean tracker in the OK packet when
       @@session_track_state_change is set to ON */
    if (m_thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
            ->is_enabled())
      m_thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->mark_as_changed(m_thd, {});
    my_ok(m_thd, 0L, 0L, "Statement prepared");
  }

  /*
    Prepapred_statement::prepare() sets the query being prepared for the PFS
    events. So reset it back to query invoking this method.
  */
  set_query_for_display(m_thd);

  m_thd->set_secondary_engine_optimization(saved_secondary_engine);

  // Set multi-result state if statement belongs to SP.
  if (m_use_thd_protocol && m_thd->sp_runtime_ctx != nullptr &&
      set_sp_multi_result_state(m_thd, m_stmt->m_lex)) {
    return true; /* purecov: inspected */
  }

  if (create_parameter_buffers()) {
    // OOM
    return true; /* purecov: inspected */
  }

  DEBUG_SYNC(m_thd, "wait_after_query_prepare");

  return false;
}

bool Prepared_statement_handle::enable_cursor() {
  assert(m_stmt->m_lex);
  assert(!m_use_thd_protocol ||
         m_thd->server_status & SERVER_MORE_RESULTS_EXISTS);

  Sql_cmd *sql_cmd = m_stmt->m_lex->m_sql_cmd;

  /*
    Note: Temporary fix to disable cursor for EXPLAIN in prepared statement
    till Bug#36332426 is resolved.
  */
  if (m_stmt->m_lex->is_explain()) return false;

  /*
    Enable cursors only when results are not directly relayed to the client and
    only command is suitable for cursors.
  */
  return (!m_use_thd_protocol && sql_cmd &&
          (sql_cmd->sql_cmd_type() == SQL_CMD_DML) &&
          (down_cast<Sql_cmd_dml *>(sql_cmd))->may_use_cursor());
}

bool Prepared_statement_handle::internal_execute() {
  DBUG_TRACE;

  Diagnostics_area_handler_raii da_handler(m_thd, true);

  // Stop if prepare is not done yet.
  if (!m_stmt) {
    /* purecov : begin inspected */
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "EXECUTE");
    return true;
    /* purecov : end */
  }

  // If execute is called again, reset the state and execute.
  if (m_stmt->m_arena.get_state() == Query_arena::STMT_EXECUTED) {
    internal_reset(false);
  }

  statement_id_to_session(m_thd);

#if defined(ENABLED_PROFILING)
  m_thd->profiling->set_query_source(m_stmt->m_query_string.str,  // TODO
                                     m_stmt->m_query_string.length);
#endif
  DBUG_PRINT("info", ("stmt: %p", m_stmt));

  assert(m_stmt->m_param_count == 0 || m_parameters != nullptr);
  // Check if value is bind to all the parameters.
  for (unsigned int idx = 0; idx < m_stmt->m_param_count; idx++) {
    if (m_parameters[idx].type == MYSQL_TYPE_INVALID) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "Prepared Statement Execute");
      return true;
    }
  }

  MYSQL_EXECUTE_PS(m_thd->m_statement_psi, m_stmt->m_prepared_stmt);

  /*
    Initially, optimize the statement for the primary storage engine.
    If an eligible secondary storage engine is found, the statement
    may be reprepared for the secondary storage engine later.
  */
  const auto saved_secondary_engine = m_thd->secondary_engine_optimization();
  m_thd->set_secondary_engine_optimization(
      Secondary_engine_optimization::PRIMARY_TENTATIVELY);

  MYSQL_SET_PS_SECONDARY_ENGINE(m_stmt->m_prepared_stmt, false);

  String expanded_query;
  expanded_query.set_charset(default_charset_info);

  // If no error happened while setting the parameters, execute statement.
  bool rc = false;
  if (!m_stmt->set_parameters(
          m_thd, &expanded_query, m_bound_new_parameter_types, m_parameters,
          Prepared_statement::enum_param_pack_type::UNPACKED)) {
    PFS_query_text_handler_raii pfs_query_text_handler(m_thd, &m_query);
    rc = m_stmt->execute_loop(m_thd, &expanded_query, enable_cursor());
    m_bound_new_parameter_types = false;
    if (!is_cursor_open()) send_statement_status();
  }

  m_thd->set_secondary_engine_optimization(saved_secondary_engine);

  sp_cache_enforce_limit(m_thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(m_thd->sp_func_cache, stored_program_cache_size);

  DEBUG_SYNC(m_thd, "wait_after_query_execution");

  return rc;
}

bool Prepared_statement_handle::internal_fetch() {
  DBUG_TRACE;
  DBUG_PRINT("Prepared_statement_handle",
             ("Asked for %zu rows\n", m_num_rows_per_fetch));

  Diagnostics_area_handler_raii da_handler(m_thd, true);

  // Stop if statement is not prepared.
  if (!m_stmt) {
    /* purecov: begin inspected */
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "FETCH");
    return true;
    /* purecov: end */
  }

  // Stop if statement is not executed or statement has no cursor.
  if (m_stmt->m_arena.get_state() != Query_arena::STMT_EXECUTED ||
      !is_cursor_open()) {
    /* purecov: begin inspected */
    my_error(ER_STMT_HAS_NO_OPEN_CURSOR, MYF(0), m_stmt->m_id);
    return true;
    /* purecov: end */
  }

  m_thd->stmt_arena = &m_stmt->m_arena;

  Server_side_cursor *cursor = m_stmt->m_cursor;
  bool rc = cursor->fetch(m_num_rows_per_fetch);

  if (rc == false) m_thd->send_statement_status();

  if (!cursor->is_open()) {
    reset_stmt_parameters(m_stmt);
  }

  m_thd->stmt_arena = m_thd;

  return rc;
}

bool Prepared_statement_handle::internal_reset() {
  return internal_reset(true);
}

bool Prepared_statement_handle::internal_reset(bool invalidate_params) {
  DBUG_TRACE;
  DBUG_PRINT("Prepared_statement_handle", ("Closing the cursor.\n"));

  if (!m_stmt) {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "RESET");
    return true;
  }

  if (is_cursor_open()) {
    m_stmt->close_cursor();
  }

  // Clear parameters from data.
  reset_stmt_parameters(m_stmt);
  if (invalidate_params) {
    for (unsigned int idx = 0; idx < m_stmt->m_param_count; idx++) {
      m_parameters[idx].type = MYSQL_TYPE_INVALID;
    }
  }

  free_old_result(); /* Delete all data from previous execution, if any */

  m_stmt->m_arena.set_state(Query_arena::STMT_PREPARED);

  query_logger.general_log_print(m_thd, m_thd->get_command(), NullS);

  return false;
}

bool Prepared_statement_handle::internal_close() {
  DBUG_TRACE;
  DBUG_PRINT("Prepared_statement_handle",
             ("Deallocating prepared statement.\n"));

  if (!m_stmt) {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "CLOSE");
    return true;
  }

  /*
    The only way currently a statement can be deallocated when it's
    in use is from within Dynamic SQL.
  */
  assert(!m_stmt->is_in_use());

  MYSQL_DESTROY_PS(m_stmt->m_prepared_stmt);

  // Close the cursor if open.
  internal_reset(false);

  m_stmt->deallocate(m_thd);
  query_logger.general_log_print(m_thd, m_thd->get_command(), NullS);

  if (m_thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->is_enabled())
    m_thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
        ->mark_as_changed(m_thd, {});

  m_stmt = nullptr;
  m_parameters = nullptr;

  return false;
}

bool Prepared_statement_handle::prepare() {
  return run([this]() { return this->internal_prepare(); });
}

bool Prepared_statement_handle::execute() {
  return run([this]() { return this->internal_execute(); });
}

bool Prepared_statement_handle::fetch() {
  return run([this]() { return this->internal_fetch(); });
}

bool Prepared_statement_handle::reset() {
  return run([this]() { return this->internal_reset(); });
}

bool Prepared_statement_handle::close() {
  return run([this]() { return this->internal_close(); });
}

template <typename Function>
bool Prepared_statement_handle::run(Function exec_func) {
  DBUG_TRACE;

  if (m_thd->in_sub_stmt ||
      (m_thd->in_loadable_function &&
       DBUG_EVALUATE_IF("skip_statement_execution_within_UDF_check", false,
                        true))) {
    my_error(ER_STMT_EXECUTION_NOT_ALLOWED_WITHIN_SP_OR_TRG_OR_UDF, MYF(0),
             "Prepared");
    return true;
  }

  query_id_t old_query_id = m_thd->query_id;
  m_thd->set_query_id(next_query_id());

  set_thd_protocol();

  // m_stmt uses its own query arena. But cleanup is done after restoring the
  // query arena in m_stmt methods. Hence, new query arena used here to save
  // query arena state before this stage.
  Query_arena arena_backup,
      execute_arena(m_thd->mem_root, Query_arena::STMT_INITIALIZED);
  m_thd->swap_query_arena(execute_arena, &arena_backup);

  Query_arena *saved_stmt_arena = m_thd->stmt_arena;
  m_thd->stmt_arena = &arena_backup;

  Item_change_list save_change_list;
  m_thd->change_list.move_elements_to(&save_change_list);

  bool error = exec_func();

  m_thd->cleanup_after_query();

  save_change_list.move_elements_to(&m_thd->change_list);

  // Restore query arena.
  m_thd->stmt_arena = saved_stmt_arena;
  m_thd->swap_query_arena(arena_backup, &execute_arena);

  /*
    Protocol_local_v2 makes use of m_current_rset to keep
    track of the last result set, while adding result sets to the end.
    Reset it to point to the first result set instead.
  */
  m_current_rset = m_result_sets;

  reset_thd_protocol();

  m_thd->set_query_id(old_query_id);
  copy_warnings();

  return error;
}

bool Prepared_statement_handle::create_parameter_buffers() {
  if (m_stmt == nullptr || m_parameters != nullptr) {
    /* purecov: begin inspected */
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null",
             "CREATE_PARAMETER_BUFFERS");
    return true;
    /* purecov: end */
  }

  uint param_count = m_stmt->m_param_count;

  if (param_count == 0) {
    return false;
  }

  m_parameters = static_cast<PS_PARAM *>(
      m_parameter_mem_root.Alloc(param_count * sizeof(PS_PARAM)));

  m_parameter_buffer_max = static_cast<ulong *>(
      m_parameter_mem_root.Alloc(param_count * sizeof(ulong)));

  if (m_parameters == nullptr) return true;

  memset(m_parameters, 0, sizeof(PS_PARAM) * param_count);
  for (unsigned int idx = 0; idx < param_count; idx++) {
    m_parameters[idx].type = MYSQL_TYPE_INVALID;
  }

  return false;
}

bool Prepared_statement_handle::set_parameter(
    uint idx, bool is_null, enum_field_types type, bool is_unsigned,
    const void *data, unsigned long data_length, const char *name,
    unsigned long name_length) {
  if (m_stmt == nullptr || m_parameters == nullptr ||
      m_parameter_buffer_max == nullptr) {
    /* purecov: begin inspected */
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "SET_PARAMETER");
    return true;
    /* purecov: end */
  }

  if (idx >= m_stmt->m_param_count) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "Parameter index", "statement");
    return true;
  }

  // Copy all the properties;
  m_parameters[idx].null_bit = is_null;
  m_parameters[idx].type = type;
  m_parameters[idx].unsigned_type = is_unsigned;
  m_parameters[idx].length = data_length;
  m_parameters[idx].name_length = name_length;

  if (name != nullptr) {
    // Setting name for a parameter is not supported for now.
    /* purecov: begin inspected */
    m_parameters[idx].name = reinterpret_cast<const unsigned char *>(
        strmake_root(&m_parameter_mem_root, name, name_length));
    /* purecov: end */
  }

  if (is_null) return false;

  // Copy application buffer value into parameter buffer
  auto src_data = static_cast<const char *>(data);
  auto dest_data = const_cast<unsigned char *>(m_parameters[idx].value);
  if (dest_data != nullptr && data_length <= m_parameter_buffer_max[idx]) {
    // Copy the value into existing buffer.
    memcpy(dest_data, src_data, data_length);
  } else {
    // Allocate and copy the value, using m_stmt mem_root
    m_parameters[idx].value = reinterpret_cast<const unsigned char *>(
        strmake_root(&m_parameter_mem_root, src_data, data_length));
    m_parameter_buffer_max[idx] = data_length;
  }

  m_bound_new_parameter_types = true;

  return false;
}

Item_param *Prepared_statement_handle::get_parameter(size_t index) {
  if (m_stmt == nullptr) {
    /* purecov: begin inspected */
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 4, "null", "GET_PARAMETER");
    return nullptr;
    /* purecov: end */
  }
  if (index >= m_stmt->m_param_count) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "Parameter index", "statement");
    return nullptr;
  }
  return m_stmt->m_param_array[index];
}

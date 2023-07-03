/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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
  @file sql/sql_select.cc

  @brief Evaluate query expressions, throughout resolving, optimization and
         execution.

  @defgroup Query_Optimizer  Query Optimizer
  @{
*/

#include "sql/sql_select.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>

#include "field_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "mem_root_deque.h"  // mem_root_deque
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_byteorder.h"  // int8store, uint8korr
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_pointer_arithmetic.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql-common/json_dom.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // *_ACL
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/enum_query_type.h"
#include "sql/error_handler.h"  // Ignore_error_handler
#include "sql/field.h"
#include "sql/filesort.h"  // filesort_free_buffers
#include "sql/handler.h"
#include "sql/intrusive_list_iterator.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_json_func.h"
#include "sql/item_subselect.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/iterators/row_iterator.h"
#include "sql/iterators/sorting_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/replace_item.h"
#include "sql/key.h"  // key_copy, key_cmp, key_cmp_if_same
#include "sql/key_spec.h"
#include "sql/lock.h"  // mysql_unlock_some_tables,
#include "sql/my_decimal.h"
#include "sql/mysqld.h"  // stage_init
#include "sql/nested_join.h"
#include "sql/opt_explain.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_hints.h"  // hint_key_state()
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_node_base.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/set_var.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_do.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_insert.h"       // Sql_cmd_insert_base
#include "sql/sql_join_buffer.h"  // JOIN_CACHE
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // bind_fields
#include "sql/sql_planner.h"    // calculate_condition_filter
#include "sql/sql_resolver.h"
#include "sql/sql_test.h"       // misc. debug printing utilities
#include "sql/sql_timer.h"      // thd_timer_set
#include "sql/sql_tmp_table.h"  // tmp tables
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/thd_raii.h"
#include "sql/window.h"  // ignore_gaf_const_opt
#include "sql_string.h"
#include "template_utils.h"
#include "thr_lock.h"

using std::max;
using std::min;

static store_key *get_store_key(THD *thd, Item *val, table_map used_tables,
                                table_map const_tables,
                                const KEY_PART_INFO *key_part, uchar *key_buff,
                                uint maybe_null);

using Global_tables_iterator =
    IntrusiveListIterator<Table_ref, &Table_ref::next_global>;

/// A list interface over the Table_ref::next_global pointer.
using Global_tables_list = IteratorContainer<Global_tables_iterator>;

/**
   Check whether the statement is a SHOW command using INFORMATION_SCHEMA system
   views.

   @param  thd   Thread (session) context.

   @returns true if command uses INFORMATION_SCHEMA system view, false otherwise
*/
inline bool is_show_cmd_using_system_view(THD *thd) {
  return sql_command_flags[thd->lex->sql_command] & CF_SHOW_USES_SYSTEM_VIEW;
}

/**
  Get the maximum execution time for a statement.

  @return Length of time in milliseconds.

  @remark A zero timeout means that no timeout should be
          applied to this particular statement.

*/
static inline ulong get_max_execution_time(THD *thd) {
  return (thd->lex->max_execution_time ? thd->lex->max_execution_time
                                       : thd->variables.max_execution_time);
}

/**
  Check whether max statement time is applicable to statement or not.


  @param  thd   Thread (session) context.

  @return true  if max statement time is applicable to statement
  @return false otherwise.
*/
static inline bool is_timer_applicable_to_statement(THD *thd) {
  /*
    The following conditions must be met:
      - is SELECT statement.
      - timer support is implemented and it is initialized.
      - statement is not made by the slave threads.
      - timer is not set for statement
      - timer out value of is set
      - SELECT statement is not from any stored programs.
  */
  return (thd->lex->sql_command == SQLCOM_SELECT &&
          (have_statement_timeout == SHOW_OPTION_YES) && !thd->slave_thread &&
          !thd->timer &&
          (thd->lex->max_execution_time || thd->variables.max_execution_time) &&
          !thd->sp_runtime_ctx);
}

/**
  Set the time until the currently running statement is aborted.

  @param  thd   Thread (session) context.

  @return true if the timer was armed.
*/

bool set_statement_timer(THD *thd) {
  ulong max_execution_time = get_max_execution_time(thd);

  /**
    whether timer can be set for the statement or not should be checked before
    calling set_statement_timer function.
  */
  assert(is_timer_applicable_to_statement(thd) == true);
  assert(thd->timer == nullptr);

  thd->timer = thd_timer_set(thd, thd->timer_cache, max_execution_time);
  thd->timer_cache = nullptr;

  if (thd->timer)
    thd->status_var.max_execution_time_set++;
  else
    thd->status_var.max_execution_time_set_failed++;

  return thd->timer;
}

/**
  Deactivate the timer associated with the statement that was executed.

  @param  thd   Thread (session) context.
*/

void reset_statement_timer(THD *thd) {
  assert(thd->timer);
  /* Cache the timer object if it can be reused. */
  thd->timer_cache = thd_timer_reset(thd->timer);
  thd->timer = nullptr;
}

/**
 * Checks if a query reads a column that is _not_ available in the secondary
 * engine (i.e. a column defined with NOT SECONDARY).
 *
 * @param lex Parse tree descriptor.
 *
 * @return True if at least one of the read columns is not in the secondary
 * engine, false otherwise.
 */
static bool reads_not_secondary_columns(const LEX *lex) {
  // Check all read base tables.
  const Table_ref *tl = lex->query_tables;
  // For INSERT INTO SELECT statements, the table to insert into does not have
  // to have a secondary engine. This table is always first in the list.
  if (lex->sql_command == SQLCOM_INSERT_SELECT && tl != nullptr)
    tl = tl->next_global;
  for (; tl != nullptr; tl = tl->next_global) {
    if (tl->is_placeholder()) continue;

    // Check all read columns of table.
    for (unsigned int i = bitmap_get_first_set(tl->table->read_set);
         i != MY_BIT_NONE; i = bitmap_get_next_set(tl->table->read_set, i)) {
      if (tl->table->field[i]->is_flag_set(NOT_SECONDARY_FLAG)) {
        Opt_trace_context *trace = &lex->thd->opt_trace;
        if (trace->is_started()) {
          std::string message("");
          message.append("Column ");
          message.append(tl->table->field[i]->field_name);
          message.append(" is marked as NOT SECONDARY.");
          Opt_trace_object trace_wrapper(trace);
          Opt_trace_object oto(trace, "secondary_engine_not_used");
          oto.add_alnum("reason", message.c_str());
        }
        return true;
      }
    }
  }

  return false;
}

bool validate_use_secondary_engine(const LEX *lex) {
  const THD *thd = lex->thd;
  const Sql_cmd *sql_cmd = lex->m_sql_cmd;
  // Ensure that all read columns are in the secondary engine.
  if (sql_cmd->using_secondary_storage_engine()) {
    if (reads_not_secondary_columns(lex)) {
      my_error(ER_SECONDARY_ENGINE, MYF(0),
               "One or more read columns are marked as NOT SECONDARY");
      return true;
    }
    return false;
  }

  // A query must be executed in secondary engine if these conditions are met:
  //
  // 1) use_secondary_engine is FORCED
  // and either
  // 2) Is a SELECT statement that accesses one or more base tables.
  // or
  // 3) Is an INSERT SELECT or CREATE TABLE AS SELECT statement that accesses
  // two or more base tables
  if (thd->variables.use_secondary_engine == SECONDARY_ENGINE_FORCED &&  // 1
      ((sql_cmd->sql_command_code() == SQLCOM_SELECT &&
        lex->table_count >= 1) ||  // 2
       ((sql_cmd->sql_command_code() == SQLCOM_INSERT_SELECT ||
         sql_cmd->sql_command_code() == SQLCOM_CREATE_TABLE) &&
        lex->table_count >= 2))) {  // 3
    my_error(
        ER_SECONDARY_ENGINE, MYF(0),
        "use_secondary_engine is FORCED but query could not be executed in "
        "secondary engine");
    return true;
  }

  return false;
}

bool Sql_cmd_dml::prepare(THD *thd) {
  DBUG_TRACE;

  bool error_handler_active = false;

  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;

  // @todo: Move this to constructor?
  lex = thd->lex;

  // Parser may have assigned a specific query result handler
  result = lex->result;

  assert(!is_prepared());

  assert(!lex->unit->is_prepared() && !lex->unit->is_optimized() &&
         !lex->unit->is_executed());

  /*
    Constant folding could cause warnings during preparation. Make
    sure they are promoted to errors when strict mode is enabled.
  */
  if (is_data_change_stmt() && needs_explicit_preparation()) {
    // Push ignore / strict error handler
    if (lex->is_ignore()) {
      thd->push_internal_handler(&ignore_handler);
      error_handler_active = true;
    } else if (thd->is_strict_mode()) {
      thd->push_internal_handler(&strict_handler);
      error_handler_active = true;
    }
  }

  // Perform a coarse statement-specific privilege check.
  if (precheck(thd)) goto err;

  // Trigger out_of_memory condition inside open_tables_for_query()
  DBUG_EXECUTE_IF("sql_cmd_dml_prepare__out_of_memory",
                  DBUG_SET("+d,simulate_out_of_memory"););
  /*
    Open tables and expand views.
    During prepare of query (not as part of an execute), acquire only
    S metadata locks instead of SW locks to be compatible with concurrent
    LOCK TABLES WRITE and global read lock.
  */
  if (open_tables_for_query(
          thd, lex->query_tables,
          needs_explicit_preparation() ? MYSQL_OPEN_FORCE_SHARED_MDL : 0)) {
    if (thd->is_error())  // @todo - dictionary code should be fixed
      goto err;
    if (error_handler_active) thd->pop_internal_handler();
    lex->cleanup(false);
    return true;
  }
  DEBUG_SYNC(thd, "after_open_tables");
#ifndef NDEBUG
  if (sql_command_code() == SQLCOM_SELECT) DEBUG_SYNC(thd, "after_table_open");
#endif

  lex->using_hypergraph_optimizer =
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);

  if (lex->set_var_list.elements && resolve_var_assignments(thd, lex))
    goto err; /* purecov: inspected */

  {
    Prepare_error_tracker tracker(thd);
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    Enable_derived_merge_guard derived_merge_guard(
        thd, is_show_cmd_using_system_view(thd));

    if (prepare_inner(thd)) goto err;
    if (needs_explicit_preparation() && result != nullptr) {
      result->cleanup();
    }
    if (!is_regular()) {
      if (save_cmd_properties(thd)) goto err;
      lex->set_secondary_engine_execution_context(nullptr);
    }
    set_prepared();
  }

  // Pop ignore / strict error handler
  if (error_handler_active) thd->pop_internal_handler();

  // Revertable changes are not supported during preparation
  assert(thd->change_list.is_empty());

  return false;

err:
  assert(thd->is_error());
  DBUG_PRINT("info", ("report_error: %d", thd->is_error()));

  lex->set_secondary_engine_execution_context(nullptr);

  if (error_handler_active) thd->pop_internal_handler();

  if (result != nullptr) result->cleanup();

  lex->cleanup(false);

  return true;
}

bool Sql_cmd_select::accept(THD *thd, Select_lex_visitor *visitor) {
  return thd->lex->unit->accept(visitor);
}

const MYSQL_LEX_CSTRING *Sql_cmd_select::eligible_secondary_storage_engine()
    const {
  return get_eligible_secondary_engine();
}

/**
  Prepare a SELECT statement.
*/

bool Sql_cmd_select::prepare_inner(THD *thd) {
  if (lex->is_explain()) {
    /*
      Always use Query_result_send for EXPLAIN, even if it's an EXPLAIN for
      SELECT ... INTO OUTFILE: a user application should be able to prepend
      EXPLAIN to any query and receive output for it, even if the query itself
      redirects the output.
    */
    result = new (thd->mem_root) Query_result_send();
    if (result == nullptr) return true; /* purecov: inspected */
  } else {
    if (result == nullptr) {
      if (sql_command_code() == SQLCOM_SELECT)
        result = new (thd->mem_root) Query_result_send();
      else if (sql_command_code() == SQLCOM_DO)
        result = new (thd->mem_root) Query_result_do();
      else  // Currently assumed to be a SHOW command
        result = new (thd->mem_root) Query_result_send();

      if (result == nullptr) return true; /* purecov: inspected */
    }
  }

  Query_expression *const unit = lex->unit;
  Query_block *parameters = unit->global_parameters();
  if (!parameters->has_limit()) {
    parameters->m_use_select_limit = true;
  }

  if (unit->is_simple()) {
    Query_block *const select = unit->first_query_block();
    select->context.resolve_in_select_list = true;
    select->set_query_result(result);
    unit->set_query_result(result);
    // Unlock the table as soon as possible, so don't set SELECT_NO_UNLOCK.
    select->make_active_options(0, 0);

    if (select->prepare(thd, nullptr)) return true;

    unit->set_prepared();
  } else {
    // If we have multiple query blocks, don't unlock and re-lock
    // tables between each each of them.
    if (unit->prepare(thd, result, nullptr, SELECT_NO_UNLOCK, 0)) return true;
  }

  return false;
}

bool Sql_cmd_dml::execute(THD *thd) {
  DBUG_TRACE;

  lex = thd->lex;

  Query_expression *const unit = lex->unit;

  bool statement_timer_armed = false;
  bool error_handler_active = false;

  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;

  // If statement is preparable, it must be prepared
  assert(owner() == nullptr || is_prepared());
  // If statement is regular, it must be unprepared
  assert(!is_regular() || !is_prepared());
  // If statement is part of SP, it can be both prepared and unprepared.

  // If a timer is applicable to statement, then set it.
  if (is_timer_applicable_to_statement(thd))
    statement_timer_armed = set_statement_timer(thd);

  if (is_data_change_stmt()) {
    // Push ignore / strict error handler
    if (lex->is_ignore()) {
      thd->push_internal_handler(&ignore_handler);
      error_handler_active = true;
      /*
        UPDATE IGNORE can be unsafe. We therefore use row based
        logging if mixed or row based logging is available.
        TODO: Check if the order of the output of the select statement is
        deterministic. Waiting for BUG#42415
      */
      if (lex->sql_command == SQLCOM_UPDATE)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UPDATE_IGNORE);
    } else if (thd->is_strict_mode()) {
      thd->push_internal_handler(&strict_handler);
      error_handler_active = true;
    }
  }

  if (!is_prepared()) {
    if (prepare(thd)) goto err;
  } else {
    /*
      Prepared statement, open tables referenced in statement and check
      privileges for it.
    */
    cleanup(thd);
    if (open_tables_for_query(thd, lex->query_tables, 0)) goto err;
#ifndef NDEBUG
    if (sql_command_code() == SQLCOM_SELECT)
      DEBUG_SYNC(thd, "after_table_open");
#endif
    // Bind table and field information
    if (restore_cmd_properties(thd)) return true;
    if (check_privileges(thd)) goto err;

    if (m_lazy_result) {
      Prepared_stmt_arena_holder ps_arena_holder(thd);

      if (result->prepare(thd, *unit->get_unit_column_types(), unit)) goto err;
      m_lazy_result = false;
    }
  }

  if (validate_use_secondary_engine(lex)) goto err;

  lex->set_exec_started();

  DBUG_EXECUTE_IF("use_attachable_trx",
                  thd->begin_attachable_ro_transaction(););

  THD_STAGE_INFO(thd, stage_init);

  thd->clear_current_query_costs();

  // Replication may require extra check of data change statements
  if (is_data_change_stmt() && run_before_dml_hook(thd)) goto err;

  // Revertable changes are not supported during preparation
  assert(thd->change_list.is_empty());

  assert(!lex->is_query_tables_locked());
  /*
    Locking of tables is done after preparation but before optimization.
    This allows to do better partition pruning and avoid locking unused
    partitions. As a consequence, in such a case, prepare stage can rely only
    on metadata about tables used and not data from them.
  */
  if (!is_empty_query()) {
    if (lock_tables(thd, lex->query_tables, lex->table_count, 0)) goto err;
  }

  // Perform statement-specific execution
  if (execute_inner(thd)) goto err;

  // Count the number of statements offloaded to a secondary storage engine.
  if (using_secondary_storage_engine() && lex->unit->is_executed())
    ++thd->status_var.secondary_engine_execution_count;

  assert(!thd->is_error());

  // Pop ignore / strict error handler
  if (error_handler_active) thd->pop_internal_handler();

  THD_STAGE_INFO(thd, stage_end);

  // Do partial cleanup (preserve plans for EXPLAIN).
  lex->cleanup(false);
  lex->clear_values_map();
  lex->set_secondary_engine_execution_context(nullptr);

  // Perform statement-specific cleanup for Query_result
  if (result != nullptr) result->cleanup();

  thd->save_current_query_costs();

  thd->update_previous_found_rows();

  DBUG_EXECUTE_IF("use_attachable_trx", thd->end_attachable_transaction(););

  if (statement_timer_armed && thd->timer) reset_statement_timer(thd);

  /*
    This sync point is normally right before thd->query_plan is reset, so
    EXPLAIN FOR CONNECTION can catch the plan. It is copied here as
    after unprepare() EXPLAIN considers the query as "not ready".
    @todo remove in WL#6570 when unprepare() is gone.
  */
  DEBUG_SYNC(thd, "before_reset_query_plan");

  return false;

err:
  assert(thd->is_error() || thd->killed);
  DBUG_PRINT("info", ("report_error: %d", thd->is_error()));
  THD_STAGE_INFO(thd, stage_end);

  lex->cleanup(false);
  lex->clear_values_map();
  lex->set_secondary_engine_execution_context(nullptr);

  // Abort and cleanup the result set (if it has been prepared).
  if (result != nullptr) {
    result->abort_result_set(thd);
    result->cleanup();
  }
  if (error_handler_active) thd->pop_internal_handler();

  if (statement_timer_armed && thd->timer) reset_statement_timer(thd);

  /*
    There are situations where we want to know the cost of a query that
    has failed during execution, e.g because of a timeout.
  */
  thd->save_current_query_costs();

  DBUG_EXECUTE_IF("use_attachable_trx", thd->end_attachable_transaction(););

  return thd->is_error();
}

void accumulate_statement_cost(const LEX *lex) {
  Opt_trace_context *trace = &lex->thd->opt_trace;
  Opt_trace_disable_I_S disable_trace(trace, true);

  double total_cost = 0.0;
  for (const Query_block *query_block = lex->all_query_blocks_list;
       query_block != nullptr;
       query_block = query_block->next_select_in_list()) {
    if (query_block->join == nullptr) continue;

    // Get the cost of this query block.
    double query_block_cost = query_block->join->best_read;

    // If it is a non-cacheable subquery, estimate how many times it
    // needs to be executed, and adjust the cost accordingly.
    const Item_subselect *item = query_block->master_query_expression()->item;
    if (item != nullptr && !query_block->is_cacheable())
      query_block_cost *= calculate_subquery_executions(item, trace);

    total_cost += query_block_cost;
  }
  lex->thd->m_current_query_cost = total_cost;
}

static bool has_external_table(Table_ref *query_tables) {
  for (Table_ref *ref = query_tables; ref != nullptr; ref = ref->next_global) {
    if (ref->table != nullptr &&
        (ref->table->file->ht->flags & HTON_SUPPORTS_EXTERNAL_SOURCE) != 0 &&
        ref->table->s->has_secondary_engine()) {
      return true;
    }
  }
  return false;
}

/**
  Checks if a query should be retried using a secondary storage engine.

  @param thd      the current session

  @retval true   if the statement should be retried in a secondary engine
  @retval false  if the statement should not be retried
*/
static bool retry_with_secondary_engine(THD *thd) {
  // Only retry if the current statement is being tentatively
  // optimized for the primary engine.
  if (thd->secondary_engine_optimization() !=
      Secondary_engine_optimization::PRIMARY_TENTATIVELY)
    return false;

  Sql_cmd *const sql_cmd = thd->lex->m_sql_cmd;
  assert(!sql_cmd->using_secondary_storage_engine());

  // Don't retry if there is a property of the statement that prevents use of
  // secondary engines.
  if (sql_cmd->eligible_secondary_storage_engine() == nullptr) {
    sql_cmd->disable_secondary_storage_engine();
    return false;
  }

  // Don't retry if it's already determined that the statement should not be
  // executed by a secondary engine.
  if (sql_cmd->secondary_storage_engine_disabled()) {
    return false;
  }

  // Don't retry if there is a property of the environment that prevents use of
  // secondary engines.
  if (!thd->is_secondary_storage_engine_eligible()) {
    return false;
  }

  // Only attempt to use the secondary engine if the estimated cost of the query
  // is higher than the specified cost threshold.
  // We allow any query to be executed in the secondary_engine when it involves
  // external tables.
  if (!has_external_table(thd->lex->query_tables) &&
      (thd->m_current_query_cost <=
       thd->variables.secondary_engine_cost_threshold)) {
    Opt_trace_context *const trace = &thd->opt_trace;
    if (trace->is_started()) {
      Opt_trace_object wrapper(trace);
      Opt_trace_object oto(trace, "secondary_engine_not_used");
      oto.add_alnum("reason",
                    "The estimated query cost does not exceed "
                    "secondary_engine_cost_threshold.");
      oto.add("cost", thd->m_current_query_cost);
      oto.add("threshold", thd->variables.secondary_engine_cost_threshold);
    }
    return false;
  }

  return true;
}

bool optimize_secondary_engine(THD *thd) {
  if (retry_with_secondary_engine(thd)) {
    thd->get_stmt_da()->reset_diagnostics_area();
    thd->get_stmt_da()->set_error_status(thd, ER_PREPARE_FOR_SECONDARY_ENGINE);
    return true;
  }

  if (thd->secondary_engine_optimization() ==
          Secondary_engine_optimization::PRIMARY_TENTATIVELY &&
      thd->lex->m_sql_cmd != nullptr &&
      thd->lex->m_sql_cmd->is_optional_transform_prepared()) {
    // A previous preparation did a secondary engine specific transform,
    // and this transform wasn't requested for the primary engine (in
    // 'optimizer_switch'), so in this new execution we need to reprepare for
    // the primary engine without the optional transform, for likely better
    // performance.
    thd->lex->m_sql_cmd->set_optional_transform_prepared(false);
    thd->get_stmt_da()->reset_diagnostics_area();
    thd->get_stmt_da()->set_error_status(thd, ER_PREPARE_FOR_PRIMARY_ENGINE);
    return true;
  }

  const handlerton *secondary_engine = thd->lex->m_sql_cmd->secondary_engine();

  /* When there is a secondary engine hook, return its return value,
   * otherwise return false (success). */
  return secondary_engine != nullptr &&
         secondary_engine->optimize_secondary_engine != nullptr &&
         secondary_engine->optimize_secondary_engine(thd, thd->lex);
}

/**
  Execute a DML statement.
  This is the default implementation for a DML statement and uses a
  nested-loop join processor per outer-most query block.
  The implementation is split in two: One for query expressions containing
  a single query block and one for query expressions containing multiple
  query blocks combined with UNION.
*/

bool Sql_cmd_dml::execute_inner(THD *thd) {
  Query_expression *unit = lex->unit;

  if (unit->optimize(thd, /*materialize_destination=*/nullptr,
                     /*create_iterators=*/true, /*finalize_access_paths=*/true))
    return true;

  // Calculate the current statement cost.
  accumulate_statement_cost(lex);

  // Perform secondary engine optimizations, if needed.
  if (optimize_secondary_engine(thd)) return true;

  // We know by now that execution will complete (successful or with error)
  lex->set_exec_completed();
  if (lex->is_explain()) {
    if (explain_query(thd, thd, unit)) return true; /* purecov: inspected */
  } else {
    if (unit->execute(thd)) return true;
  }

  return false;
}

bool Sql_cmd_dml::restore_cmd_properties(THD *thd) {
  lex->restore_cmd_properties();
  bind_fields(thd->stmt_arena->item_list());

  return false;
}

bool Sql_cmd_dml::save_cmd_properties(THD *thd) {
  return lex->save_cmd_properties(thd);
}

Query_result *Sql_cmd_dml::query_result() const {
  assert(is_prepared());
  return lex->unit->query_result() != nullptr
             ? lex->unit->query_result()
             : lex->unit->first_query_block()->query_result();
}

void Sql_cmd_dml::set_query_result(Query_result *result_arg) {
  result = result_arg;
  Query_expression *unit = lex->unit;
  unit->query_term()->query_block()->set_query_result(result);
  unit->set_query_result(result);
}

/**
  Performs access check for the locking clause, if present.

  @param thd Current session, used for checking access and raising error.

  @param tables Tables in the query's from clause.

  @retval true There was a locking clause and access was denied. An error has
  been raised.

  @retval false There was no locking clause or access was allowed to it. This
  is always returned in an embedded build.
*/
static bool check_locking_clause_access(THD *thd, Global_tables_list tables) {
  for (Table_ref *table_ref : tables)
    if (table_ref->lock_descriptor().type == TL_WRITE) {  // i.e. FOR UPDATE
      bool access_is_granted = false;
      /*
        If either of these privileges is present along with SELECT, access is
        granted.
      */
      for (uint allowed_priv : {UPDATE_ACL, DELETE_ACL, LOCK_TABLES_ACL}) {
        ulong priv = SELECT_ACL | allowed_priv;
        if (!check_table_access(thd, priv, table_ref, false, 1, true)) {
          access_is_granted = true;
          // No need to check for other privileges for this table.
          // However, we still need to check privileges for other tables.
          break;
        }
      }

      if (!access_is_granted) {
        const Security_context *sctx = thd->security_context();

        my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                 "SELECT with locking clause", sctx->priv_user().str,
                 sctx->host_or_ip().str, table_ref->get_table_name());

        return true;
      }
    }

  return false;
}

/**
  Perform an authorization precheck for an unprepared SELECT statement.

  This function will check that we have some privileges to all involved tables
  of the query (and possibly to other entities).
*/

bool Sql_cmd_select::precheck(THD *thd) {
  /*
    lex->exchange != NULL implies SELECT .. INTO OUTFILE and this
    requires FILE_ACL access.
  */
  bool check_file_acl =
      (lex->result != nullptr && lex->result->needs_file_privilege());

  /*
    Check following,

    1) Check FILE privileges for current user who runs a query if needed.

    2) Check privileges for every user specified as a definer for a view or
       check privilege to access any DB in case a table wasn't specified.
       Although calling of check_access() when no tables are specified results
       in returning false value immediately, this call has important side
       effect: the counter 'stage/sql/checking permissions' in performance
       schema is incremented. Therefore, this function is called in order to
       save backward compatibility.

    3) Performs access check for the locking clause, if present.

  */
  Table_ref *tables = lex->query_tables;

  if (check_file_acl && check_global_access(thd, FILE_ACL)) return true;

  bool res;
  if (tables)
    res = check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false);
  else
    res = check_access(thd, SELECT_ACL, any_db, nullptr, nullptr, false, false);

  return res || check_locking_clause_access(thd, Global_tables_list(tables));
}

/**
  Perform an authorization check for a prepared SELECT statement.
*/

bool Sql_cmd_select::check_privileges(THD *thd) {
  /*
    lex->exchange != nullptr implies SELECT .. INTO OUTFILE and this
    requires FILE_ACL access.
  */
  if (result->needs_file_privilege() &&
      check_access(thd, FILE_ACL, any_db, nullptr, nullptr, false, false))
    return true;

  if (check_all_table_privileges(thd)) return true;

  if (check_locking_clause_access(thd, Global_tables_list(lex->query_tables)))
    return true;

  Query_expression *const unit = lex->unit;

  for (auto qt : unit->query_terms<>())
    if (qt->query_block()->check_column_privileges(thd)) return true;

  return false;
}

bool Sql_cmd_dml::check_all_table_privileges(THD *thd) {
  // Check for all possible DML privileges

  const Table_ref *const first_not_own_table = thd->lex->first_not_own_table();
  for (Table_ref *tr = lex->query_tables; tr != first_not_own_table;
       tr = tr->next_global) {
    if (tr->is_internal())  // No privilege check required for internal tables
      continue;
    // Calculated wanted privilege based on how table/view is used:
    ulong want_privilege = 0;
    if (tr->is_inserted()) {
      want_privilege |= INSERT_ACL;
    }
    if (tr->is_updated()) {
      want_privilege |= UPDATE_ACL;
    }
    if (tr->is_deleted()) {
      want_privilege |= DELETE_ACL;
    }
    if (want_privilege == 0) {
      want_privilege = SELECT_ACL;
    }
    if (tr->referencing_view == nullptr) {
      // This is a base table
      if (check_single_table_access(thd, want_privilege, tr, false))
        return true;
    } else {
      // This is a view, set handler for transformation of errors
      Internal_error_handler_holder<View_error_handler, Table_ref> view_handler(
          thd, true, tr);
      for (Table_ref *t = tr; t->referencing_view; t = t->referencing_view) {
        if (check_single_table_access(thd, want_privilege, t, false))
          return true;
      }
    }
  }
  return false;
}
const MYSQL_LEX_CSTRING *Sql_cmd_dml::get_eligible_secondary_engine() const {
  // Don't use secondary storage engines for statements that call stored
  // routines.
  if (lex->uses_stored_routines()) return nullptr;
  // Now check if the opened tables are available in a secondary
  // storage engine. Only use the secondary tables if all the tables
  // have a secondary tables, and they are all in the same secondary
  // storage engine.
  const LEX_CSTRING *secondary_engine = nullptr;
  const Table_ref *tl = lex->query_tables;

  if (lex->sql_command == SQLCOM_INSERT_SELECT && tl != nullptr) {
    // If table from Table_ref is either view or derived table then
    // do not perform INSERT AS SELECT.
    if (tl->is_view_or_derived()) return nullptr;
    // For INSERT INTO SELECT statements, the table to insert into does not have
    // to have a secondary engine. This table is always first in the list.
    tl = tl->next_global;
  }
  for (; tl != nullptr; tl = tl->next_global) {
    // Schema tables are not available in secondary engines.
    if (tl->schema_table != nullptr) return nullptr;

    // We're only interested in base tables.
    if (tl->is_placeholder()) continue;

    assert(!tl->table->s->is_secondary_engine());
    // If not in a secondary engine
    if (!tl->table->s->has_secondary_engine()) return nullptr;
    // Compare two engine names using the system collation.
    auto equal = [](const LEX_CSTRING &s1, const LEX_CSTRING &s2) {
      return system_charset_info->coll->strnncollsp(
                 system_charset_info,
                 pointer_cast<const unsigned char *>(s1.str), s1.length,
                 pointer_cast<const unsigned char *>(s2.str), s2.length) == 0;
    };

    if (secondary_engine == nullptr) {
      // First base table. Save its secondary engine name for later.
      secondary_engine = &tl->table->s->secondary_engine;
    } else if (!equal(*secondary_engine, tl->table->s->secondary_engine)) {
      // In a different secondary engine than the previous base tables.
      return nullptr;
    }
  }

  return secondary_engine;
}

/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  All tables must be opened.
*****************************************************************************/

/**
  @brief Check if two items are compatible wrt. materialization.

  @param outer Expression from outer query
  @param inner Expression from inner query

  @retval true   If subquery types allow materialization.
  @retval false  Otherwise.

  @note the purpose is similar to that of comparable_in_index().
*/

bool types_allow_materialization(Item *outer, Item *inner) {
  auto res_outer = outer->result_type();
  auto res_inner = inner->result_type();
  // Materialization of rows nested inside rows is not currently supported.
  if (res_outer == ROW_RESULT || res_inner == ROW_RESULT) return false;
  bool num_outer = res_outer == INT_RESULT || res_outer == REAL_RESULT ||
                   res_outer == DECIMAL_RESULT;
  bool num_inner = res_inner == INT_RESULT || res_inner == REAL_RESULT ||
                   res_inner == DECIMAL_RESULT;
  /*
    Materialization uses index lookup which implicitly converts the type of
    res_outer into that of res_inner.
    However, this can be done only if it respects rules in:
    https://dev.mysql.com/doc/refman/8.0/en/type-conversion.html
    https://dev.mysql.com/doc/refman/8.0/en/date-and-time-type-conversion.html
    Those rules say that, generally, if types differ, we convert them to
    REAL.
    So, looking up into a number is ok: outer will be converted to
    number. Collations don't matter.
    This covers e.g. looking up INT into DECIMAL, CHAR into INT, DECIMAL into
    BIT.
  */
  if (num_inner) return true;
  // Conversely, looking up one number into a non-number is not possible.
  if (num_outer) return false;
  /*
    Arguments are strings or temporal.
    Require same collation for correct comparison.
  */
  assert(res_outer == STRING_RESULT && res_inner == STRING_RESULT);
  if (outer->collation.collation != inner->collation.collation) return false;
  bool temp_outer = outer->is_temporal();
  bool temp_inner = inner->is_temporal();
  /*
    Same logic as for numbers.
    As explained in add_key_field(), IndexedTimeComparedToDate is not working;
    see also field_time_cmp_date().
    @todo unify all pieces of code which deal with this same problem.
  */
  if (temp_inner) {
    if (!inner->is_temporal_with_date())
      return temp_outer && !outer->is_temporal_with_date();
    return true;
  }
  if (temp_outer) return false;
  return true;
}

/*
  Check if the table's rowid is included in the temptable

  SYNOPSIS
    sj_table_is_included()
      join      The join
      join_tab  The table to be checked

  DESCRIPTION
    SemiJoinDuplicateElimination: check the table's rowid should be included
    in the temptable. This is so if

    1. The table is not embedded within some semi-join nest
    2. The has been pulled out of a semi-join nest, or

    3. The table is functionally dependent on some previous table

    [4. This is also true for constant tables that can't be
        NULL-complemented but this function is not called for such tables]

  RETURN
    true  - Include table's rowid
    false - Don't
*/

static bool sj_table_is_included(JOIN *join, JOIN_TAB *join_tab) {
  if (join_tab->emb_sj_nest) return false;

  /* Check if this table is functionally dependent on the tables that
     are within the same outer join nest
  */
  Table_ref *embedding = join_tab->table_ref->embedding;
  if (join_tab->type() == JT_EQ_REF) {
    table_map depends_on = 0;
    uint idx;

    for (uint kp = 0; kp < join_tab->ref().key_parts; kp++)
      depends_on |= join_tab->ref().items[kp]->used_tables();

    Table_map_iterator it(depends_on & ~PSEUDO_TABLE_BITS);
    while ((idx = it.next_bit()) != Table_map_iterator::BITMAP_END) {
      JOIN_TAB *ref_tab = join->map2table[idx];
      if (embedding != ref_tab->table_ref->embedding) return true;
    }
    /* Ok, functionally dependent */
    return false;
  }
  /* Not functionally dependent => need to include*/
  return true;
}

SJ_TMP_TABLE *create_sj_tmp_table(THD *thd, JOIN *join,
                                  SJ_TMP_TABLE_TAB *first_tab,
                                  SJ_TMP_TABLE_TAB *last_tab) {
  uint jt_rowid_offset =
      0;                  // # tuple bytes are already occupied (w/o NULL bytes)
  uint jt_null_bits = 0;  // # null bits in tuple bytes
  for (SJ_TMP_TABLE_TAB *tab = first_tab; tab != last_tab; ++tab) {
    QEP_TAB *qep_tab = tab->qep_tab;
    tab->rowid_offset = jt_rowid_offset;
    jt_rowid_offset += qep_tab->table()->file->ref_length;
    if (qep_tab->table()->is_nullable()) {
      tab->null_byte = jt_null_bits / 8;
      tab->null_bit = jt_null_bits++;
    }
    qep_tab->table()->prepare_for_position();
  }

  SJ_TMP_TABLE *sjtbl;
  if (jt_rowid_offset) /* Temptable has at least one rowid */
  {
    sjtbl = new (thd->mem_root) SJ_TMP_TABLE;
    if (sjtbl == nullptr) return nullptr;
    sjtbl->tabs =
        thd->mem_root->ArrayAlloc<SJ_TMP_TABLE_TAB>(last_tab - first_tab);
    if (sjtbl->tabs == nullptr) return nullptr;
    sjtbl->tabs_end = std::uninitialized_copy(first_tab, last_tab, sjtbl->tabs);
    sjtbl->is_confluent = false;
    sjtbl->rowid_len = jt_rowid_offset;
    sjtbl->null_bits = jt_null_bits;
    sjtbl->null_bytes = (jt_null_bits + 7) / 8;
    sjtbl->tmp_table = create_duplicate_weedout_tmp_table(
        thd, sjtbl->rowid_len + sjtbl->null_bytes, sjtbl);
    if (sjtbl->tmp_table == nullptr) return nullptr;
    if (sjtbl->tmp_table->hash_field)
      sjtbl->tmp_table->file->ha_index_init(0, false);
    join->sj_tmp_tables.push_back(sjtbl->tmp_table);
  } else {
    /*
      This is confluent case where the entire subquery predicate does
      not depend on anything at all, ie this is
        WHERE const IN (uncorrelated select)
    */
    if (!(sjtbl = new (thd->mem_root) SJ_TMP_TABLE))
      return nullptr; /* purecov: inspected */
    sjtbl->tmp_table = nullptr;
    sjtbl->is_confluent = true;
    sjtbl->have_confluent_row = false;
  }
  return sjtbl;
}

/**
  Setup the strategies to eliminate semi-join duplicates.

  @param join           Join to process
  @param no_jbuf_after  Do not use join buffering after the table with this
                        number

  @retval false  OK
  @retval true   Out of memory error

    Setup the strategies to eliminate semi-join duplicates.
    At the moment there are 5 strategies:

    -# DuplicateWeedout (use of temptable to remove duplicates based on rowids
                         of row combinations)
    -# FirstMatch (pick only the 1st matching row combination of inner tables)
    -# LooseScan (scanning the sj-inner table in a way that groups duplicates
                  together and picking the 1st one)
    -# MaterializeLookup (Materialize inner tables, then setup a scan over
                          outer correlated tables, lookup in materialized table)
    -# MaterializeScan (Materialize inner tables, then setup a scan over
                        materialized tables, perform lookup in outer tables)

    The join order has "duplicate-generating ranges", and every range is
    served by one strategy or a combination of FirstMatch with with some
    other strategy.

    "Duplicate-generating range" is defined as a range within the join order
    that contains all of the inner tables of a semi-join. All ranges must be
    disjoint, if tables of several semi-joins are interleaved, then the ranges
    are joined together, which is equivalent to converting

     `SELECT ... WHERE oe1 IN (SELECT ie1 ...) AND oe2 IN (SELECT ie2 )`

    to

      `SELECT ... WHERE (oe1, oe2) IN (SELECT ie1, ie2 ... ...)`.

    Applicability conditions are as follows:

    @par DuplicateWeedout strategy

    @code
      (ot|nt)*  [ it ((it|ot|nt)* (it|ot))]  (nt)*
      +------+  +=========================+  +---+
        (1)                 (2)               (3)
    @endcode

    -# Prefix of OuterTables (those that participate in IN-equality and/or are
       correlated with subquery) and outer Non-correlated tables.

    -# The handled range. The range starts with the first sj-inner table, and
       covers all sj-inner and outer tables Within the range, Inner, Outer,
       outer non-correlated tables may follow in any order.

    -# The suffix of outer non-correlated tables.

    @par FirstMatch strategy

    @code
      (ot|nt)*  [ it (it)* ]  (nt)*
      +------+  +==========+  +---+
        (1)          (2)        (3)

    @endcode
    -# Prefix of outer correlated and non-correlated tables

    -# The handled range, which may contain only inner tables.

    -# The suffix of outer non-correlated tables.

    @par LooseScan strategy

    @code
     (ot|ct|nt) [ loosescan_tbl (ot|nt|it)* it ]  (ot|nt)*
     +--------+   +===========+ +=============+   +------+
        (1)           (2)          (3)              (4)
    @endcode

    -# Prefix that may contain any outer tables. The prefix must contain all
       the non-trivially correlated outer tables. (non-trivially means that
       the correlation is not just through the IN-equality).

    -# Inner table for which the LooseScan scan is performed.  Notice that
       special requirements for existence of certain indexes apply to this
       table, @see class Loose_scan_opt.

    -# The remainder of the duplicate-generating range. It is served by
       application of FirstMatch strategy. Outer IN-correlated tables must be
       correlated to the LooseScan table but not to the inner tables in this
       range. (Currently, there can be no outer tables in this range because
       of implementation restrictions, @see
       Optimize_table_order::advance_sj_state()).

    -# The suffix of outer correlated and non-correlated tables.

    @par MaterializeLookup strategy

    @code
     (ot|nt)*  [ it (it)* ]  (nt)*
     +------+  +==========+  +---+
        (1)         (2)        (3)
    @endcode

    -# Prefix of outer correlated and non-correlated tables.

    -# The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table that is
            later used as a lookup structure for the outer correlated tables.

    -# The suffix of outer non-correlated tables.

    @par MaterializeScan strategy

    @code
     (ot|nt)*  [ it (it)* ]  (ot|nt)*
     +------+  +==========+  +-----+
        (1)         (2)         (3)
    @endcode

    -# Prefix of outer correlated and non-correlated tables.

    -# The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table which is
            later used to setup a scan.

    -# The suffix of outer correlated and non-correlated tables.

  Note that MaterializeLookup and MaterializeScan has overlap in their
  patterns. It may be possible to consolidate the materialization strategies
  into one.

  The choice between the strategies is made by the join optimizer (see
  advance_sj_state() and fix_semijoin_strategies()).  This function sets up
  all fields/structures/etc needed for execution except for
  setup/initialization of semi-join materialization which is done in
  setup_materialized_table().
*/

static bool setup_semijoin_dups_elimination(JOIN *join, uint no_jbuf_after) {
  uint tableno;
  THD *thd = join->thd;
  DBUG_TRACE;
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  if (join->query_block->sj_nests.empty()) return false;

  QEP_TAB *const qep_array = join->qep_tab;
  for (tableno = join->const_tables; tableno < join->primary_tables;) {
#ifndef NDEBUG
    const bool tab_in_sj_nest = join->best_ref[tableno]->emb_sj_nest != nullptr;
#endif
    QEP_TAB *const tab = &qep_array[tableno];
    POSITION *const pos = tab->position();

    if (pos->sj_strategy == SJ_OPT_NONE) {
      tableno++;  // nothing to do
      continue;
    }
    QEP_TAB *last_sj_tab = tab + pos->n_sj_tables - 1;
    switch (pos->sj_strategy) {
      case SJ_OPT_MATERIALIZE_LOOKUP:
      case SJ_OPT_MATERIALIZE_SCAN:
        assert(false);  // Should not occur among "primary" tables
        // Do nothing
        tableno += pos->n_sj_tables;
        break;
      case SJ_OPT_LOOSE_SCAN: {
        assert(tab_in_sj_nest);  // First table must be inner
        /* We jump from the last table to the first one */
        tab->match_tab = last_sj_tab->idx();

        /* For LooseScan, duplicate elimination is based on rows being sorted
           on key. We need to make sure that range select keeps the sorted index
           order. (When using MRR it may not.)

           Note: need_sorted_output() implementations for range select classes
           that do not support sorted output, will trigger an assert. This
           should not happen since LooseScan strategy is only picked if sorted
           output is supported.
        */
        if (tab->range_scan()) {
          assert(used_index(tab->range_scan()) == pos->loosescan_key);
          set_need_sorted_output(tab->range_scan());
        }

        const uint keyno = pos->loosescan_key;
        assert(tab->keys().is_set(keyno));
        tab->set_index(keyno);

        /* Calculate key length */
        uint keylen = 0;
        for (uint kp = 0; kp < pos->loosescan_parts; kp++)
          keylen += tab->table()->key_info[keyno].key_part[kp].store_length;
        tab->loosescan_key_len = keylen;

        if (pos->n_sj_tables > 1) {
          last_sj_tab->firstmatch_return = tab->idx();
          last_sj_tab->match_tab = last_sj_tab->idx();
        }
        tableno += pos->n_sj_tables;
        break;
      }
      case SJ_OPT_DUPS_WEEDOUT: {
        assert(tab_in_sj_nest);  // First table must be inner
        /*
          Consider a semijoin of one outer and one inner table, both
          with two rows. The inner table is assumed to be confluent
          (See sj_opt_materialize_lookup)

          If normal nested loop execution is used, we do not need to
          include semi-join outer table rowids in the duplicate
          weedout temp table since NL guarantees that outer table rows
          are encountered only consecutively and because all rows in
          the temp table are deleted for every new outer table
          combination (example is with a confluent inner table):

            ot1.row1|it1.row1
                 '-> temp table's have_confluent_row == false
                   |-> output ot1.row1
                   '-> set have_confluent_row= true
            ot1.row1|it1.row2
                 |-> temp table's have_confluent_row == true
                 | '-> do not output ot1.row1
                 '-> no more join matches - set have_confluent_row= false
            ot1.row2|it1.row1
                 '-> temp table's have_confluent_row == false
                   |-> output ot1.row2
                   '-> set have_confluent_row= true
              ...

          Note: not having outer table rowids in the temp table and
          then emptying the temp table when a new outer table row
          combinition is encountered is an optimization. Including
          outer table rowids in the temp table is not harmful but
          wastes memory.

          Now consider the join buffering algorithms (BNL/BKA). These
          algorithms join each inner row with outer rows in "reverse"
          order compared to NL. Effectively, this means that outer
          table rows may be encountered multiple times in a
          non-consecutive manner:

            NL:                 BNL/BKA:
            ot1.row1|it1.row1   ot1.row1|it1.row1
            ot1.row1|it1.row2   ot1.row2|it1.row1
            ot1.row2|it1.row1   ot1.row1|it1.row2
            ot1.row2|it1.row2   ot1.row2|it1.row2

          It is clear from the above that there is no place we can
          empty the temp table like we do in NL to avoid storing outer
          table rowids.

          Below we check if join buffering might be used. If so, set
          first_table to the first non-constant table so that outer
          table rowids are included in the temp table. Do not destroy
          other duplicate elimination methods.
        */
        uint first_table = tableno;
        for (uint sj_tableno = tableno; sj_tableno < tableno + pos->n_sj_tables;
             sj_tableno++) {
          if (join->best_ref[sj_tableno]->use_join_cache() &&
              sj_tableno <= no_jbuf_after) {
            /* Join buffering will probably be used */
            first_table = join->const_tables;
            break;
          }
        }

        QEP_TAB *const first_sj_tab = qep_array + first_table;
        if (last_sj_tab->first_inner() != NO_PLAN_IDX &&
            first_sj_tab->first_inner() != last_sj_tab->first_inner()) {
          /*
            The first duplicate weedout table is an outer table of an outer join
            and the last duplicate weedout table is one of the inner tables of
            the outer join.
            In this case, we must assure that all the inner tables of the
            outer join are part of the duplicate weedout operation.
            This is to assure that NULL-extension for inner tables of an
            outer join is performed before duplicate elimination is performed,
            otherwise we will have extra NULL-extended rows being output, which
            should have been eliminated as duplicates.
          */
          QEP_TAB *tab2 = &qep_array[last_sj_tab->first_inner()];
          /*
            First, locate the table that is the first inner table of the
            outer join operation that first_sj_tab is outer for.
          */
          while (tab2->first_upper() != NO_PLAN_IDX &&
                 tab2->first_upper() != first_sj_tab->first_inner())
            tab2 = qep_array + tab2->first_upper();
          // Then, extend the range with all inner tables of the join nest:
          if (qep_array[tab2->first_inner()].last_inner() > last_sj_tab->idx())
            last_sj_tab =
                &qep_array[qep_array[tab2->first_inner()].last_inner()];
        }

        SJ_TMP_TABLE_TAB sjtabs[MAX_TABLES];
        SJ_TMP_TABLE_TAB *last_tab = sjtabs;
        /*
          Walk through the range and remember
           - tables that need their rowids to be put into temptable
           - the last outer table
        */
        for (QEP_TAB *tab_in_range = qep_array + first_table;
             tab_in_range <= last_sj_tab; tab_in_range++) {
          if (sj_table_is_included(join, join->best_ref[tab_in_range->idx()])) {
            last_tab->qep_tab = tab_in_range;
            ++last_tab;
          }
        }

        SJ_TMP_TABLE *sjtbl = create_sj_tmp_table(thd, join, sjtabs, last_tab);
        if (sjtbl == nullptr) {
          return true;
        }

        qep_array[first_table].flush_weedout_table = sjtbl;
        last_sj_tab->check_weed_out_table = sjtbl;

        tableno += pos->n_sj_tables;
        break;
      }
      case SJ_OPT_FIRST_MATCH: {
        /*
          Setup a "jump" from the last table in the range of inner tables
          to the last outer table before the inner tables.
        */
        plan_idx jump_to = tab->idx() - 1;
        assert(tab_in_sj_nest);  // First table must be inner
        for (QEP_TAB *tab_in_range = tab; tab_in_range <= last_sj_tab;
             tab_in_range++) {
          if (!join->best_ref[tab_in_range->idx()]->emb_sj_nest) {
            /*
              Let last non-correlated table be jump target for
              subsequent inner tables.
            */
            assert(false);  // no "split jump" should exist.
            jump_to = tab_in_range->idx();
          } else {
            /*
              Assign jump target for last table in a consecutive range of
              inner tables.
            */
            if (tab_in_range == last_sj_tab ||
                !join->best_ref[tab_in_range->idx() + 1]->emb_sj_nest) {
              tab_in_range->firstmatch_return = jump_to;
              tab_in_range->match_tab = last_sj_tab->idx();
            }
          }
        }
        tableno += pos->n_sj_tables;
        break;
      }
    }
  }
  return false;
}

/*
  Destroy all temporary tables created by NL-semijoin runtime
*/

static void destroy_sj_tmp_tables(JOIN *join) {
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table = it++)) {
    /*
      SJ-Materialization tables are initialized for either sequential reading
      or index lookup, DuplicateWeedout tables are not initialized for read
      (we only write to them), so need to call ha_index_or_rnd_end.
    */
    if (table->file != nullptr) {
      table->file->ha_index_or_rnd_end();
    }
    close_tmp_table(table);
    free_tmp_table(table);
  }
  join->sj_tmp_tables.clear();
}

/**
  Remove all rows from all temp tables used by NL-semijoin runtime

  All rows must be removed from all temporary tables before every join
  re-execution.
*/

bool JOIN::clear_sj_tmp_tables() {
  List_iterator<TABLE> it(sj_tmp_tables);
  TABLE *table;
  while ((table = it++)) {
    if (table->empty_result_table()) return true; /* purecov: inspected */
  }
  return false;
}

/// Empties all correlated materialized derived tables
bool JOIN::clear_corr_derived_tmp_tables() {
  for (uint i = const_tables; i < tables; i++) {
    auto tl = qep_tab[i].table_ref;
    if (tl && tl->is_derived() && !tl->common_table_expr() &&
        (tl->derived_query_expression()->uncacheable & UNCACHEABLE_DEPENDENT) &&
        tl->table) {
      /*
        Applied only to non-CTE derived tables, as CTEs are reset in
        Query_expression::clear_correlated_query_blocks()
      */
      if (tl->derived_query_expression()->query_result()->reset()) return true;
    }
  }
  return false;
}

/**
  Reset the state of this join object so that it is ready for a
  new execution.
*/

void JOIN::reset() {
  DBUG_TRACE;

  if (!executed) return;

  query_expression()->offset_limit_cnt = (ha_rows)(
      query_block->offset_limit ? query_block->offset_limit->val_uint() : 0ULL);

  group_sent = false;
  recursive_iteration_count = 0;
  executed = false;

  List_iterator<Window> li(query_block->m_windows);
  Window *w;
  while ((w = li++)) {
    w->reset_round();
  }

  if (tmp_tables) {
    for (uint tmp = primary_tables; tmp < primary_tables + tmp_tables; tmp++) {
      (void)qep_tab[tmp].table()->empty_result_table();
    }
  }
  clear_sj_tmp_tables();
  set_ref_item_slice(REF_SLICE_SAVED_BASE);

  if (qep_tab) {
    if (query_block->derived_table_count) clear_corr_derived_tmp_tables();
    /* need to reset ref access state (see EQRefIterator) */
    for (uint i = 0; i < tables; i++) {
      QEP_TAB *const tab = &qep_tab[i];
      /*
        If qep_tab==NULL, we may still have done ref access (to read a const
        table); const tables will not be re-read in the next execution of this
        subquery, so resetting key_err is not needed.
      */
      tab->ref().key_err = true;
    }
  }

  /* Reset of sum functions */
  if (sum_funcs) {
    Item_sum *func, **func_ptr = sum_funcs;
    while ((func = *(func_ptr++))) func->clear();
  }

  if (query_block->has_ft_funcs()) {
    /* TODO: move the code to JOIN::exec */
    (void)init_ftfuncs(thd, query_block);
  }
}

/**
  Prepare join result.

  @details Prepare join result prior to join execution or describing.
  Instantiate derived tables and get schema tables result if necessary.

  @return
    true  An error during derived or schema tables instantiation.
    false Ok
*/

bool JOIN::prepare_result() {
  DBUG_TRACE;

  error = 0;

  if (query_block->query_result()->start_execution(thd)) goto err;

  return false;

err:
  error = 1;
  return true;
}

/**
  Clean up and destroy join object.
*/

void JOIN::destroy() {
  cond_equal = nullptr;

  set_plan_state(NO_PLAN);

  if (qep_tab) {
    assert(!join_tab);
    for (uint i = 0; i < tables; i++) {
      TABLE *table = qep_tab[i].table();
      if (table != nullptr) {
        // These were owned by the root iterator, which we just destroyed.
        // Keep filesort_free_buffers() from trying to call CleanupAfterQuery()
        // on them.
        table->sorting_iterator = nullptr;
        table->duplicate_removal_iterator = nullptr;
      }
      qep_tab[i].cleanup();
    }
  } else if (thd->lex->using_hypergraph_optimizer) {
    // Same, for hypergraph queries.
    for (Table_ref *tl = query_block->leaf_tables; tl; tl = tl->next_leaf) {
      TABLE *table = tl->table;
      if (table != nullptr) {
        // For prepared statements, a derived table's temp table handler
        // gets cleaned up at the end of prepare and it is setup again
        // during optimization. However, if optimization for a derived
        // table query block fails for some reason (E.g. Secondary engine
        // rejects all the plans), handler is not setup for the rest of
        // the derived tables. So we need to call set_keyread() only
        // when handler is initialized.
        // TODO(Chaithra): This should be moved to a more suitable place,
        // perhaps TableRowIterator's destructor ?
        if (table->file != nullptr) {
          table->set_keyread(false);
        }
        table->sorting_iterator = nullptr;
        table->duplicate_removal_iterator = nullptr;
      }
    }
    for (JOIN::TemporaryTableToCleanup cleanup : temp_tables) {
      if (cleanup.table != nullptr) {
        cleanup.table->sorting_iterator = nullptr;
        cleanup.table->duplicate_removal_iterator = nullptr;
      }
      close_tmp_table(cleanup.table);
      free_tmp_table(cleanup.table);
      ::destroy(cleanup.temp_table_param);
    }
    for (Filesort *filesort : filesorts_to_cleanup) {
      ::destroy(filesort);
    }
    temp_tables.clear();
    filesorts_to_cleanup.clear();
  }
  if (join_tab || best_ref) {
    for (uint i = 0; i < tables; i++) {
      JOIN_TAB *const tab = join_tab ? &join_tab[i] : best_ref[i];
      tab->cleanup();
    }
  }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */

  // Run Cached_item DTORs!
  group_fields.destroy_elements();
  semijoin_deduplication_fields.destroy_elements();

  tmp_table_param.cleanup();

  /* Cleanup items referencing temporary table columns */
  if (tmp_fields != nullptr) {
    cleanup_item_list(tmp_fields[REF_SLICE_TMP1]);
    cleanup_item_list(tmp_fields[REF_SLICE_TMP2]);
    for (uint widx = 0; widx < m_windows.elements; widx++) {
      cleanup_item_list(tmp_fields[REF_SLICE_WIN_1 + widx]);
    }
  }

  /*
    If current optimization detected any const tables, expressions may have
    been updated without used tables information from those const tables, so
    make sure to restore used tables information as from resolving.
  */
  if (const_tables > 0) query_block->update_used_tables();

  destroy_sj_tmp_tables(this);

  List_iterator<Semijoin_mat_exec> sjm_list_it(sjm_exec_list);
  Semijoin_mat_exec *sjm;
  while ((sjm = sjm_list_it++)) ::destroy(sjm);
  sjm_exec_list.clear();

  keyuse_array.clear();
  // Free memory for rollup arrays
  if (query_block->olap == ROLLUP_TYPE) {
    rollup_group_items.clear();
    rollup_group_items.shrink_to_fit();
    rollup_sums.clear();
    rollup_sums.shrink_to_fit();
  }
}

void JOIN::cleanup_item_list(const mem_root_deque<Item *> &items) const {
  for (Item *item : items) {
    item->cleanup();
  }
}

/**
  Optimize a query block and all inner query expressions

  @param thd    thread handler
  @param finalize_access_paths
                if true, finalize access paths, cf. FinalizePlanForQueryBlock
  @returns false if success, true if error
*/

bool Query_block::optimize(THD *thd, bool finalize_access_paths) {
  DBUG_TRACE;

  assert(join == nullptr);
  JOIN *const join_local = new (thd->mem_root) JOIN(thd, this);
  if (!join_local) return true; /* purecov: inspected */

  /*
    Updating Query_block::join requires acquiring THD::LOCK_query_plan
    to avoid races when EXPLAIN FOR CONNECTION is used.
  */
  thd->lock_query_plan();
  join = join_local;
  thd->unlock_query_plan();

  if (join->optimize(finalize_access_paths)) return true;

  if (join->zero_result_cause && !is_implicitly_grouped()) return false;

  for (Query_expression *query_expression = first_inner_query_expression();
       query_expression;
       query_expression = query_expression->next_query_expression()) {
    // Derived tables and const subqueries are already optimized
    if (!query_expression->is_optimized() &&
        query_expression->optimize(thd, /*materialize_destination=*/nullptr,
                                   /*create_iterators=*/false,
                                   /*finalize_access_paths=*/true))
      return true;
  }

  return false;
}

/**
  Check privileges for all columns referenced from this query block.
  Also check privileges for referenced subqueries.

  @param thd      thread handler

  @returns false if success, true if error (insufficient privileges)

  @todo - skip this if we have table SELECT privileges for all tables
*/
bool Query_block::check_column_privileges(THD *thd) {
  Column_privilege_tracker tracker(thd, SELECT_ACL);

  for (Item *item : visible_fields()) {
    if (item->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                   pointer_cast<uchar *>(thd)))
      return true;
  }
  if (m_current_table_nest &&
      check_privileges_for_join(thd, m_current_table_nest))
    return true;
  if (where_cond() != nullptr &&
      where_cond()->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                         pointer_cast<uchar *>(thd)))
    return true;
  for (ORDER *group = group_list.first; group; group = group->next) {
    if ((*group->item)
            ->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                   pointer_cast<uchar *>(thd)))
      return true;
  }
  if (having_cond() != nullptr &&
      having_cond()->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                          pointer_cast<uchar *>(thd)))
    return true;
  List_iterator<Window> wi(m_windows);
  Window *w;
  while ((w = wi++)) {
    for (ORDER *wp = w->first_partition_by(); wp != nullptr; wp = wp->next)
      if ((*wp->item)->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                            pointer_cast<uchar *>(thd)))
        return true;

    for (ORDER *wo = w->first_order_by(); wo != nullptr; wo = wo->next)
      if ((*wo->item)->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                            pointer_cast<uchar *>(thd)))
        return true;
  }
  for (ORDER *order = order_list.first; order; order = order->next) {
    if ((*order->item)
            ->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                   pointer_cast<uchar *>(thd)))
      return true;
  }

  if (check_privileges_for_subqueries(thd)) return true;

  return false;
}

/**
  Check privileges for column references in a JOIN expression

  @param thd      thread handler
  @param tables   list of joined tables

  @returns false if success, true if error (insufficient privileges)
*/

bool check_privileges_for_join(THD *thd, mem_root_deque<Table_ref *> *tables) {
  thd->want_privilege = SELECT_ACL;

  for (Table_ref *table_ref : *tables) {
    if (table_ref->join_cond() != nullptr &&
        table_ref->join_cond()->walk(&Item::check_column_privileges,
                                     enum_walk::PREFIX,
                                     pointer_cast<uchar *>(thd)))
      return true;

    if (table_ref->nested_join != nullptr &&
        check_privileges_for_join(thd, &table_ref->nested_join->m_tables))
      return true;
  }

  return false;
}

/**
  Check privileges for column references in an item list

  @param thd      thread handler
  @param items    list of items
  @param privileges the required privileges

  @returns false if success, true if error (insufficient privileges)
*/
bool check_privileges_for_list(THD *thd, const mem_root_deque<Item *> &items,
                               ulong privileges) {
  thd->want_privilege = privileges;
  for (Item *item : items) {
    if (item->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                   pointer_cast<uchar *>(thd)))
      return true;
  }
  return false;
}

/**
  Check privileges for column references in subqueries of a query block

  @param thd      thread handler

  @returns false if success, true if error (insufficient privileges)
*/

bool Query_block::check_privileges_for_subqueries(THD *thd) {
  for (Query_expression *query_expression = first_inner_query_expression();
       query_expression;
       query_expression = query_expression->next_query_expression()) {
    for (Query_block *sl = query_expression->first_query_block(); sl;
         sl = sl->next_query_block()) {
      if (sl->check_column_privileges(thd)) return true;
    }
  }
  return false;
}

/*****************************************************************************
  Go through all combinations of not marked tables and find the one
  which uses least records
*****************************************************************************/

/**
  Find how much space the previous read not const tables takes in cache.
*/

void calc_used_field_length(TABLE *table, bool needs_rowid,
                            uint *p_used_fieldlength) {
  uint null_fields, blobs, fields, rec_length;
  Field **f_ptr, *field;
  uint uneven_bit_fields;
  MY_BITMAP *read_set = table->read_set;

  uneven_bit_fields = null_fields = blobs = fields = rec_length = 0;
  for (f_ptr = table->field; (field = *f_ptr); f_ptr++) {
    if (bitmap_is_set(read_set, field->field_index())) {
      fields++;
      rec_length += field->pack_length();
      if (field->is_flag_set(BLOB_FLAG) || field->is_array()) blobs++;
      if (!field->is_flag_set(NOT_NULL_FLAG)) null_fields++;
      if (field->type() == MYSQL_TYPE_BIT && ((Field_bit *)field)->bit_len)
        uneven_bit_fields++;
    }
  }
  if (null_fields || uneven_bit_fields)
    rec_length += (table->s->null_fields + 7) / 8;
  if (table->is_nullable()) rec_length += sizeof(bool);
  if (blobs) {
    uint blob_length = (uint)(table->file->stats.mean_rec_length -
                              (table->s->reclength - rec_length));
    rec_length += max<uint>(4U, blob_length);
  }

  if (needs_rowid) {
    rec_length += table->file->ref_length;
    fields++;
  }

  *p_used_fieldlength = rec_length;
}

bool JOIN::init_ref_access() {
  DBUG_TRACE;
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);

  for (uint tableno = const_tables; tableno < tables; tableno++) {
    JOIN_TAB *const tab = best_ref[tableno];

    if (tab->type() == JT_REF)  // Here JT_REF means all kinds of ref access
    {
      assert(tab->position() && tab->position()->key);
      if (create_ref_for_key(this, tab, tab->position()->key,
                             tab->prefix_tables()))
        return true;
    }
  }

  return false;
}

/**
  Set the first_sj_inner_tab and last_sj_inner_tab fields for all tables
  inside the semijoin nests of the query.
*/
void JOIN::set_semijoin_info() {
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);
  if (query_block->sj_nests.empty()) return;

  for (uint tableno = const_tables; tableno < tables;) {
    JOIN_TAB *const tab = best_ref[tableno];
    const POSITION *const pos = tab->position();

    if (!pos) {
      tableno++;
      continue;
    }
    switch (pos->sj_strategy) {
      case SJ_OPT_NONE:
        tableno++;
        break;
      case SJ_OPT_MATERIALIZE_LOOKUP:
      case SJ_OPT_MATERIALIZE_SCAN:
      case SJ_OPT_LOOSE_SCAN:
      case SJ_OPT_DUPS_WEEDOUT:
      case SJ_OPT_FIRST_MATCH:
        /*
          Remember the first and last semijoin inner tables; this serves to tell
          a JOIN_TAB's semijoin strategy (like in setup_join_buffering()).
        */
        plan_idx last_sj_tab = tableno + pos->n_sj_tables - 1;
        plan_idx last_sj_inner = (pos->sj_strategy == SJ_OPT_DUPS_WEEDOUT)
                                     ?
                                     /* Range may end with non-inner table so
                                        cannot set last_sj_inner_tab */
                                     NO_PLAN_IDX
                                     : last_sj_tab;
        for (plan_idx tab_in_range = tableno; tab_in_range <= last_sj_tab;
             tab_in_range++) {
          best_ref[tab_in_range]->set_first_sj_inner(tableno);
          best_ref[tab_in_range]->set_last_sj_inner(last_sj_inner);
        }
        tableno += pos->n_sj_tables;
        break;
    }
  }
}

void calc_length_and_keyparts(Key_use *keyuse, JOIN_TAB *tab, const uint key,
                              table_map used_tables, Key_use **chosen_keyuses,
                              uint *length_out, uint *keyparts_out,
                              table_map *dep_map, bool *maybe_null) {
  assert(!dep_map || maybe_null);
  uint keyparts = 0, length = 0;
  uint found_part_ref_or_null = 0;
  KEY *const keyinfo = tab->table()->key_info + key;

  do {
    /*
      This Key_use is chosen if:
      - it involves a key part at the right place (if index is (a,b) we
      can have a search criterion on 'b' only if we also have a criterion
      on 'a'),
      - it references only tables earlier in the plan.
      Moreover, the execution layer is limited to maximum one ref_or_null
      keypart, as Index_lookup::null_ref_key is only one byte.
    */
    if (!(~used_tables & keyuse->used_tables) && keyparts == keyuse->keypart &&
        !(found_part_ref_or_null & keyuse->optimize)) {
      assert(keyparts <= MAX_REF_PARTS);
      if (chosen_keyuses) chosen_keyuses[keyparts] = keyuse;
      keyparts++;
      length += keyinfo->key_part[keyuse->keypart].store_length;
      found_part_ref_or_null |= keyuse->optimize;
      if (dep_map) {
        *dep_map |= keyuse->val->used_tables();
        *maybe_null |= keyinfo->key_part[keyuse->keypart].null_bit &&
                       (keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL);
      }
    }
    keyuse++;
  } while (keyuse->table_ref == tab->table_ref && keyuse->key == key);
  if (keyparts <= 0) {
    assert(false);
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Key not found");  // In debug build we assert, but in release we
                                // guard against a potential server exit with an
                                // error
    return;
  }
  *length_out = length;
  *keyparts_out = keyparts;
}

bool init_ref(THD *thd, unsigned keyparts, unsigned length, unsigned keyno,
              Index_lookup *ref) {
  ref->key_parts = keyparts;
  ref->key_length = length;
  ref->key = keyno;
  if (!(ref->key_buff = thd->mem_root->ArrayAlloc<uchar>(ALIGN_SIZE(length))) ||
      !(ref->key_buff2 =
            thd->mem_root->ArrayAlloc<uchar>(ALIGN_SIZE(length))) ||
      !(ref->key_copy = thd->mem_root->ArrayAlloc<store_key *>(keyparts)) ||
      !(ref->items = thd->mem_root->ArrayAlloc<Item *>(keyparts)) ||
      !(ref->cond_guards = thd->mem_root->ArrayAlloc<bool *>(keyparts))) {
    return true;
  }
  ref->key_err = true;
  ref->null_rejecting = 0;
  ref->use_count = 0;
  ref->disable_cache = false;
  return false;
}

bool init_ref_part(THD *thd, unsigned part_no, Item *val, bool *cond_guard,
                   bool null_rejecting, table_map const_tables,
                   table_map used_tables, bool nullable,
                   const KEY_PART_INFO *key_part_info, uchar *key_buff,
                   Index_lookup *ref) {
  ref->items[part_no] = val;  // Save for cond removal
  ref->cond_guards[part_no] = cond_guard;
  // Set ref as "null rejecting" only if either side is really nullable:
  if (null_rejecting && (nullable || val->is_nullable()))
    ref->null_rejecting |= (key_part_map)1 << part_no;

  store_key *s_key = get_store_key(thd, val, used_tables, const_tables,
                                   key_part_info, key_buff, nullable);
  if (unlikely(!s_key || thd->is_error())) return true;

  if (used_tables & ~INNER_TABLE_BIT) {
    /* Comparing against a non-constant. */
    ref->key_copy[part_no] = s_key;
  } else {
    /*
      The outer reference is to a const table, so we copy the value
      straight from that table now (during optimization), instead of from
      the temporary table created during execution.

      TODO: Synchronize with the temporary table creation code, so that
      there is no need to create a column for this value.
    */
    bool dummy_value = false;
    val->walk(&Item::repoint_const_outer_ref, enum_walk::PREFIX,
              pointer_cast<uchar *>(&dummy_value));
    /*
      key is const, copy value now and possibly skip it while ::exec().

      Note:
        Result check of store_key::copy() is unnecessary,
        it could be an error returned by store_key::copy() method
        but stored value is not null and default value could be used
        in this case. Methods which used for storing the value
        should be responsible for proper null value setting
        in case of an error. Thus it's enough to check s_key->null_key
        value only.
    */
    (void)s_key->copy();
    /*
      It should be reevaluated in ::exec() if
      constant evaluated to NULL value which we might need to
      handle as a special case during JOIN::exec()
      (As in : 'Full scan on NULL key')
    */
    if (s_key->null_key)
      ref->key_copy[part_no] = s_key;  // Reevaluate in JOIN::exec()
    else
      ref->key_copy[part_no] = nullptr;
  }
  return false;
}

/**
  Setup a ref access for looking up rows via an index (a key).

  @param join          The join object being handled
  @param j             The join_tab which will have the ref access populated
  @param org_keyuse    First key part of (possibly multi-part) key
  @param used_tables   Bitmap of available tables

  @return False if success, True if error

  Given a Key_use structure that specifies the fields that can be used
  for index access, this function creates and set up the structure
  used for index look up via one of the access methods {JT_FT,
  JT_CONST, JT_REF_OR_NULL, JT_REF, JT_EQ_REF} for the plan operator
  'j'. Generally the function sets up the structure j->ref (of type
  Index_lookup), and the access method j->type.

  @note We cannot setup fields used for ref access before we have sorted
        the items within multiple equalities according to the final order of
        the tables involved in the join operation. Currently, this occurs in
        @see substitute_for_best_equal_field().
        The exception is ref access for const tables, which are fixed
        before the greedy search planner is invoked.
*/

bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
                        table_map used_tables) {
  DBUG_TRACE;

  const uint key = org_keyuse->key;
  const bool ftkey = (org_keyuse->keypart == FT_KEYPART);
  THD *const thd = join->thd;
  uint keyparts, length;
  TABLE *const table = j->table();
  KEY *const keyinfo = table->key_info + key;
  Key_use *chosen_keyuses[MAX_REF_PARTS];

  assert(j->keys().is_set(org_keyuse->key));

  /* Calculate the length of the used key. */
  if (ftkey) {
    Item_func_match *ifm = down_cast<Item_func_match *>(org_keyuse->val);

    length = 0;
    keyparts = 1;
    ifm->get_master()->score_from_index_scan = true;
  } else /* not ftkey */
    calc_length_and_keyparts(org_keyuse, j, key, used_tables, chosen_keyuses,
                             &length, &keyparts, nullptr, nullptr);
  if (thd->is_error()) {
    return true;
  }
  /* set up fieldref */
  if (init_ref(thd, keyparts, length, (int)key, &j->ref())) {
    return true;
  }

  uchar *key_buff = j->ref().key_buff;
  uchar *null_ref_key = nullptr;
  bool keyuse_uses_no_tables = true;
  bool null_rejecting_key = true;
  if (ftkey) {
    Key_use *keyuse = org_keyuse;
    j->ref().items[0] = ((Item_func *)(keyuse->val))->key_item();
    /* Predicates pushed down into subquery can't be used FT access */
    j->ref().cond_guards[0] = nullptr;
    // not supported yet. SerG
    assert(!(keyuse->used_tables & ~PSEUDO_TABLE_BITS));

    j->set_type(JT_FT);
    j->set_ft_func(down_cast<Item_func_match *>(keyuse->val));
    memset(j->ref().key_copy, 0, sizeof(j->ref().key_copy[0]) * keyparts);

    return false;
  }
  // Set up Index_lookup based on chosen Key_use-s.
  for (uint part_no = 0; part_no < keyparts; part_no++) {
    Key_use *keyuse = chosen_keyuses[part_no];
    bool nullable = keyinfo->key_part[part_no].null_bit;

    if (keyuse->val->type() == Item::FIELD_ITEM) {
      // Look up the most appropriate field to base the ref access on.
      keyuse->val = get_best_field(down_cast<Item_field *>(keyuse->val),
                                   join->cond_equal);
      keyuse->used_tables = keyuse->val->used_tables();
    }

    if (init_ref_part(thd, part_no, keyuse->val, keyuse->cond_guard,
                      keyuse->null_rejecting, join->const_table_map,
                      keyuse->used_tables, nullable,
                      &keyinfo->key_part[part_no], key_buff, &j->ref())) {
      return true;
    }

    keyuse_uses_no_tables = keyuse_uses_no_tables && !keyuse->used_tables;

    /*
      Remember if we are going to use REF_OR_NULL
      But only if field _really_ can be null i.e. we force JT_REF
      instead of JT_REF_OR_NULL in case if field can't be null
    */
    if ((keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL) && nullable) {
      assert(null_ref_key == nullptr);  // or we would overwrite it below
      null_ref_key = key_buff;
    }
    /*
      The selected key will reject matches on NULL values if:
       - the key field is nullable, and
       - predicate rejects NULL values (keyuse->null_rejecting is true), or
       - JT_REF_OR_NULL is not effective.
    */
    if ((keyinfo->key_part[part_no].field->is_nullable() ||
         table->is_nullable()) &&
        (!keyuse->null_rejecting || null_ref_key != nullptr)) {
      null_rejecting_key = false;
    }
    key_buff += keyinfo->key_part[part_no].store_length;
  }
  assert(j->type() != JT_FT);
  if (j->type() == JT_CONST)
    j->table()->const_table = true;
  else if (((actual_key_flags(keyinfo) & HA_NOSAME) == 0) ||
           ((actual_key_flags(keyinfo) & HA_NULL_PART_KEY) &&
            !null_rejecting_key) ||
           keyparts != actual_key_parts(keyinfo)) {
    /* Must read with repeat */
    j->set_type(null_ref_key ? JT_REF_OR_NULL : JT_REF);
    j->ref().null_ref_key = null_ref_key;
  } else if (keyuse_uses_no_tables &&
             !(table->file->ha_table_flags() & HA_BLOCK_CONST_TABLE)) {
    /*
      This happen if we are using a constant expression in the ON part
      of an LEFT JOIN.
      SELECT * FROM a LEFT JOIN b ON b.key=30
      Here we should not mark the table as a 'const' as a field may
      have a 'normal' value or a NULL value.
    */
    j->set_type(JT_CONST);
    j->position()->rows_fetched = 1.0;
  } else {
    j->set_type(JT_EQ_REF);
    j->position()->rows_fetched = 1.0;
  }

  return thd->is_error();
}

namespace {

class store_key_const_item final : public store_key {
  int cached_result = -1;

 public:
  store_key_const_item(THD *thd, Field *to_field_arg, uchar *ptr,
                       uchar *null_ptr_arg, uint length, Item *item_arg)
      : store_key(thd, to_field_arg, ptr, null_ptr_arg, length, item_arg) {}
  const char *name() const override { return STORE_KEY_CONST_NAME; }

 protected:
  enum store_key_result copy_inner() override {
    if (cached_result == -1) {
      cached_result = store_key::copy_inner();
    }
    return static_cast<store_key_result>(cached_result);
  }
};

/*
  Class used for indexes over JSON expressions. The value to lookup is
  obtained from val_json() method and then converted according to field's
  result type and saved. This allows proper handling of temporal values.
*/
class store_key_json_item final : public store_key {
  /// Whether the key is constant.
  const bool m_const_key{false};
  /// Whether the key was already copied.
  bool m_inited{false};

 public:
  store_key_json_item(THD *thd, Field *to_field_arg, uchar *ptr,
                      uchar *null_ptr_arg, uint length, Item *item_arg,
                      bool const_key_arg)
      : store_key(thd, to_field_arg, ptr, null_ptr_arg, length, item_arg),
        m_const_key(const_key_arg) {}

  const char *name() const override {
    return m_const_key ? STORE_KEY_CONST_NAME : "func";
  }

 protected:
  enum store_key_result copy_inner() override;
};

}  // namespace

static store_key *get_store_key(THD *thd, Item *val, table_map used_tables,
                                table_map const_tables,
                                const KEY_PART_INFO *key_part, uchar *key_buff,
                                uint maybe_null) {
  if (key_part->field->is_array()) {
    return new (thd->mem_root)
        store_key_json_item(thd, key_part->field, key_buff + maybe_null,
                            maybe_null ? key_buff : nullptr, key_part->length,
                            val, (!((~const_tables) & used_tables)));
  }
  if (!((~const_tables) & used_tables))  // if const item
  {
    return new (thd->mem_root) store_key_const_item(
        thd, key_part->field, key_buff + maybe_null,
        maybe_null ? key_buff : nullptr, key_part->length, val);
  }
  return new (thd->mem_root)
      store_key(thd, key_part->field, key_buff + maybe_null,
                maybe_null ? key_buff : nullptr, key_part->length, val);
}

store_key::store_key(THD *thd, Field *field_arg, uchar *ptr, uchar *null,
                     uint length, Item *item_arg)
    : item(item_arg) {
  if (field_arg->type() == MYSQL_TYPE_BLOB ||
      field_arg->type() == MYSQL_TYPE_GEOMETRY) {
    /*
      Key segments are always packed with a 2 byte length prefix.
      See mi_rkey for details.
    */
    to_field = new (thd->mem_root) Field_varstring(
        ptr, length, 2, null, 1, Field::NONE, field_arg->field_name,
        field_arg->table->s, field_arg->charset());
    to_field->init(field_arg->table);
  } else
    to_field =
        field_arg->new_key_field(thd->mem_root, field_arg->table, ptr, null, 1);

  // If the item is nullable, but we cannot store null, make
  // to_field temporary nullable so that we can check in copy_inner()
  // if we end up with an illegal null value.
  if (!to_field->is_nullable() && item->is_nullable())
    to_field->set_tmp_nullable();
}

store_key::store_key_result store_key::copy() {
  enum store_key_result result;
  THD *thd = current_thd;
  enum_check_fields saved_check_for_truncated_fields =
      thd->check_for_truncated_fields;
  sql_mode_t sql_mode = thd->variables.sql_mode;
  thd->variables.sql_mode &= ~(MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE);

  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;

  result = copy_inner();

  thd->check_for_truncated_fields = saved_check_for_truncated_fields;
  thd->variables.sql_mode = sql_mode;

  return result;
}

enum store_key::store_key_result store_key_hash_item::copy_inner() {
  enum store_key_result res = store_key::copy_inner();
  if (res != STORE_KEY_FATAL) {
    // Convert to and from little endian, since that is what gets
    // stored in the hash field we are lookup up against.
    ulonglong h = uint8korr(pointer_cast<char *>(hash));
    h = unique_hash(to_field, &h);
    int8store(pointer_cast<char *>(hash), h);
  }
  return res;
}

namespace {

enum store_key::store_key_result store_key_json_item::copy_inner() {
  THD *thd = current_thd;
  TABLE *table = to_field->table;
  // Temporarily mark all table's fields writable to avoid assert.
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  if (!m_inited) {
    Json_wrapper wr;
    String str_val, buf;

    Functional_index_error_handler functional_index_error_handler(to_field,
                                                                  thd);
    // Get JSON value and store its value as the key. MEMBER OF is the only
    // function that can use this function
    if (get_json_atom_wrapper(&item, 0, "MEMBER OF", &str_val, &buf, &wr,
                              nullptr, true) ||
        save_json_to_field(thd, to_field, &wr, false))
      return STORE_KEY_FATAL;
    // Copy constant key only once
    if (m_const_key) m_inited = true;
  }

  dbug_tmp_restore_column_map(table->write_set, old_map);
  null_key = to_field->is_null() || item->null_value;
  assert(!thd->is_error());
  return STORE_KEY_OK;
}

}  // namespace

static store_key::store_key_result type_conversion_status_to_store_key(
    THD *thd, type_conversion_status ts) {
  switch (ts) {
    case TYPE_OK:
      return store_key::STORE_KEY_OK;
    case TYPE_NOTE_TRUNCATED:
    case TYPE_WARN_TRUNCATED:
    case TYPE_NOTE_TIME_TRUNCATED:
      if (thd->check_for_truncated_fields)
        return store_key::STORE_KEY_CONV;
      else
        return store_key::STORE_KEY_OK;
    case TYPE_WARN_OUT_OF_RANGE:
    case TYPE_WARN_INVALID_STRING:
    case TYPE_ERR_NULL_CONSTRAINT_VIOLATION:
    case TYPE_ERR_BAD_VALUE:
    case TYPE_ERR_OOM:
      return store_key::STORE_KEY_FATAL;
  }

  assert(false);  // not possible
  return store_key::STORE_KEY_FATAL;
}

enum store_key::store_key_result store_key::copy_inner() {
  THD *thd = current_thd;
  TABLE *table = to_field->table;
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  type_conversion_status save_res = item->save_in_field(to_field, true);
  store_key_result res;
  /*
    Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
    we need to check for errors executing it and react accordingly.
  */
  if (save_res != TYPE_OK && thd->is_error())
    res = STORE_KEY_FATAL;
  else
    res = type_conversion_status_to_store_key(thd, save_res);
  dbug_tmp_restore_column_map(table->write_set, old_map);
  null_key = to_field->is_null() || item->null_value;
  return to_field->is_tmp_null() ? STORE_KEY_FATAL : res;
}

/**
  Extend e1 by AND'ing e2 to the condition e1 points to. The resulting
  condition is fixed. Requirement: the input Items must already have
  been fixed. This is a variant of and_items(); it is intended for use in
  the optimizer phase.

  @param[in,out]   e1 Pointer to condition that will be extended with e2
  @param           e2 Condition that will extend e1

  @retval true   if there was a memory allocation error, in which case
                 e1 remains unchanged
  @retval false  otherwise
*/

bool and_conditions(Item **e1, Item *e2) {
  assert(!(*e1) || (*e1)->fixed);
  assert(!e2 || e2->fixed);
  if (*e1) {
    if (!e2) return false;
    Item *res = new Item_cond_and(*e1, e2);
    if (unlikely(!res)) return true;

    *e1 = res;
    res->quick_fix_field();
    res->update_used_tables();

  } else
    *e1 = e2;
  return false;
}

/*
  Get a part of the condition that can be checked using only index fields

  SYNOPSIS
    make_cond_for_index()
      cond           The source condition
      table          The table that is partially available
      keyno          The index in the above table. Only fields covered by the
  index are available other_tbls_ok  true <=> Fields of other non-const tables
  are allowed

  DESCRIPTION
    Get a part of the condition that can be checked when for the given table
    we have values only of fields covered by some index. The condition may
    refer to other tables, it is assumed that we have values of all of their
    fields.

    Example:
      make_cond_for_index(
         "cond(t1.field) AND cond(t2.key1) AND cond(t2.non_key) AND
  cond(t2.key2)", t2, keyno(t2.key1)) will return "cond(t1.field) AND
  cond(t2.key2)"

  RETURN
    Index condition, or NULL if no condition could be inferred.
*/

static Item *make_cond_for_index(Item *cond, TABLE *table, uint keyno,
                                 bool other_tbls_ok) {
  assert(cond != nullptr);

  if (cond->type() == Item::COND_ITEM) {
    uint n_marked = 0;
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      table_map used_tables = 0;
      Item_cond_and *new_cond = new Item_cond_and;
      if (!new_cond) return nullptr;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_for_index(item, table, keyno, other_tbls_ok);
        if (fix) {
          new_cond->argument_list()->push_back(fix);
          used_tables |= fix->used_tables();
        }
        n_marked += (item->marker == Item::MARKER_ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked == ((Item_cond *)cond)->argument_list()->elements)
        cond->marker = Item::MARKER_ICP_COND_USES_INDEX_ONLY;
      switch (new_cond->argument_list()->elements) {
        case 0:
          return nullptr;
        case 1:
          new_cond->set_used_tables(used_tables);
          return new_cond->argument_list()->head();
        default:
          new_cond->quick_fix_field();
          new_cond->set_used_tables(used_tables);
          return new_cond;
      }
    } else /* It's OR */
    {
      Item_cond_or *new_cond = new Item_cond_or;
      if (!new_cond) return nullptr;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_for_index(item, table, keyno, other_tbls_ok);
        if (!fix) return nullptr;
        new_cond->argument_list()->push_back(fix);
        n_marked += (item->marker == Item::MARKER_ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked == ((Item_cond *)cond)->argument_list()->elements)
        cond->marker = Item::MARKER_ICP_COND_USES_INDEX_ONLY;
      new_cond->quick_fix_field();
      new_cond->set_used_tables(cond->used_tables());
      new_cond->apply_is_true();
      return new_cond;
    }
  }

  if (!uses_index_fields_only(cond, table, keyno, other_tbls_ok)) {
    /*
      Reset marker since it might have the value
      MARKER_ICP_COND_USES_INDEX_ONLY if this condition is part of the select
      condition for multiple tables.
    */
    cond->marker = Item::MARKER_NONE;
    return nullptr;
  }
  cond->marker = Item::MARKER_ICP_COND_USES_INDEX_ONLY;
  return cond;
}

static Item *make_cond_remainder(Item *cond, bool exclude_index) {
  if (exclude_index && cond->marker == Item::MARKER_ICP_COND_USES_INDEX_ONLY)
    return nullptr; /* Already checked */

  if (cond->type() == Item::COND_ITEM) {
    table_map tbl_map = 0;
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      /* Create new top level AND item */
      Item_cond_and *new_cond = new Item_cond_and;
      if (!new_cond) return (Item *)nullptr;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_remainder(item, exclude_index);
        if (fix) {
          new_cond->argument_list()->push_back(fix);
          tbl_map |= fix->used_tables();
        }
      }
      switch (new_cond->argument_list()->elements) {
        case 0:
          return (Item *)nullptr;
        case 1:
          return new_cond->argument_list()->head();
        default:
          new_cond->quick_fix_field();
          new_cond->set_used_tables(tbl_map);
          return new_cond;
      }
    } else /* It's OR */
    {
      Item_cond_or *new_cond = new Item_cond_or;
      if (!new_cond) return (Item *)nullptr;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_remainder(item, false);
        if (!fix) return (Item *)nullptr;
        new_cond->argument_list()->push_back(fix);
        tbl_map |= fix->used_tables();
      }
      new_cond->quick_fix_field();
      new_cond->set_used_tables(tbl_map);
      new_cond->apply_is_true();
      return new_cond;
    }
  }
  return cond;
}

/**
  Try to extract and push the index condition down to table handler

  @param  join_tab       join_tab for table
  @param  keyno          Index for which extract and push the condition
  @param  trace_obj      trace object where information is to be added
*/
void QEP_TAB::push_index_cond(const JOIN_TAB *join_tab, uint keyno,
                              Opt_trace_object *trace_obj) {
  JOIN *const join_ = join();
  DBUG_TRACE;

  ASSERT_BEST_REF_IN_JOIN_ORDER(join_);
  assert(join_tab == join_->best_ref[idx()]);

  if (join_tab->reversed_access)  // @todo: historical limitation, lift it!
    return;

  TABLE *const tbl = table();

  // Disable ICP for Innodb intrinsic temp table because of performance
  if (tbl->s->db_type() == innodb_hton && tbl->s->tmp_table != NO_TMP_TABLE &&
      tbl->s->tmp_table != TRANSACTIONAL_TMP_TABLE)
    return;

  // TODO: Currently, index on virtual generated column doesn't support ICP
  if (tbl->vfield && tbl->index_contains_some_virtual_gcol(keyno)) return;

  /*
    Fields of other non-const tables aren't allowed in following cases:
       type is:
        (JT_ALL | JT_INDEX_SCAN | JT_RANGE | JT_INDEX_MERGE)
       and BNL is used.
    and allowed otherwise.
  */
  const bool other_tbls_ok =
      !((type() == JT_ALL || type() == JT_INDEX_SCAN || type() == JT_RANGE ||
         type() == JT_INDEX_MERGE) &&
        join_tab->use_join_cache() == JOIN_CACHE::ALG_BNL);

  /*
    We will only attempt to push down an index condition when the
    following criteria are true:
    0. The table has a select condition
    1. The storage engine supports ICP.
    2. The index_condition_pushdown switch is on and
       the use of ICP is not disabled by the NO_ICP hint.
    3. The query is not a multi-table update or delete statement. The reason
       for this requirement is that the same handler will be used
       both for doing the select/join and the update. The pushed index
       condition might then also be applied by the storage engine
       when doing the update part and result in either not finding
       the record to update or updating the wrong record.
    4. The JOIN_TAB is not part of a subquery that has guarded conditions
       that can be turned on or off during execution of a 'Full scan on NULL
       key'.
       @see Item_in_optimizer::val_int()
       @see subselect_iterator_engine::exec()
       @see Index_lookup::cond_guards
       @see setup_join_buffering
    5. The join type is not CONST or SYSTEM. The reason for excluding
       these join types, is that these are optimized to only read the
       record once from the storage engine and later re-use it. In a
       join where a pushed index condition evaluates fields from
       tables earlier in the join sequence, the pushed condition would
       only be evaluated the first time the record value was needed.
    6. The index is not a clustered index. The performance improvement
       of pushing an index condition on a clustered key is much lower
       than on a non-clustered key. This restriction should be
       re-evaluated when WL#6061 is implemented.
    7. The index on virtual generated columns is not supported for ICP.
  */
  if (condition() &&
      tbl->file->index_flags(keyno, 0, true) & HA_DO_INDEX_COND_PUSHDOWN &&
      hint_key_state(join_->thd, table_ref, keyno, ICP_HINT_ENUM,
                     OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN) &&
      join_->thd->lex->sql_command != SQLCOM_UPDATE_MULTI &&
      join_->thd->lex->sql_command != SQLCOM_DELETE_MULTI &&
      !has_guarded_conds() && type() != JT_CONST && type() != JT_SYSTEM &&
      !(keyno == tbl->s->primary_key &&
        tbl->file->primary_key_is_clustered())) {
    DBUG_EXECUTE("where", print_where(join_->thd, condition(), "full cond",
                                      QT_ORDINARY););
    Item *idx_cond =
        make_cond_for_index(condition(), tbl, keyno, other_tbls_ok);
    DBUG_EXECUTE("where",
                 print_where(join_->thd, idx_cond, "idx cond", QT_ORDINARY););
    if (idx_cond) {
      /*
        Check that the condition to push actually contains fields from
        the index. Without any fields from the index it is unlikely
        that it will filter out any records since the conditions on
        fields from other tables in most cases have already been
        evaluated.
      */
      idx_cond->update_used_tables();
      if ((idx_cond->used_tables() & table_ref->map()) == 0) {
        /*
          The following assert is to check that we only skip pushing the
          index condition for the following situations:
          1. We actually are allowed to generate an index condition on another
             table.
          2. The index condition is a constant item.
          3. The index condition contains an updatable user variable
             (test this by checking that the RAND_TABLE_BIT is set).
        */
        assert(other_tbls_ok ||                    // 1
               idx_cond->const_item() ||           // 2
               idx_cond->is_non_deterministic());  // 3
        return;
      }

      Item *idx_remainder_cond = nullptr;

      /*
        For BKA cache, we don't store the condition, because evaluation of the
        condition would require additional operations before the evaluation.
      */
      if (join_tab->use_join_cache() &&
          /*
            if cache is used then the value is true only
            for BKA cache (see setup_join_buffering() func).
            In this case other_tbls_ok is an equivalent of
            cache->is_key_access().
          */
          other_tbls_ok &&
          (idx_cond->used_tables() &
           ~(table_ref->map() | join_->const_table_map))) {
        idx_remainder_cond = idx_cond;
        trace_obj->add("not_pushed_due_to_BKA", true);
      } else {
        idx_remainder_cond = tbl->file->idx_cond_push(keyno, idx_cond);
        DBUG_EXECUTE("where",
                     print_where(join_->thd, tbl->file->pushed_idx_cond,
                                 "icp cond", QT_ORDINARY););
      }
      /*
        Disable eq_ref's "lookup cache" if we've pushed down an index
        condition.
        TODO: This check happens to work on current ICP implementations, but
        there may exist a compliant implementation that will not work
        correctly with it. Sort this out when we stabilize the condition
        pushdown APIs.
      */
      if (idx_remainder_cond != idx_cond) {
        ref().disable_cache = true;
        trace_obj->add("pushed_index_condition", idx_cond);
      }

      Item *row_cond = make_cond_remainder(condition(), true);
      DBUG_EXECUTE("where", print_where(join_->thd, row_cond, "remainder cond",
                                        QT_ORDINARY););

      if (row_cond) {
        and_conditions(&row_cond, idx_remainder_cond);
        idx_remainder_cond = row_cond;
      }
      set_condition(idx_remainder_cond);
      trace_obj->add("table_condition_attached", idx_remainder_cond);
    }
  }
}

/**
  Setup the materialized table for a semi-join nest

  @param tab       join_tab for the materialized semi-join table
  @param tableno   table number of materialized table
  @param inner_pos information about the first inner table of the subquery
  @param sjm_pos   information about the materialized semi-join table,
                   to be filled with data.

  @details
    Setup execution structures for one semi-join materialization nest:
    - Create the materialization temporary table, including Table_ref
  object.
    - Create a list of Item_field objects per column in the temporary table.
    - Create a keyuse array describing index lookups into the table
      (for MaterializeLookup)

  @return False if OK, True if error
*/

bool JOIN::setup_semijoin_materialized_table(JOIN_TAB *tab, uint tableno,
                                             POSITION *inner_pos,
                                             POSITION *sjm_pos) {
  DBUG_TRACE;
  Table_ref *const emb_sj_nest = inner_pos->table->emb_sj_nest;
  Semijoin_mat_optimize *const sjm_opt = &emb_sj_nest->nested_join->sjm;
  Semijoin_mat_exec *const sjm_exec = tab->sj_mat_exec();
  const uint field_count = emb_sj_nest->nested_join->sj_inner_exprs.size();

  assert(field_count > 0);
  assert(inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP ||
         inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN);

  /*
    Set up the table to write to, do as
    Query_result_union::create_result_table does
  */
  sjm_exec->table_param = Temp_table_param();
  count_field_types(query_block, &sjm_exec->table_param,
                    emb_sj_nest->nested_join->sj_inner_exprs, false, true);
  sjm_exec->table_param.bit_fields_as_long = true;

  char buffer[NAME_LEN];
  const size_t len = snprintf(buffer, sizeof(buffer) - 1, "<subquery%u>",
                              emb_sj_nest->nested_join->query_block_id);
  char *name = (char *)thd->mem_root->Alloc(len + 1);
  if (name == nullptr) return true; /* purecov: inspected */

  memcpy(name, buffer, len);
  name[len] = '\0';
  TABLE *table;
  if (!(table =
            create_tmp_table(thd, &sjm_exec->table_param,
                             emb_sj_nest->nested_join->sj_inner_exprs, nullptr,
                             true /* distinct */, true /* save_sum_fields */,
                             thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS,
                             HA_POS_ERROR /* rows_limit */, name)))
    return true; /* purecov: inspected */
  sjm_exec->table = table;
  map2table[tableno] = tab;
  table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
  sj_tmp_tables.push_back(table);
  sjm_exec_list.push_back(sjm_exec);

  /*
    Hash_field is not applicable for MATERIALIZE_LOOKUP. If hash_field is
    created for temporary table, semijoin_types_allow_materialization must
    assure that MATERIALIZE_LOOKUP can't be chosen.
  */
  assert((inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP &&
          !table->hash_field) ||
         inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN);

  auto tl = new (thd->mem_root) Table_ref("", name, TL_IGNORE);
  if (tl == nullptr) return true; /* purecov: inspected */
  tl->table = table;

  /*
    If the SJ nest is inside an outer join nest, this tmp table belongs to
    it. It's important for attachment of the semi-join ON condition with the
    proper guards, to this table. If it's an AJ nest it's an outer join
    nest too.
  */
  if (emb_sj_nest->is_aj_nest())
    tl->embedding = emb_sj_nest;
  else
    tl->embedding = emb_sj_nest->outer_join_nest();
  /*
    Above, we do not set tl->emb_sj_nest, neither first_sj_inner nor
    last_sj_inner; it's because there's no use to say that this table is part
    of the SJ nest; but it's necessary to say that it's part of any outer join
    nest. The antijoin nest is an outer join nest, but from the POV of the
    sj-tmp table it's only an outer join nest, so there is no need to set
    emb_sj_nest even in this case.
  */

  // Table is "nullable" if inner table of an outer_join
  if (tl->is_inner_table_of_outer_join()) table->set_nullable();

  tl->set_tableno(tableno);

  table->pos_in_table_list = tl;
  table->pos_in_table_list->query_block = query_block;

  if (!(sjm_opt->mat_fields = (Item_field **)thd->mem_root->Alloc(
            field_count * sizeof(Item_field **))))
    return true;

  for (uint fieldno = 0; fieldno < field_count; fieldno++) {
    if (!(sjm_opt->mat_fields[fieldno] =
              new Item_field(table->visible_field_ptr()[fieldno])))
      return true;
  }

  tab->table_ref = tl;
  tab->set_table(table);
  tab->set_position(sjm_pos);

  tab->worst_seeks = 1.0;
  tab->set_records((ha_rows)emb_sj_nest->nested_join->sjm.expected_rowcount);

  tab->found_records = tab->records();
  tab->read_time = emb_sj_nest->nested_join->sjm.scan_cost.total_cost();

  tab->init_join_cond_ref(tl);

  table->keys_in_use_for_query.set_all();
  sjm_pos->table = tab;
  sjm_pos->sj_strategy = SJ_OPT_NONE;

  sjm_pos->use_join_buffer = false;
  /*
    No need to recalculate filter_effect since there are no post-read
    conditions for materialized tables.
  */
  sjm_pos->filter_effect = 1.0;

  /*
    Key_use objects are required so that create_ref_for_key() can set up
    a proper ref access for this table.
  */
  Key_use_array *keyuse =
      create_keyuse_for_table(thd, field_count, sjm_opt->mat_fields,
                              emb_sj_nest->nested_join->sj_outer_exprs);
  if (!keyuse) return true;

  double fanout = ((uint)tab->idx() == const_tables)
                      ? 1.0
                      : best_ref[tab->idx() - 1]->position()->prefix_rowcount;
  if (!sjm_exec->is_scan) {
    sjm_pos->key = keyuse->begin();  // MaterializeLookup will use the index
    sjm_pos->read_cost =
        emb_sj_nest->nested_join->sjm.lookup_cost.total_cost() * fanout;
    tab->set_keyuse(keyuse->begin());
    tab->keys().set_bit(0);  // There is one index - use it always
    tab->set_index(0);
    sjm_pos->rows_fetched = 1.0;
    tab->set_type(JT_REF);
  } else {
    sjm_pos->key = nullptr;  // No index use for MaterializeScan
    sjm_pos->read_cost = tab->read_time * fanout;
    sjm_pos->rows_fetched = static_cast<double>(tab->records());
    tab->set_type(JT_ALL);
  }
  sjm_pos->set_prefix_join_cost((tab - join_tab), cost_model());

  return false;
}

/**
  A helper function that sets the right op type for join cache (BNL/BKA).
*/

void QEP_TAB::init_join_cache(JOIN_TAB *join_tab) {
  assert(idx() > 0);
  ASSERT_BEST_REF_IN_JOIN_ORDER(join());
  assert(join_tab == join()->best_ref[idx()]);

  switch (join_tab->use_join_cache()) {
    case JOIN_CACHE::ALG_BNL:
      op_type = QEP_TAB::OT_BNL;
      break;
    case JOIN_CACHE::ALG_BKA:
      op_type = QEP_TAB::OT_BKA;
      break;
    default:
      assert(0);
  }
}

/**
  Plan refinement stage: do various setup things for the executor

  @param join          Join being processed
  @param no_jbuf_after Don't use join buffering after table with this number.

  @return false if successful, true if error (Out of memory)

  @details
    Plan refinement stage: do various set ups for the executioner
      - setup join buffering use
      - push index conditions
      - increment relevant counters
      - etc
*/

bool make_join_readinfo(JOIN *join, uint no_jbuf_after) {
  const bool statistics = !join->thd->lex->is_explain();
  const bool prep_for_pos = join->need_tmp_before_win ||
                            join->select_distinct ||
                            !join->group_list.empty() || !join->order.empty() ||
                            join->m_windows.elements > 0;

  DBUG_TRACE;
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  Opt_trace_context *const trace = &join->thd->opt_trace;
  Opt_trace_object wrapper(trace);
  Opt_trace_array trace_refine_plan(trace, "refine_plan");

  if (setup_semijoin_dups_elimination(join, no_jbuf_after))
    return true; /* purecov: inspected */

  for (uint i = join->const_tables; i < join->tables; i++) {
    QEP_TAB *const qep_tab = &join->qep_tab[i];
    if (!qep_tab->position()) continue;

    JOIN_TAB *const tab = join->best_ref[i];
    TABLE *const table = qep_tab->table();
    Table_ref *const table_ref = qep_tab->table_ref;
    /*
     Need to tell handlers that to play it safe, it should fetch all
     columns of the primary key of the tables: this is because MySQL may
     build row pointers for the rows, and for all columns of the primary key
     the read set has not necessarily been set by the server code.
    */
    if (prep_for_pos) table->prepare_for_position();

    Opt_trace_object trace_refine_table(trace);
    trace_refine_table.add_utf8_table(table_ref);

    if (tab->use_join_cache() != JOIN_CACHE::ALG_NONE)
      qep_tab->init_join_cache(tab);

    switch (qep_tab->type()) {
      case JT_EQ_REF:
      case JT_REF_OR_NULL:
      case JT_REF:
      case JT_SYSTEM:
      case JT_CONST:
        if (table->covering_keys.is_set(qep_tab->ref().key) &&
            !table->no_keyread)
          table->set_keyread(true);
        else
          qep_tab->push_index_cond(tab, qep_tab->ref().key,
                                   &trace_refine_table);
        break;
      case JT_ALL:
        join->thd->set_status_no_index_used();
        qep_tab->using_dynamic_range = (tab->use_quick == QS_DYNAMIC_RANGE);
        [[fallthrough]];
      case JT_INDEX_SCAN:
        if (tab->position()->filter_effect != COND_FILTER_STALE_NO_CONST &&
            !tab->sj_mat_exec()) {
          /*
            rows_w_const_cond is # of rows which will be read by the access
            method, minus those which will not pass the constant condition;
            that's how calculate_scan_cost() works. Such number is useful inside
            the planner, but obscure to the reader of EXPLAIN; so we put the
            real count of read rows into rows_fetched, and move the constant
            condition's filter to filter_effect.
          */
          double rows_w_const_cond = qep_tab->position()->rows_fetched;
          table_ref->fetch_number_of_rows();
          tab->position()->rows_fetched =
              static_cast<double>(table->file->stats.records);
          if (tab->position()->filter_effect != COND_FILTER_STALE) {
            // Constant condition moves to filter_effect:
            if (tab->position()->rows_fetched == 0)  // avoid division by zero
              tab->position()->filter_effect = 0.0f;
            else
              tab->position()->filter_effect *= static_cast<float>(
                  rows_w_const_cond / tab->position()->rows_fetched);
          }
        }
        if (qep_tab->using_dynamic_range) {
          join->thd->set_status_no_good_index_used();
          if (statistics) join->thd->inc_status_select_range_check();
        } else {
          if (statistics) {
            if (i == join->const_tables)
              join->thd->inc_status_select_scan();
            else
              join->thd->inc_status_select_full_join();
          }
        }
        break;
      case JT_RANGE:
      case JT_INDEX_MERGE:
        qep_tab->using_dynamic_range = (tab->use_quick == QS_DYNAMIC_RANGE);
        if (statistics) {
          if (i == join->const_tables)
            join->thd->inc_status_select_range();
          else
            join->thd->inc_status_select_full_range_join();
        }
        if (!table->no_keyread && qep_tab->type() == JT_RANGE) {
          if (table->covering_keys.is_set(used_index(qep_tab->range_scan()))) {
            assert(used_index(qep_tab->range_scan()) != MAX_KEY);
            table->set_keyread(true);
          }
          if (!table->key_read)
            qep_tab->push_index_cond(tab, used_index(qep_tab->range_scan()),
                                     &trace_refine_table);
        }
        if (tab->position()->filter_effect != COND_FILTER_STALE_NO_CONST) {
          double rows_w_const_cond = qep_tab->position()->rows_fetched;
          qep_tab->position()->rows_fetched =
              tab->range_scan()->num_output_rows();
          if (tab->position()->filter_effect != COND_FILTER_STALE) {
            // Constant condition moves to filter_effect:
            if (tab->position()->rows_fetched == 0)  // avoid division by zero
              tab->position()->filter_effect = 0.0f;
            else
              tab->position()->filter_effect *= static_cast<float>(
                  rows_w_const_cond / tab->position()->rows_fetched);
          }
        }
        break;
      case JT_FT:
        if (tab->join()->fts_index_access(tab)) {
          table->set_keyread(true);
          table->covering_keys.set_bit(tab->ft_func()->key);
        }
        break;
      default:
        DBUG_PRINT("error", ("Table type %d found",
                             qep_tab->type())); /* purecov: deadcode */
        assert(0);
        break; /* purecov: deadcode */
    }

    // Now that we have decided which index to use and whether to use "dynamic
    // range scan", it is time to filter away base columns for virtual generated
    // columns from the read_set. This is so that if we are scanning on a
    // covering index, code that uses the table's read set (join buffering, hash
    // join, filesort; they all use it to figure out which records to pack into
    // their buffers) do not try to pack the non-existent base columns. See
    // filter_virtual_gcol_base_cols().
    filter_virtual_gcol_base_cols(qep_tab);

    if (tab->position()->filter_effect <= COND_FILTER_STALE) {
      /*
        Give a proper value for EXPLAIN.
        For performance reasons, we do not recalculate the filter for
        non-EXPLAIN queries; thus, EXPLAIN CONNECTION may show 100%
        for a query.

        Also calculate the proper value if max_join_size is in effect and there
        is a limit, since it's needed in order to calculate how many rows to
        read from the base table if rows are filtered before the limit is
        applied.
      */
      tab->position()->filter_effect =
          (join->thd->lex->is_explain() ||
           (join->m_select_limit != HA_POS_ERROR &&
            !Overlaps(join->thd->variables.option_bits, OPTION_BIG_SELECTS)))
              ? calculate_condition_filter(
                    tab,
                    (tab->ref().key != -1) ? tab->position()->key : nullptr,
                    tab->prefix_tables() & ~table_ref->map(),
                    tab->position()->rows_fetched, false, false,
                    trace_refine_table)
              : COND_FILTER_ALLPASS;
    }

    assert(!table_ref->is_recursive_reference() || qep_tab->type() == JT_ALL);

    qep_tab->set_reversed_access(tab->reversed_access);

    // Materialize derived tables prior to accessing them.
    if (table_ref->is_table_function()) {
      qep_tab->materialize_table = QEP_TAB::MATERIALIZE_TABLE_FUNCTION;
      if (tab->dependent) qep_tab->rematerialize = true;
    } else if (table_ref->uses_materialization()) {
      qep_tab->materialize_table = QEP_TAB::MATERIALIZE_DERIVED;
    }

    if (qep_tab->sj_mat_exec())
      qep_tab->materialize_table = QEP_TAB::MATERIALIZE_SEMIJOIN;

    if (table_ref->is_derived() &&
        table_ref->derived_query_expression()->m_lateral_deps) {
      auto deps = table_ref->derived_query_expression()->m_lateral_deps;
      plan_idx last = NO_PLAN_IDX;
      for (JOIN_TAB **tab2 = join->map2table; deps; tab2++, deps >>= 1) {
        if (deps & 1) last = std::max(last, (*tab2)->idx());
      }
      /*
        We identified the last dependency of table_ref in the plan, and it's
        the table whose reading must trigger rematerialization of table_ref.
      */
      if (last != NO_PLAN_IDX) {
        QEP_TAB &t = join->qep_tab[last];
        t.lateral_derived_tables_depend_on_me |= TableBitmap(i);
        trace_refine_table.add_utf8("rematerialized_for_each_row_of",
                                    t.table()->alias);
      }
    }
  }

  return false;
}

void JOIN_TAB::set_table(TABLE *t) {
  if (t != nullptr) t->reginfo.join_tab = this;
  m_qs->set_table(t);
}

void JOIN_TAB::init_join_cond_ref(Table_ref *tl) {
  m_join_cond_ref = tl->join_cond_optim_ref();
}

/**
  Cleanup table of join operation.
*/

void JOIN_TAB::cleanup() {
  // Delete parts specific of JOIN_TAB:

  if (table()) table()->reginfo.join_tab = nullptr;

  // Delete shared parts:
  if (join()->qep_tab) {
    // deletion will be done by QEP_TAB
  } else
    qs_cleanup();
}

void QEP_TAB::cleanup() {
  // Delete parts specific of QEP_TAB:
  destroy(filesort);
  filesort = nullptr;

  TABLE *const t = table();

  if (t != nullptr) {
    t->reginfo.qep_tab = nullptr;
    t->const_table = false;  // Note: Also done in TABLE::init()
  }

  // Delete shared parts:
  qs_cleanup();

  // Order of qs_cleanup() and this, matters:
  if (op_type == QEP_TAB::OT_MATERIALIZE ||
      op_type == QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE ||
      op_type == QEP_TAB::OT_AGGREGATE_INTO_TMP_TABLE ||
      op_type == QEP_TAB::OT_WINDOWING_FUNCTION) {
    if (t != nullptr)  // Check tmp table is not yet freed.
    {
      close_tmp_table(t);
      free_tmp_table(t);
    }
    destroy(tmp_table_param);
    tmp_table_param = nullptr;
  }
  if (table_ref != nullptr && table_ref->uses_materialization()) {
    assert(t == table_ref->table);
    t->merge_keys.clear_all();
    t->quick_keys.clear_all();
    t->covering_keys.clear_all();
    t->possible_quick_keys.clear_all();

    close_tmp_table(t);
  }
}

void QEP_shared_owner::qs_cleanup() {
  /* Skip non-existing derived tables/views result tables */
  if (table() &&
      (table()->s->tmp_table != INTERNAL_TMP_TABLE || table()->is_created())) {
    table()->set_keyread(false);
    table()->file->ha_index_or_rnd_end();
    free_io_cache(table());
    filesort_free_buffers(table(), true);
    Table_ref *const table_ref = table()->pos_in_table_list;
    if (table_ref) {
      table_ref->derived_keys_ready = false;
      table_ref->derived_key_list.clear();
    }
  }
  destroy(range_scan());
}

uint QEP_TAB::sjm_query_block_id() const {
  assert(sj_is_materialize_strategy(get_sj_strategy()));
  for (uint i = 0; i < join()->primary_tables; ++i) {
    // Find the sj-mat tmp table whose sj nest contains us:
    Semijoin_mat_exec *const sjm = join()->qep_tab[i].sj_mat_exec();
    if (sjm && (uint)idx() >= sjm->inner_table_index &&
        (uint)idx() < sjm->inner_table_index + sjm->table_count)
      return sjm->sj_nest->nested_join->query_block_id;
  }
  assert(false);
  return 0;
}

/**
  Extend join_tab->cond by AND'ing add_cond to it

  @param add_cond    The condition to AND with the existing cond
                     for this JOIN_TAB

  @retval true   if there was a memory allocation error
  @retval false  otherwise
*/
bool QEP_shared_owner::and_with_condition(Item *add_cond) {
  Item *tmp = condition();
  if (and_conditions(&tmp, add_cond)) return true;
  set_condition(tmp);
  return false;
}

/**
  Partially cleanup JOIN after it has executed: close index or rnd read
  (table cursors), free quick selects.

    This function is called in the end of execution of a JOIN, before the used
    tables are unlocked and closed.

    For a join that is resolved using a temporary table, the first sweep is
    performed against actual tables and an intermediate result is inserted
    into the temporary table.
    The last sweep is performed against the temporary table. Therefore,
    the base tables and associated buffers used to fill the temporary table
    are no longer needed, and this function is called to free them.

    For a join that is performed without a temporary table, this function
    is called after all rows are sent, but before EOF packet is sent.

    For a simple SELECT with no subqueries this function performs a full
    cleanup of the JOIN and calls mysql_unlock_read_tables to free used base
    tables.

    If a JOIN is executed for a subquery or if it has a subquery, we can't
    do the full cleanup and need to do a partial cleanup only.
    - If a JOIN is not the top level join, we must not unlock the tables
    because the outer select may not have been evaluated yet, and we
    can't unlock only selected tables of a query.
    - Additionally, if this JOIN corresponds to a correlated subquery, we
    should not free quick selects and join buffers because they will be
    needed for the next execution of the correlated subquery.
    - However, if this is a JOIN for a [sub]select, which is not
    a correlated subquery itself, but has subqueries, we can free it
    fully and also free JOINs of all its subqueries. The exception
    is a subquery in SELECT list, e.g:
    @code
    SELECT a, (select max(b) from t1) group by c
    @endcode
    This subquery will not be evaluated at first sweep and its value will
    not be inserted into the temporary table. Instead, it's evaluated
    when selecting from the temporary table. Therefore, it can't be freed
    here even though it's not correlated.

  @todo
    Unlock tables even if the join isn't top level select in the tree
*/

void JOIN::join_free() {
  Query_expression *tmp_query_expression;
  Query_block *sl;
  /*
    Optimization: if not EXPLAIN and we are done with the JOIN,
    free all tables.
  */
  bool full = (!query_block->uncacheable && !thd->lex->is_explain());
  bool can_unlock = full;
  DBUG_TRACE;

  cleanup();

  for (tmp_query_expression = query_block->first_inner_query_expression();
       tmp_query_expression;
       tmp_query_expression = tmp_query_expression->next_query_expression())
    for (sl = tmp_query_expression->first_query_block(); sl;
         sl = sl->next_query_block()) {
      Item_subselect *subselect = sl->master_query_expression()->item;
      bool full_local = full && (!subselect || subselect->is_evaluated());
      /*
        If this join is evaluated, we can partially clean it up and clean up
        all its underlying joins even if they are correlated, only query plan
        is left in case a user will run EXPLAIN FOR CONNECTION.
        If this join is not yet evaluated, we still must clean it up to
        close its table cursors -- it may never get evaluated, as in case of
        ... HAVING FALSE OR a IN (SELECT ...))
        but all table cursors must be closed before the unlock.
      */
      sl->cleanup_all_joins();
      /* Can't unlock if at least one JOIN is still needed */
      can_unlock = can_unlock && full_local;
    }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (can_unlock && lock && thd->lock && !thd->locked_tables_mode &&
      !(query_block->active_options() & SELECT_NO_UNLOCK) &&
      !query_block->subquery_in_having &&
      (query_block == thd->lex->unit->query_term()->query_block())) {
    /*
      TODO: unlock tables even if the join isn't top level select in the
      tree.
    */
    mysql_unlock_read_tables(thd, lock);  // Don't free join->lock
    DEBUG_SYNC(thd, "after_join_free_unlock");
    lock = nullptr;
  }
}

static void cleanup_table(TABLE *table) {
  if (table->is_created()) {
    table->file->ha_index_or_rnd_end();
  }
  free_io_cache(table);
  filesort_free_buffers(table, false);
}

/**
  Free resources of given join.

  @note
    With subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/

void JOIN::cleanup() {
  DBUG_TRACE;

  assert(const_tables <= primary_tables && primary_tables <= tables);

  if (qep_tab || join_tab || best_ref) {
    for (uint i = 0; i < tables; i++) {
      QEP_TAB *qtab;
      TABLE *table;
      if (qep_tab) {
        assert(!join_tab);
        qtab = &qep_tab[i];
        table = qtab->table();
      } else {
        qtab = nullptr;
        table = (join_tab ? &join_tab[i] : best_ref[i])->table();
      }
      if (!table) continue;
      cleanup_table(table);
    }
  } else if (thd->lex->using_hypergraph_optimizer) {
    for (Table_ref *tl = query_block->leaf_tables; tl; tl = tl->next_leaf) {
      cleanup_table(tl->table);
    }
    for (JOIN::TemporaryTableToCleanup cleanup : temp_tables) {
      cleanup_table(cleanup.table);
    }
  }

  if (rollup_state != RollupState::NONE) {
    for (size_t i = 0; i < fields->size(); i++) {
      Item *inner = unwrap_rollup_group(query_block->base_ref_items[i]);
      if (inner->type() == Item::SUM_FUNC_ITEM) {
        Item_sum *sum = down_cast<Item_sum *>(inner);
        if (sum->is_rollup_sum_wrapper()) {
          // unwrap sum switcher to restore original Item_sum
          inner = down_cast<Item_rollup_sum_switcher *>(sum)->unwrap_sum();
        }
      }
      query_block->base_ref_items[i] = inner;
    }
  }
  /* Restore ref array to original state */
  set_ref_item_slice(REF_SLICE_SAVED_BASE);
}

/**
  Filter out ORDER BY items that are equal to constants in WHERE condition

  This function is a limited version of remove_const() for use
  with non-JOIN statements (i.e. single-table UPDATE and DELETE).

  @param order            Linked list of ORDER BY arguments.
  @param where            Where condition.

  @return pointer to new filtered ORDER list or NULL if whole list eliminated

  @note
    This function overwrites input order list.
*/

ORDER *simple_remove_const(ORDER *order, Item *where) {
  if (order == nullptr || where == nullptr) return order;

  ORDER *first = nullptr, *prev = nullptr;
  for (; order; order = order->next) {
    assert(!order->item[0]->has_aggregation());  // should never happen
    if (!check_field_is_const(where, order->item[0])) {
      if (first == nullptr) first = order;
      if (prev != nullptr) prev->next = order;
      prev = order;
    }
  }
  if (prev != nullptr) prev->next = nullptr;
  return first;
}

bool equality_determines_uniqueness(const Item_func_comparison *func,
                                    const Item *v, const Item *c) {
  /*
    - The "c" argument must be a constant.
    - The result type of both arguments must be the same.
      However, since a temporal type is also classified as a string type,
      we do not allow a temporal constant to be considered equal to a
      variable character string.
    - If both arguments are strings, the comparison operator must have the same
      collation as the ordering operation applied to the variable expression.
  */
  return c->const_for_execution() && v->result_type() == c->result_type() &&
         (v->result_type() != STRING_RESULT ||
          (!(is_string_type(v->data_type()) &&
             is_temporal_type(c->data_type())) &&
           func->compare_collation() == v->collation.collation));
}

bool equality_has_no_implicit_casts(const Item_func_comparison *func,
                                    const Item *item1, const Item *item2) {
  // See equality_determines_uniqueness() for the logic around strings
  // and dates.
  if (item1->result_type() != item2->result_type()) {
    return false;
  }
  if (item1->result_type() == STRING_RESULT) {
    if (is_temporal_type(item1->data_type()) !=
        is_temporal_type(item2->data_type())) {
      return false;
    }
    if (is_string_type(item1->data_type()) !=
        is_string_type(item2->data_type())) {
      return false;
    }
    if (is_string_type(item1->data_type())) {
      if (func->compare_collation() != item1->collation.collation ||
          func->compare_collation() != item2->collation.collation) {
        return false;
      }
    }
  }
  return true;
}

/*
  Return true if i1 and i2 (if any) are equal items,
  or if i1 is a wrapper item around the f2 field.
*/

static bool equal(const Item *i1, const Item *i2, const Field *f2) {
  assert((i2 == nullptr) ^ (f2 == nullptr));

  if (i2 != nullptr)
    return i1->eq(i2, true);
  else if (i1->type() == Item::FIELD_ITEM)
    return f2->eq(down_cast<const Item_field *>(i1)->field);
  else
    return false;
}

/**
  Check if a field is equal to a constant value in a condition

  @param      cond        condition to search within
  @param      order_item  Item to find in condition (if order_field is NULL)
  @param      order_field Field to find in condition (if order_item is NULL)
  @param[out] const_item  Used in calculation with conjunctive predicates,
                          must be NULL in outer-most call.

  @returns true if the field is a constant value in condition, false otherwise
*/
bool check_field_is_const(Item *cond, const Item *order_item,
                          const Field *order_field, Item **const_item) {
  assert((order_item == nullptr) ^ (order_field == nullptr));

  Item *intermediate = nullptr;
  if (const_item == nullptr) const_item = &intermediate;

  if (cond->type() == Item::COND_ITEM) {
    Item_cond *const c = down_cast<Item_cond *>(cond);
    bool and_level = c->functype() == Item_func::COND_AND_FUNC;
    List_iterator_fast<Item> li(*c->argument_list());
    Item *item;
    while ((item = li++)) {
      if (check_field_is_const(item, order_item, order_field, const_item)) {
        if (and_level) return true;
      } else if (!and_level)
        return false;
    }
    return !and_level;
  }
  if (cond->type() != Item::FUNC_ITEM) return false;
  Item_func *const func = down_cast<Item_func *>(cond);
  if (func->functype() != Item_func::EQUAL_FUNC &&
      func->functype() != Item_func::EQ_FUNC)
    return false;
  Item_func_comparison *comp = down_cast<Item_func_comparison *>(func);
  Item *left = comp->arguments()[0];
  Item *right = comp->arguments()[1];
  if (equal(left, order_item, order_field)) {
    if (equality_determines_uniqueness(comp, left, right)) {
      if (*const_item != nullptr) return right->eq(*const_item, true);
      *const_item = right;
      return true;
    }
  } else if (equal(right, order_item, order_field)) {
    if (equality_determines_uniqueness(comp, right, left)) {
      if (*const_item != nullptr) return left->eq(*const_item, true);
      *const_item = left;
      return true;
    }
  }
  return false;
}

/**
  Update TMP_TABLE_PARAM with count of the different type of fields.

  This function counts the number of fields, functions and sum
  functions (items with type SUM_FUNC_ITEM) for use by
  create_tmp_table() and stores it in the Temp_table_param object. It
  also updates the allow_group_via_temp_table property if needed.

  @param query_block           Query_block of query
  @param param                Description of temp table
  @param fields               List of fields to count
  @param reset_with_sum_func  Whether to reset with_sum_func of func items
  @param save_sum_fields      Count in the way create_tmp_table() expects when
                              given the same parameter.
*/

void count_field_types(const Query_block *query_block, Temp_table_param *param,
                       const mem_root_deque<Item *> &fields,
                       bool reset_with_sum_func, bool save_sum_fields) {
  DBUG_TRACE;

  param->sum_func_count = 0;
  param->func_count = fields.size();
  param->hidden_field_count = 0;
  param->outer_sum_func_count = 0;
  /*
    Loose index scan guarantees that all grouping is done and MIN/MAX
    functions are computed, so create_tmp_table() treats this as if
    save_sum_fields is set.
  */
  save_sum_fields |= param->precomputed_group_by;

  for (Item *field : fields) {
    Item *real = field->real_item();
    Item::Type real_type = real->type();

    if (real_type == Item::SUM_FUNC_ITEM && !real->m_is_window_function) {
      if (!field->const_item()) {
        Item_sum *sum_item = down_cast<Item_sum *>(field->real_item());
        if (sum_item->aggr_query_block == query_block) {
          if (!sum_item->allow_group_via_temp_table)
            param->allow_group_via_temp_table = false;  // UDF SUM function
          param->sum_func_count++;

          // Add one column per argument.
          param->func_count += sum_item->argument_count();
        }
      } else if (save_sum_fields) {
        /*
          Count the way create_tmp_table() does if asked to preserve
          Item_sum_* functions in fields list.

          Item field is an Item_sum_* or a reference to such an
          item. We need to distinguish between these two cases since
          they are treated differently by create_tmp_table().
        */
        if (field->type() != Item::SUM_FUNC_ITEM) {
          // A reference to an Item_sum_*
          param->func_count++;  // TODO: Is this really needed?
          param->sum_func_count++;
        }
      }
    } else if (real_type == Item::SUM_FUNC_ITEM) {
      assert(real->m_is_window_function);

      Item_sum *window_item = down_cast<Item_sum *>(real);
      param->func_count += window_item->argument_count();
    } else {
      if (reset_with_sum_func) field->reset_aggregation();
      if (field->has_aggregation()) param->outer_sum_func_count++;
    }
  }
}

/**
  Return 1 if second is a subpart of first argument.

  If first parts has different direction, change it to second part
  (group is sorted like order)
*/

bool test_if_subpart(ORDER *a, ORDER *b) {
  ORDER *first = a;
  ORDER *second = b;
  for (; first && second; first = first->next, second = second->next) {
    if ((*first->item)->eq(*second->item, true))
      continue;
    else
      return false;
  }
  // If the second argument is not subpart of the first return false
  if (second) return false;
  // Else assign the direction of the second argument to the first
  for (; a && b; a = a->next, b = b->next) a->direction = b->direction;
  return true;
}

/**
  calc how big buffer we need for comparing group entries.
*/

void calc_group_buffer(JOIN *join, ORDER *group) {
  DBUG_TRACE;
  uint key_length = 0, parts = 0, null_parts = 0;

  if (group) join->grouped = true;
  for (; group; group = group->next) {
    Item *group_item = *group->item;
    Field *field = group_item->get_tmp_table_field();
    if (field) {
      enum_field_types type;
      if ((type = field->type()) == MYSQL_TYPE_BLOB)
        key_length += MAX_BLOB_WIDTH;  // Can't be used as a key
      else if (type == MYSQL_TYPE_VARCHAR || type == MYSQL_TYPE_VAR_STRING)
        key_length += field->field_length + HA_KEY_BLOB_LENGTH;
      else if (type == MYSQL_TYPE_BIT) {
        /* Bit is usually stored as a longlong key for group fields */
        key_length += 8;  // Big enough
      } else
        key_length += field->pack_length();
    } else {
      switch (group_item->result_type()) {
        case REAL_RESULT:
          key_length += sizeof(double);
          break;
        case INT_RESULT:
          key_length += sizeof(longlong);
          break;
        case DECIMAL_RESULT:
          key_length += my_decimal_get_binary_size(
              group_item->max_length - (group_item->decimals ? 1 : 0),
              group_item->decimals);
          break;
        case STRING_RESULT: {
          /*
            As items represented as DATE/TIME fields in the group buffer
            have STRING_RESULT result type, we increase the length
            by 8 as maximum pack length of such fields.
          */
          if (group_item->is_temporal()) {
            key_length += 8;
          } else if (group_item->data_type() == MYSQL_TYPE_BLOB)
            key_length += MAX_BLOB_WIDTH;  // Can't be used as a key
          else {
            /*
              Group strings are taken as varstrings and require an length field.
              A field is not yet created by create_tmp_field()
              and the sizes should match up.
            */
            key_length += group_item->max_length + HA_KEY_BLOB_LENGTH;
          }
          break;
        }
        default:
          /* This case should never be chosen */
          assert(0);
          my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      }
    }
    parts++;
    if (group_item->is_nullable()) null_parts++;
  }
  join->tmp_table_param.group_length = key_length + null_parts;
  join->tmp_table_param.group_parts = parts;
  join->tmp_table_param.group_null_parts = null_parts;
}

/**
  Make an array of pointers to sum_functions to speed up
  sum_func calculation.

  @retval
    0	ok
  @retval
    1	Error
*/

bool JOIN::alloc_func_list() {
  uint func_count, group_parts;
  DBUG_TRACE;

  func_count = tmp_table_param.sum_func_count;
  /*
    If we are using rollup, we need a copy of the summary functions for
    each level
  */
  if (rollup_state != RollupState::NONE) func_count *= (send_group_parts + 1);

  group_parts = send_group_parts;
  /*
    If distinct, reserve memory for possible
    disctinct->group_by optimization
  */
  if (select_distinct) {
    group_parts += CountVisibleFields(*fields);
    /*
      If the ORDER clause is specified then it's possible that
      it also will be optimized, so reserve space for it too
    */
    if (!order.empty()) {
      ORDER *ord;
      for (ord = order.order; ord; ord = ord->next) group_parts++;
    }
  }

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs =
      (Item_sum **)thd->mem_calloc(sizeof(Item_sum **) * (func_count + 1) +
                                   sizeof(Item_sum ***) * (group_parts + 1));
  return sum_funcs == nullptr;
}

/**
  Initialize 'sum_funcs' array with all Item_sum objects.

  @param fields            All items
  @param before_group_by   Set to 1 if this is called before GROUP BY handling
  @param recompute         Set to true if sum_funcs must be recomputed

  @retval
    0  ok
  @retval
    1  error
*/

bool JOIN::make_sum_func_list(const mem_root_deque<Item *> &fields,
                              bool before_group_by, bool recompute) {
  DBUG_TRACE;

  if (*sum_funcs && !recompute)
    return false; /* We have already initialized sum_funcs. */

  Item_sum **func = sum_funcs;
  for (Item *item : fields) {
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
        down_cast<Item_sum *>(item)->aggr_query_block == query_block) {
      assert(!item->m_is_window_function);
      *func++ = down_cast<Item_sum *>(item);
    }
  }
  if (before_group_by && rollup_state == RollupState::INITED) {
    rollup_state = RollupState::READY;
  } else if (rollup_state == RollupState::READY)
    return false;   // Don't put end marker
  *func = nullptr;  // End marker
  return false;
}

/**
  Free joins of subselect of this select.

  @param select   pointer to Query_block which subselects joins we will free

  @todo when the final use of this function (from SET statements) is removed,
  this function can be deleted.
*/

void free_underlaid_joins(Query_block *select) {
  for (Query_expression *query_expression =
           select->first_inner_query_expression();
       query_expression;
       query_expression = query_expression->next_query_expression())
    query_expression->cleanup(false);
}

/**
  Change the Query_result object of the query block.

  If old_result is not used, forward the call to the current
  Query_result in case it is a wrapper around old_result.

  Call prepare() on the new Query_result if we decide to use it.

  @param thd        Thread handle
  @param new_result New Query_result object
  @param old_result Old Query_result object (NULL to force change)

  @retval false Success
  @retval true  Error
*/

bool Query_block::change_query_result(THD *thd,
                                      Query_result_interceptor *new_result,
                                      Query_result_interceptor *old_result) {
  DBUG_TRACE;
  if (old_result == nullptr || query_result() == old_result) {
    set_query_result(new_result);
    if (query_result()->prepare(thd, fields, master_query_expression()))
      return true; /* purecov: inspected */
    return false;
  } else {
    const bool ret = query_result()->change_query_result(thd, new_result);
    return ret;
  }
}

/**
  Add having condition as a filter condition, which is applied when reading
  from the temp table.

  @param    curr_tmp_table  Table number to which having conds are added.
  @returns  false if success, true if error.
*/

bool JOIN::add_having_as_tmp_table_cond(uint curr_tmp_table) {
  having_cond->update_used_tables();
  QEP_TAB *const curr_table = &qep_tab[curr_tmp_table];
  table_map used_tables;
  Opt_trace_context *const trace = &thd->opt_trace;

  DBUG_TRACE;

  if (curr_table->table_ref)
    used_tables = curr_table->table_ref->map();
  else {
    /*
      Pushing parts of HAVING to an internal temporary table.
      Fields in HAVING condition may have been replaced with fields in an
      internal temporary table. This table has map=1.
    */
    assert(having_cond->has_subquery() ||
           !(having_cond->used_tables() & ~(1 | PSEUDO_TABLE_BITS)));
    used_tables = 1;
  }
  // Condition may contain outer references, const and non-deterministic exprs:
  used_tables |= PSEUDO_TABLE_BITS;

  /*
    All conditions which can be applied after reading from used_tables are
    added as filter conditions of curr_tmp_table. If condition's used_tables is
    not read yet for example subquery in having, then it will be kept as it is
    in original having_cond of join.
    If ROLLUP, having condition needs to be tested after writing rollup data.
    So do not move the having condition.
  */
  Item *sort_table_cond =
      (rollup_state == RollupState::NONE)
          ? make_cond_for_table(thd, having_cond, used_tables, table_map{0},
                                false)
          : nullptr;
  if (sort_table_cond) {
    if (!curr_table->condition())
      curr_table->set_condition(sort_table_cond);
    else {
      curr_table->set_condition(
          new Item_cond_and(curr_table->condition(), sort_table_cond));
      if (curr_table->condition()->fix_fields(thd, nullptr)) return true;
    }
    curr_table->condition()->apply_is_true();
    DBUG_EXECUTE("where", print_where(thd, curr_table->condition(),
                                      "select and having", QT_ORDINARY););

    having_cond = make_cond_for_table(thd, having_cond, ~table_map{0},
                                      ~used_tables, false);
    DBUG_EXECUTE("where", print_where(thd, having_cond, "having after sort",
                                      QT_ORDINARY););

    Opt_trace_object trace_wrapper(trace);
    Opt_trace_object(trace, "sort_using_internal_table")
        .add("condition_for_sort", sort_table_cond)
        .add("having_after_sort", having_cond);
  }

  return false;
}

bool CreateFramebufferTable(
    THD *thd, const Temp_table_param &tmp_table_param,
    const Query_block &query_block, const mem_root_deque<Item *> &source_fields,
    const mem_root_deque<Item *> &window_output_fields,
    Func_ptr_array *mapping_from_source_to_window_output, Window *window) {
  /*
    Create the window frame buffer tmp table.  We create a
    temporary table with same contents as the output tmp table
    in the windowing pipeline (columns defined by
    curr_all_fields), but used for intermediate storage, saving
    the window's frame buffer now that we know the window needs
    buffering.
  */
  Temp_table_param *par =
      new (thd->mem_root) Temp_table_param(thd->mem_root, tmp_table_param);
  par->m_window_frame_buffer = true;

  // Don't include temporary fields that originally came from
  // a window function (or an expression containing a window function).
  // Window functions are not relevant to store in the framebuffer,
  // and in fact, trying to restore them would often overwrite
  // good data we shouldn't.
  //
  // Not that the regular filtering in create_tmp_table() cannot do this
  // for us, as it only sees the Item_field, not where it came from.
  mem_root_deque<Item *> fb_fields(window_output_fields);
  for (size_t i = 0; i < fb_fields.size(); ++i) {
    Item *orig_item = source_fields[i];
    if (orig_item->has_wf()) {
      fb_fields[i] = nullptr;
    }
  }
  fb_fields.erase(std::remove(fb_fields.begin(), fb_fields.end(), nullptr),
                  fb_fields.end());
  count_field_types(&query_block, par, fb_fields, false, false);

  TABLE *table =
      create_tmp_table(thd, par, fb_fields, nullptr, false, false,
                       query_block.active_options(), HA_POS_ERROR, "");
  if (table == nullptr) return true;

  window->set_frame_buffer_param(par);
  window->set_frame_buffer(table);

  // For window function expressions we are to evaluate after
  // framebuffering, we need to replace their arguments to point to the
  // output table instead of the input table (we could probably also have
  // used the framebuffer if we wanted). E.g., if our input is t1 and our
  // output is <temporary>, we need to rewrite 1 + SUM(t1.x) OVER w into
  // 1 + SUM(<temporary>.x) OVER w.
  for (Func_ptr &ptr : *mapping_from_source_to_window_output) {
    if (ptr.func()->has_wf()) {
      ReplaceMaterializedItems(thd, ptr.func(),
                               *mapping_from_source_to_window_output,
                               /*need_exact_match=*/false);
    }
  }
  return false;
}

/**
  Init tmp tables usage info.

  @details
  This function finalizes execution plan by taking following actions:
    .) tmp tables are created, but not instantiated (this is done during
       execution). QEP_TABs dedicated to tmp tables are filled appropriately.
       see JOIN::create_intermediate_table.
    .) prepare fields lists (fields, all_fields, ref_item_array slices) for
       each required stage of execution. These fields lists are set for
       tmp tables' tabs and for the tab of last table in the join.
    .) fill info for sorting/grouping/dups removal is prepared and saved to
       appropriate tabs. Here is an example:
        SELECT * from t1,t2 WHERE ... GROUP BY t1.f1 ORDER BY t2.f2, t1.f2
        and lets assume that the table order in the plan is t1,t2.
       In this case optimizer will sort for group only the first table as the
       second one isn't mentioned in GROUP BY. The result will be materialized
       in tmp table.  As filesort can't sort join optimizer will sort tmp table
       also. The first sorting (for group) is called simple as is doesn't
       require tmp table.  The Filesort object for it is created here - in
       JOIN::create_intermediate_table.  Filesort for the second case is
       created here, in JOIN::make_tmp_tables_info.

  @note
  This function may change tmp_table_param.precomputed_group_by. This
  affects how create_tmp_table() treats aggregation functions, so
  count_field_types() must be called again to make sure this is taken
  into consideration.

  @returns
  false - Ok
  true  - Error
*/

bool JOIN::make_tmp_tables_info() {
  assert(!join_tab);
  mem_root_deque<Item *> *curr_fields = fields;
  bool materialize_join = false;
  uint curr_tmp_table = const_tables;
  TABLE *exec_tmp_table = nullptr;

  auto cleanup_tmp_tables_on_error =
      create_scope_guard([this, &curr_tmp_table] {
        if (qep_tab == nullptr) {
          return;
        }
        for (unsigned table_idx = primary_tables; table_idx <= curr_tmp_table;
             ++table_idx) {
          TABLE *table = qep_tab[table_idx].table();
          if (table != nullptr) {
            close_tmp_table(table);
            free_tmp_table(table);
            qep_tab[table_idx].set_table(nullptr);
          }
        }
      });

  /*
    If the plan is constant, we will not do window tmp table processing
    cf. special code path for handling const plans.
  */
  m_windowing_steps = m_windows.elements > 0 && !plan_is_const() &&
                      !implicit_grouping && !group_optimized_away;
  const bool may_trace =  // just to avoid an empty trace block
      need_tmp_before_win || implicit_grouping || m_windowing_steps ||
      !group_list.empty() || !order.empty();

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_disable_I_S trace_disabled(trace, !may_trace);
  Opt_trace_object wrapper(trace);
  Opt_trace_array trace_tmp(trace, "considering_tmp_tables");

  DBUG_TRACE;

  /*
    In this function, we may change having_cond into a condition on a
    temporary sort/group table, so we have to assign having_for_explain now:
  */
  having_for_explain = having_cond;

  const bool has_group_by = this->grouped;

  /*
    The loose index scan access method guarantees that all grouping or
    duplicate row elimination (for distinct) is already performed
    during data retrieval, and that all MIN/MAX functions are already
    computed for each group. Thus all MIN/MAX functions should be
    treated as regular functions, and there is no need to perform
    grouping in the main execution loop.
    Notice that currently loose index scan is applicable only for
    single table queries, thus it is sufficient to test only the first
    join_tab element of the plan for its access method.
  */
  if (qep_tab && qep_tab[0].range_scan() &&
      is_loose_index_scan(qep_tab[0].range_scan()))
    tmp_table_param.precomputed_group_by =
        !is_agg_loose_index_scan(qep_tab[0].range_scan());

  /*
    Create the first temporary table if distinct elimination is requested or
    if the sort is too complicated to be evaluated as a filesort.
  */
  if (need_tmp_before_win) {
    curr_tmp_table = primary_tables;
    Opt_trace_object trace_this_outer(trace);
    trace_this_outer.add("adding_tmp_table_in_plan_at_position",
                         curr_tmp_table);
    tmp_tables++;

    /*
      Make a copy of the base slice in the save slice.
      This is needed because later steps will overwrite the base slice with
      another slice (1-3).
      After this slice has been used, overwrite the base slice again with
      the copy in the save slice.
    */
    if (alloc_ref_item_slice(thd, REF_SLICE_SAVED_BASE)) return true;

    copy_ref_item_slice(REF_SLICE_SAVED_BASE, REF_SLICE_ACTIVE);
    current_ref_item_slice = REF_SLICE_SAVED_BASE;

    /*
      Create temporary table for use in a single execution.
      (Will be reused if this is a subquery that is executed several times
       for one execution of the statement)
      Don't use tmp table grouping for json aggregate funcs as it's
      very ineffective.
    */
    ORDER_with_src tmp_group;
    if (!simple_group && !(test_flags & TEST_NO_KEY_GROUP) && !with_json_agg)
      tmp_group = group_list;

    tmp_table_param.hidden_field_count = CountHiddenFields(*fields);

    if (create_intermediate_table(&qep_tab[curr_tmp_table], *fields, tmp_group,
                                  !group_list.empty() && simple_group))
      return true;
    exec_tmp_table = qep_tab[curr_tmp_table].table();

    if (exec_tmp_table->s->is_distinct) optimize_distinct();

    /*
      If there is no sorting or grouping, 'use_order'
      index result should not have been requested.
      Exception: LooseScan strategy for semijoin requires
      sorted access even if final result is not to be sorted.
    */
    assert(
        !(m_ordered_index_usage == ORDERED_INDEX_VOID && !plan_is_const() &&
          qep_tab[const_tables].position()->sj_strategy != SJ_OPT_LOOSE_SCAN &&
          qep_tab[const_tables].use_order()));

    /*
      Allocate a slice of ref items that describe the items to be copied
      from the first temporary table.
    */
    if (alloc_ref_item_slice(thd, REF_SLICE_TMP1)) return true;

    // Change sum_fields reference to calculated fields in tmp_table
    if (streaming_aggregation || qep_tab[curr_tmp_table].table()->group ||
        tmp_table_param.precomputed_group_by) {
      if (change_to_use_tmp_fields(fields, thd, ref_items[REF_SLICE_TMP1],
                                   &tmp_fields[REF_SLICE_TMP1],
                                   query_block->m_added_non_hidden_fields))
        return true;
    } else {
      if (change_to_use_tmp_fields_except_sums(
              fields, thd, query_block, ref_items[REF_SLICE_TMP1],
              &tmp_fields[REF_SLICE_TMP1],
              query_block->m_added_non_hidden_fields))
        return true;
    }
    curr_fields = &tmp_fields[REF_SLICE_TMP1];
    // Need to set them now for correct group_fields setup, reset at the end.
    set_ref_item_slice(REF_SLICE_TMP1);
    qep_tab[curr_tmp_table].ref_item_slice = REF_SLICE_TMP1;
    setup_tmptable_write_func(&qep_tab[curr_tmp_table], &trace_this_outer);

    /*
      If having is not handled here, it will be checked before the row is sent
      to the client.
    */
    if (having_cond &&
        (streaming_aggregation ||
         (exec_tmp_table->s->is_distinct && group_list.empty()))) {
      /*
        If there is no select distinct or rollup, then move the having to table
        conds of tmp table.
        NOTE : We cannot apply having after distinct. If columns of having are
               not part of select distinct, then distinct may remove rows
               which can satisfy having.

        As this condition will read the tmp table, it is appropriate that
        REF_SLICE_TMP1 is in effect when we create it below.
      */
      if ((!select_distinct && rollup_state == RollupState::NONE) &&
          add_having_as_tmp_table_cond(curr_tmp_table))
        return true;

      /*
        Having condition which we are not able to add as tmp table conds are
        kept as before. And, this will be applied before storing the rows in
        tmp table.
      */
      qep_tab[curr_tmp_table].having = having_cond;
      having_cond = nullptr;  // Already done
    }

    tmp_table_param.func_count = 0;

    if (streaming_aggregation || qep_tab[curr_tmp_table].table()->group) {
      tmp_table_param.func_count += tmp_table_param.sum_func_count;
      tmp_table_param.sum_func_count = 0;
    }

    if (exec_tmp_table->group) {  // Already grouped
                                  /*
                                    Check if group by has to respect ordering. If true, move group by to
                                    order by.
                                  */
      if (order.empty() && !skip_sort_order) {
        for (ORDER *group = group_list.order; group; group = group->next) {
          if (group->direction != ORDER_NOT_RELEVANT) {
            order = group_list; /* order by group */
            break;
          }
        }
      }
      group_list.clean();
    }
    /*
      If we have different sort & group then we must sort the data by group
      and copy it to a second temporary table.
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)) or when distinct is used with rollup.
    */

    if ((!group_list.empty() &&
         (!test_if_subpart(group_list.order, order.order) || select_distinct ||
          m_windowing_steps || rollup_state != RollupState::NONE)) ||
        (select_distinct && (tmp_table_param.using_outer_summary_function ||
                             rollup_state != RollupState::NONE))) {
      DBUG_PRINT("info", ("Creating group table"));

      calc_group_buffer(this, group_list.order);
      count_field_types(query_block, &tmp_table_param,
                        tmp_fields[REF_SLICE_TMP1],
                        select_distinct && group_list.empty(), false);
      tmp_table_param.hidden_field_count =
          CountHiddenFields(tmp_fields[REF_SLICE_TMP1]);
      streaming_aggregation = false;
      if (!exec_tmp_table->group && !exec_tmp_table->s->is_distinct) {
        // 1st tmp table were materializing join result
        materialize_join = true;
        explain_flags.set(ESC_BUFFER_RESULT, ESP_USING_TMPTABLE);
      }
      curr_tmp_table++;
      tmp_tables++;
      Opt_trace_object trace_this_tbl(trace);
      trace_this_tbl.add("adding_tmp_table_in_plan_at_position", curr_tmp_table)
          .add_alnum("cause", "sorting_to_make_groups");

      /* group data to new table */
      /*
        If the access method is loose index scan then all MIN/MAX
        functions are precomputed, and should be treated as regular
        functions. See extended comment above.
      */
      if (qep_tab[0].range_scan() &&
          is_loose_index_scan(qep_tab[0].range_scan()))
        tmp_table_param.precomputed_group_by = true;

      ORDER_with_src dummy;  // TODO can use table->group here also

      if (create_intermediate_table(&qep_tab[curr_tmp_table], *curr_fields,
                                    dummy, true))
        return true;

      if (!group_list.empty()) {
        explain_flags.set(group_list.src, ESP_USING_TMPTABLE);
        if (!plan_is_const())  // No need to sort a single row
        {
          if (add_sorting_to_table(curr_tmp_table - 1, &group_list,
                                   /*sort_before_group=*/true))
            return true;
        }

        if (make_group_fields(this, this)) return true;
      }

      // Setup sum funcs only when necessary, otherwise we might break info
      // for the first table
      if (!group_list.empty() || tmp_table_param.sum_func_count) {
        if (make_sum_func_list(*curr_fields, true, true)) return true;
        const bool need_distinct =
            !(qep_tab[0].range_scan() &&
              is_agg_loose_index_scan(qep_tab[0].range_scan()));
        if (prepare_sum_aggregators(sum_funcs, need_distinct)) return true;
        group_list.clean();
        if (setup_sum_funcs(thd, sum_funcs)) return true;
      }

      /*
        Allocate a slice of ref items that describe the items to be copied
        from the second temporary table.
      */
      if (alloc_ref_item_slice(thd, REF_SLICE_TMP2)) return true;

      // No sum funcs anymore
      if (change_to_use_tmp_fields(&tmp_fields[REF_SLICE_TMP1], thd,
                                   ref_items[REF_SLICE_TMP2],
                                   &tmp_fields[REF_SLICE_TMP2],
                                   query_block->m_added_non_hidden_fields))
        return true;

      curr_fields = &tmp_fields[REF_SLICE_TMP2];
      set_ref_item_slice(REF_SLICE_TMP2);
      qep_tab[curr_tmp_table].ref_item_slice = REF_SLICE_TMP2;
      setup_tmptable_write_func(&qep_tab[curr_tmp_table], &trace_this_tbl);
    }
    if (qep_tab[curr_tmp_table].table()->s->is_distinct)
      select_distinct = false; /* Each row is unique */

    if (select_distinct && group_list.empty() && !m_windowing_steps) {
      if (having_cond) {
        qep_tab[curr_tmp_table].having = having_cond;
        having_cond->update_used_tables();
        having_cond = nullptr;
      }
      qep_tab[curr_tmp_table].needs_duplicate_removal = true;
      trace_this_outer.add("reading_from_table_eliminates_duplicates", true);
      explain_flags.set(ESC_DISTINCT, ESP_DUPS_REMOVAL);
      select_distinct = false;
    }
    /* Clean tmp_table_param for the next tmp table. */
    tmp_table_param.sum_func_count = tmp_table_param.func_count = 0;

    tmp_table_param.cleanup();
    streaming_aggregation = false;

    if (!group_optimized_away) {
      grouped = false;
    } else {
      /*
        If grouping has been optimized away, a temporary table is
        normally not needed unless we're explicitly requested to create
        one (e.g. due to a SQL_BUFFER_RESULT hint or INSERT ... SELECT or
        there is a windowing function that needs sorting).

        In this case (grouping was optimized away), temp_table was
        created without a grouping expression and JOIN::exec() will not
        perform the necessary grouping (by the use of end_send_group()
        or end_write_group()) if JOIN::group is set to false.
      */
      /*
         The temporary table was explicitly requested or there is a window
         function which needs sorting (check need_tmp_before_win in
         JOIN::optimize).
      */
      assert(query_block->active_options() & OPTION_BUFFER_RESULT ||
             m_windowing_steps);
      // the temporary table does not have a grouping expression
      assert(!qep_tab[curr_tmp_table].table()->group);
    }
    calc_group_buffer(this, group_list.order);
    count_field_types(query_block, &tmp_table_param, *curr_fields, false,
                      false);
  }

  /*
    Set up structures for a temporary table but do not actually create
    the temporary table if one of these conditions are true:
    - The query is implicitly grouped.
    - The query is explicitly grouped and
        + implemented as a simple grouping, or
        + LIMIT 1 is specified, or
        + ROLLUP is specified, or
        + <some unknown condition>.
  */

  if ((grouped || implicit_grouping) && !m_windowing_steps) {
    if (make_group_fields(this, this)) return true;

    if (make_sum_func_list(*curr_fields, true, true)) return true;
    const bool need_distinct =
        !(qep_tab && qep_tab[0].range_scan() &&
          is_agg_loose_index_scan(qep_tab[0].range_scan()));
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) return true;
    if (setup_sum_funcs(thd, sum_funcs) || thd->is_fatal_error()) return true;
  }

  if (qep_tab && (!group_list.empty() ||
                  (!order.empty() && !m_windowing_steps /* [1] */))) {
    /*
      [1] above: too early to do query ORDER BY if we have windowing; must
      wait till after window processing.
    */
    ASSERT_BEST_REF_IN_JOIN_ORDER(this);
    DBUG_PRINT("info", ("Sorting for send_result_set_metadata"));
    /*
      If we have already done the group, add HAVING to sorted table except
      when rollup is present
    */
    if (having_cond && group_list.empty() && !streaming_aggregation &&
        rollup_state == RollupState::NONE) {
      if (add_having_as_tmp_table_cond(curr_tmp_table)) return true;
    }

    if (grouped)
      m_select_limit = HA_POS_ERROR;
    else if (!need_tmp_before_win) {
      /*
        We can abort sorting after thd->select_limit rows if there are no
        filter conditions for any tables after the sorted one.
        Filter conditions come in several forms:
         1. as a condition item attached to the join_tab, or
         2. as a keyuse attached to the join_tab (ref access).
      */
      for (uint i = const_tables + 1; i < primary_tables; i++) {
        QEP_TAB *const tab = qep_tab + i;
        if (tab->condition() ||  // 1
            (best_ref[tab->idx()]->keyuse() &&
             tab->first_inner() == NO_PLAN_IDX))  // 2
        {
          /* We have to sort all rows */
          m_select_limit = HA_POS_ERROR;
          break;
        }
      }
    }
    /*
      Here we add sorting stage for ORDER BY/GROUP BY clause, if the
      optimiser chose FILESORT to be faster than INDEX SCAN or there is
      no suitable index present.
      OPTION_FOUND_ROWS supersedes LIMIT and is taken into account.
    */
    DBUG_PRINT("info", ("Sorting for order by/group by"));
    ORDER_with_src order_arg = group_list.empty() ? order : group_list;
    if (qep_tab &&
        m_ordered_index_usage != (group_list.empty()
                                      ? ORDERED_INDEX_ORDER_BY
                                      : ORDERED_INDEX_GROUP_BY) &&
        // Windowing will change order, so it's too early to sort here
        !m_windowing_steps) {
      // Sort either first non-const table or the last tmp table
      QEP_TAB *const sort_tab = &qep_tab[curr_tmp_table];
      if (need_tmp_before_win && !materialize_join && !exec_tmp_table->group)
        explain_flags.set(order_arg.src, ESP_USING_TMPTABLE);

      if (add_sorting_to_table(curr_tmp_table, &order_arg,
                               /*sort_before_group=*/false))
        return true;
      /*
        filesort_limit:	 Return only this many rows from filesort().
        We can use select_limit_cnt only if we have no group_by and 1 table.
        This allows us to use Bounded_queue for queries like:
          "select * from t1 order by b desc limit 1;"
        m_select_limit == HA_POS_ERROR (we need a full table scan)
        query_expression->select_limit_cnt == 1 (we only need one row in the
        result set)
      */
      if (sort_tab->filesort)
        sort_tab->filesort->limit =
            (has_group_by || (primary_tables > curr_tmp_table + 1) ||
             calc_found_rows)
                ? m_select_limit
                : query_expression()->select_limit_cnt;
    }
  }

  if (qep_tab && m_windowing_steps) {
    for (uint wno = 0; wno < m_windows.elements; wno++) {
      tmp_table_param.m_window = m_windows[wno];

      if (!tmp_tables) {
        curr_tmp_table = primary_tables;
        tmp_tables++;

        if (ref_items[REF_SLICE_SAVED_BASE].is_null()) {
          /*
           Make a copy of the base slice in the save slice.
           This is needed because later steps will overwrite the base slice with
           another slice (1-3 or window slice).
           After this slice has been used, overwrite the base slice again with
           the copy in the save slice.
           */
          if (alloc_ref_item_slice(thd, REF_SLICE_SAVED_BASE)) return true;

          copy_ref_item_slice(REF_SLICE_SAVED_BASE, REF_SLICE_ACTIVE);
          current_ref_item_slice = REF_SLICE_SAVED_BASE;
        }
      } else {
        curr_tmp_table++;
        tmp_tables++;
      }

      ORDER_with_src dummy;

      tmp_table_param.hidden_field_count = CountHiddenFields(*curr_fields);

      /*
        Allocate a slice of ref items that describe the items to be copied
        from the next temporary table.
      */
      const uint widx = REF_SLICE_WIN_1 + wno;

      QEP_TAB *tab = &qep_tab[curr_tmp_table];
      mem_root_deque<Item *> *orig_fields = curr_fields;
      {
        Opt_trace_object trace_this_tbl(trace);
        trace_this_tbl
            .add("adding_tmp_table_in_plan_at_position", curr_tmp_table)
            .add_alnum("cause", "output_for_window_functions")
            .add("with_buffer", m_windows[wno]->needs_buffering());

        if (create_intermediate_table(tab, *curr_fields, dummy, false))
          return true;

        if (alloc_ref_item_slice(thd, widx)) return true;

        if (change_to_use_tmp_fields(curr_fields, thd, ref_items[widx],
                                     &tmp_fields[widx],
                                     query_block->m_added_non_hidden_fields))
          return true;

        curr_fields = &tmp_fields[widx];
        set_ref_item_slice(widx);
        tab->ref_item_slice = widx;
        setup_tmptable_write_func(tab, &trace_this_tbl);
      }

      if (m_windows[wno]->needs_buffering()) {
        if (CreateFramebufferTable(
                thd, tmp_table_param, *query_block, *orig_fields, *curr_fields,
                tab->tmp_table_param->items_to_copy, m_windows[wno])) {
          return true;
        }
      }

      if (m_windows[wno]->make_special_rows_cache(thd, tab->table()))
        return true;

      ORDER_with_src w_partition(m_windows[wno]->sorting_order(),
                                 ESC_WINDOWING);

      if (w_partition.order != nullptr) {
        Opt_trace_object trace_pre_sort(trace, "adding_sort_to_previous_table");
        if (add_sorting_to_table(curr_tmp_table - 1, &w_partition,
                                 /*sort_before_group=*/false))
          return true;
      }

      if (m_windows[wno]->is_last()) {
        if (!order.empty() && m_ordered_index_usage != ORDERED_INDEX_ORDER_BY) {
          if (add_sorting_to_table(curr_tmp_table, &order,
                                   /*sort_before_group=*/false))
            return true;
        }
        if (!tab->filesort && !tab->table()->s->keys &&
            (!(query_block->active_options() & OPTION_BUFFER_RESULT) ||
             need_tmp_before_win || wno >= 1)) {
          /*
            Last tmp table of execution; no sort, no duplicate elimination, no
            buffering imposed by user (or it has already been implemented by
            a previous tmp table): hence any row needn't be written to
            tmp table's storage; send it out to query's result instead:
          */
          m_windows[wno]->set_short_circuit(true);
        }
      }

      if (having_cond != nullptr) {
        tab->having = having_cond;
        having_cond = nullptr;
      }
    }
  }

  // In the case of rollup (only): After the base slice list was made, we may
  // have modified the field list to add rollup group items and sum switchers.
  // Since there may be HAVING filters with refs that refer to the base slice,
  // we need to refresh that slice (and its copy, REF_SLICE_SAVED_BASE) so
  // that it includes the updated items.
  //
  // Note that we do this after we've made the TMP1 and TMP2 slices, since
  // there's a lot of logic that looks through the GROUP BY list, which refers
  // to the base slice and expects _not_ to find rollup items there.
  refresh_base_slice();

  fields = curr_fields;
  // Reset before execution
  set_ref_item_slice(REF_SLICE_SAVED_BASE);
  if (qep_tab) {
    qep_tab[primary_tables + tmp_tables].op_type = get_end_select_func();
  }
  grouped = has_group_by;

  unplug_join_tabs();

  /*
    Tmp tables are a layer between the nested loop and the derived table's
    result, WITH RECURSIVE cannot work with them. This should not happen, as a
    recursive query cannot have clauses which use a tmp table (GROUP BY,
    etc).
  */
  assert(!query_block->is_recursive() || !tmp_tables);
  cleanup_tmp_tables_on_error.commit();
  return false;
}

void JOIN::refresh_base_slice() {
  unsigned num_hidden_fields = CountHiddenFields(*fields);
  const size_t num_select_elements = fields->size() - num_hidden_fields;
  const size_t orig_num_select_elements =
      num_select_elements - query_block->m_added_non_hidden_fields;

  for (unsigned i = 0; i < fields->size(); ++i) {
    Item *item = (*fields)[i];
    size_t pos;
    // See change_to_use_tmp_fields_except_sums for an explanation of how
    // the visible fields, hidden fields and additional fields added by
    // transformations are organized in fields and ref_item_array.
    if (i < num_hidden_fields) {
      pos = fields->size() - i - 1 - query_block->m_added_non_hidden_fields;
    } else {
      pos = i - num_hidden_fields;
      if (pos >= orig_num_select_elements) pos += num_hidden_fields;
    }
    query_block->base_ref_items[pos] = item;
    if (!ref_items[REF_SLICE_SAVED_BASE].is_null()) {
      ref_items[REF_SLICE_SAVED_BASE][pos] = item;
    }
  }
}

void JOIN::unplug_join_tabs() {
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);

  map2table = nullptr;

  for (uint i = 0; i < tables; ++i) best_ref[i]->cleanup();

  best_ref = nullptr;
}

/**
  @brief Add Filesort object to the given table to sort if with filesort

  @param idx        JOIN_TAB's position in the qep_tab array. The
                    created Filesort object gets attached to this.

  @param sort_order List of expressions to sort the table by
  @param sort_before_group
                    If true, this sort happens before grouping is done
                    (potentially as a step of grouping itself),
                    so any wrapped rollup group items should be
                    unwrapped.

  @note This function moves tab->select, if any, to filesort->select

  @return false on success, true on OOM
*/

bool JOIN::add_sorting_to_table(uint idx, ORDER_with_src *sort_order,
                                bool sort_before_group) {
  DBUG_TRACE;
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);
  assert(!query_block->is_recursive());
  const enum join_type jt = qep_tab[idx].type();
  if (jt == JT_CONST || jt == JT_EQ_REF)
    return false;  // 1 single row: is already sorted

  // Weedout needs an underlying table to store refs from (it deduplicates
  // by row ID), so if this table is part of a weedout operation, we need
  // to force sorting by row IDs -- sorting rows with addon fields returns
  // rows that have no reference to the underlying table object.
  bool force_sort_rowids = false;
  for (plan_idx i = 0; i <= static_cast<plan_idx>(idx); ++i) {
    if (!qep_tab[i].starts_weedout()) {
      continue;
    }

    plan_idx weedout_end = NO_PLAN_IDX;  // Exclusive.
    for (uint j = i; j < primary_tables; ++j) {
      if (qep_tab[j].check_weed_out_table == qep_tab[i].flush_weedout_table) {
        weedout_end = j + 1;
        break;
      }
    }
    if (weedout_end != NO_PLAN_IDX &&
        weedout_end > static_cast<plan_idx>(idx)) {
      force_sort_rowids = true;
      break;
    }
  }

  explain_flags.set(sort_order->src, ESP_USING_FILESORT);
  QEP_TAB *const tab = &qep_tab[idx];
  bool keep_buffers =
      qep_tab->join() != nullptr &&
      qep_tab->join()->query_block->master_query_expression()->item !=
          nullptr &&
      qep_tab->join()
          ->query_block->master_query_expression()
          ->item->is_uncacheable();

  {
    // Switch to the right slice if applicable, so that we fetch out the correct
    // items from order_arg.
    Switch_ref_item_slice slice_switch(this, tab->ref_item_slice);
    tab->filesort = new (thd->mem_root)
        Filesort(thd, {tab->table()}, keep_buffers, sort_order->order,
                 HA_POS_ERROR, /*remove_duplicates=*/false, force_sort_rowids,
                 /*unwrap_rollup=*/sort_before_group);
    tab->filesort_pushed_order = sort_order->order;
  }
  if (!tab->filesort) return true;
  Opt_trace_object trace_tmp(&thd->opt_trace, "filesort");
  trace_tmp.add_alnum("adding_sort_to_table",
                      tab->table() ? tab->table()->alias : "");

  return false;
}

/**
  Find a cheaper access key than a given key.

  @param          tab                 NULL or JOIN_TAB of the accessed table
  @param          order               Linked list of ORDER BY arguments
  @param          table               Table if tab == NULL or tab->table()
  @param          usable_keys         Key map to find a cheaper key in
  @param          ref_key
                * 0 <= key < MAX_KEY   - key number (hint) to start the search
                * -1                   - no key number provided
  @param          select_limit        LIMIT value, or HA_POS_ERROR if no limit
  @param [out]    new_key             Key number if success, otherwise undefined
  @param [out]    new_key_direction   Return -1 (reverse) or +1 if success,
                                      otherwise undefined
  @param [out]    new_select_limit    Return adjusted LIMIT
  @param [out]    new_used_key_parts  NULL by default, otherwise return number
                                      of new_key prefix columns if success
                                      or undefined if the function fails
  @param [out]  saved_best_key_parts  NULL by default, otherwise preserve the
                                      value for further use in
                                      ReverseIndexRangeScanIterator

  @note
    This function takes into account table->quick_condition_rows statistic
    (that is calculated by JOIN::make_join_plan()).
    However, single table procedures such as mysql_update() and mysql_delete()
    never call JOIN::make_join_plan(), so they have to update it manually
    (@see get_index_for_order()).
    This function resets bits in TABLE::quick_keys for indexes with mixed
    ASC/DESC keyparts as range scan doesn't support range reordering
    required for them.
*/

bool test_if_cheaper_ordering(const JOIN_TAB *tab, ORDER_with_src *order,
                              TABLE *table, Key_map usable_keys, int ref_key,
                              ha_rows select_limit, int *new_key,
                              int *new_key_direction, ha_rows *new_select_limit,
                              uint *new_used_key_parts,
                              uint *saved_best_key_parts) {
  DBUG_TRACE;
  /*
    Check whether there is an index compatible with the given order
    usage of which is cheaper than usage of the ref_key index (ref_key>=0)
    or a table scan.
    It may be the case if ORDER/GROUP BY is used with LIMIT.
  */
  ha_rows best_select_limit = HA_POS_ERROR;
  JOIN *join = tab ? tab->join() : nullptr;
  if (join) ASSERT_BEST_REF_IN_JOIN_ORDER(join);
  uint nr;
  uint best_key_parts = 0;
  int best_key_direction = 0;
  ha_rows best_records = 0;
  double read_time;
  int best_key = -1;
  bool is_best_covering = false;
  double fanout = 1;
  ha_rows table_records = table->file->stats.records;
  bool group = join && join->grouped && order == &join->group_list;
  double refkey_rows_estimate =
      static_cast<double>(table->quick_condition_rows);
  const bool has_limit = (select_limit != HA_POS_ERROR);
  const join_type cur_access_method = tab ? tab->type() : JT_ALL;

  if (join) {
    read_time = tab->position()->read_cost;
    for (uint jt = tab->idx() + 1; jt < join->primary_tables; jt++) {
      POSITION *pos = join->best_ref[jt]->position();
      fanout *= pos->rows_fetched * pos->filter_effect;
      if (fanout < 0) break;  // fanout became 'unknown'
    }
  } else
    read_time = table->file->table_scan_cost().total_cost();

  /*
    Calculate the selectivity of the ref_key for REF_ACCESS. For
    RANGE_ACCESS we use table->quick_condition_rows.
  */
  if (ref_key >= 0 && cur_access_method == JT_REF) {
    if (table->quick_keys.is_set(ref_key))
      refkey_rows_estimate = static_cast<double>(table->quick_rows[ref_key]);
    else {
      const KEY *ref_keyinfo = table->key_info + ref_key;
      if (ref_keyinfo->has_records_per_key(tab->ref().key_parts - 1))
        refkey_rows_estimate =
            ref_keyinfo->records_per_key(tab->ref().key_parts - 1);
      else
        refkey_rows_estimate = 1.0;  // No index statistics
    }
    assert(refkey_rows_estimate >= 1.0);
  }

  for (nr = 0; nr < table->s->keys; nr++) {
    int direction = 0;
    uint used_key_parts;
    bool skip_quick;

    if (usable_keys.is_set(nr) &&
        (direction = test_if_order_by_key(order, table, nr, &used_key_parts,
                                          &skip_quick))) {
      bool is_covering = table->covering_keys.is_set(nr) ||
                         (nr == table->s->primary_key &&
                          table->file->primary_key_is_clustered());
      // Don't allow backward scans on indexes with mixed ASC/DESC key parts
      if (skip_quick) table->quick_keys.clear_bit(nr);

      /*
        Don't use an index scan with ORDER BY without limit.
        For GROUP BY without limit always use index scan
        if there is a suitable index.
        Why we hold to this asymmetry hardly can be explained
        rationally. It's easy to demonstrate that using
        temporary table + filesort could be cheaper for grouping
        queries too.
      */
      if (is_covering || select_limit != HA_POS_ERROR ||
          (ref_key < 0 && (group || table->force_index_order))) {
        rec_per_key_t rec_per_key;
        KEY *keyinfo = table->key_info + nr;
        if (select_limit == HA_POS_ERROR) select_limit = table_records;
        if (group) {
          /*
            Used_key_parts can be larger than keyinfo->key_parts
            when using a secondary index clustered with a primary
            key (e.g. as in Innodb).
            See Bug #28591 for details.
          */
          rec_per_key =
              used_key_parts && used_key_parts <= actual_key_parts(keyinfo)
                  ? keyinfo->records_per_key(used_key_parts - 1)
                  : 1.0f;
          rec_per_key = std::max(rec_per_key, 1.0f);
          /*
            With a grouping query each group containing on average
            rec_per_key records produces only one row that will
            be included into the result set.
          */
          if (select_limit > table_records / rec_per_key)
            select_limit = table_records;
          else
            select_limit = (ha_rows)(select_limit * rec_per_key);
        }
        /*
          If tab=tk is not the last joined table tn then to get first
          L records from the result set we can expect to retrieve
          only L/fanout(tk,tn) where fanout(tk,tn) says how many
          rows in the record set on average will match each row tk.
          Usually our estimates for fanouts are too pessimistic.
          So the estimate for L/fanout(tk,tn) will be too optimistic
          and as result we'll choose an index scan when using ref/range
          access + filesort will be cheaper.
        */
        if (fanout == 0) {              // Would have been a division-by-zero
          select_limit = HA_POS_ERROR;  // -> 'infinite'
        } else if (fanout >= 0) {       // 'fanout' not unknown
          const double new_limit = max(select_limit / fanout, 1.0);
          if (new_limit >= static_cast<double>(HA_POS_ERROR)) {
            select_limit = HA_POS_ERROR;
          } else {
            select_limit = new_limit;
          }
        }
        /*
          We assume that each of the tested indexes is not correlated
          with ref_key. Thus, to select first N records we have to scan
          N/selectivity(ref_key) index entries.
          selectivity(ref_key) = #scanned_records/#table_records =
          refkey_rows_estimate/table_records.
          In any case we can't select more than #table_records.
          N/(refkey_rows_estimate/table_records) > table_records
          <=> N > refkey_rows_estimate.
          Neither should it be possible to select more than #table_records
          rows from refkey_rows_estimate.
         */
        if (select_limit > refkey_rows_estimate)
          select_limit = table_records;
        else if (table_records >= refkey_rows_estimate)
          select_limit = (ha_rows)(select_limit * (double)table_records /
                                   refkey_rows_estimate);
        rec_per_key =
            keyinfo->records_per_key(keyinfo->user_defined_key_parts - 1);
        rec_per_key = std::max(rec_per_key, 1.0f);
        /*
          Here we take into account the fact that rows are
          accessed in sequences rec_per_key records in each.
          Rows in such a sequence are supposed to be ordered
          by rowid/primary key. When reading the data
          in a sequence we'll touch not more pages than the
          table file contains.
          TODO. Use the formula for a disk sweep sequential access
          to calculate the cost of accessing data rows for one
          index entry.
        */
        const Cost_estimate table_scan_time = table->file->table_scan_cost();
        const double index_scan_time =
            select_limit / rec_per_key *
            min<double>(table->file->page_read_cost(nr, rec_per_key),
                        table_scan_time.total_cost());

        /*
          Switch to index that gives order if its scan time is smaller than
          read_time of current chosen access method. In addition, if the
          current chosen access method is index scan or table scan, always
          switch to the index that gives order when it is covering or when
          force index order or group by is present.
        */
        if (((cur_access_method == JT_ALL ||
              cur_access_method == JT_INDEX_SCAN) &&
             (is_covering || group || table->force_index_order)) ||
            index_scan_time < read_time) {
          ha_rows quick_records = table_records;
          const ha_rows refkey_select_limit =
              (ref_key >= 0 && table->covering_keys.is_set(ref_key))
                  ? static_cast<ha_rows>(refkey_rows_estimate)
                  : HA_POS_ERROR;

          if ((is_best_covering && !is_covering) ||
              (is_covering && refkey_select_limit < select_limit))
            continue;
          if (table->quick_keys.is_set(nr))
            quick_records = table->quick_rows[nr];
          if (best_key < 0 ||
              (select_limit <= min(quick_records, best_records)
                   ? keyinfo->user_defined_key_parts < best_key_parts
                   : quick_records < best_records) ||
              // We assume forward scan is faster than backward even if the
              // key is longer. This should be taken into account in cost
              // calculation.
              direction > best_key_direction) {
            best_key = nr;
            best_key_parts = keyinfo->user_defined_key_parts;
            if (saved_best_key_parts) *saved_best_key_parts = used_key_parts;
            best_records = quick_records;
            is_best_covering = is_covering;
            best_key_direction = direction;
            best_select_limit = select_limit;
          }
        }
      }
    }
  }

  if (best_key < 0 || best_key == ref_key) return false;

  *new_key = best_key;
  *new_key_direction = best_key_direction;
  *new_select_limit = has_limit ? best_select_limit : table_records;
  if (new_used_key_parts != nullptr) *new_used_key_parts = best_key_parts;

  return true;
}

/**
  Find a key to apply single table UPDATE/DELETE by a given ORDER

  @param       order           Linked list of ORDER BY arguments
  @param       table           Table to find a key
  @param       limit           LIMIT clause parameter
  @param       range_scan      Range scan used for this table, if any
  @param [out] need_sort       true if filesort needed
  @param [out] reverse
    true if the key is reversed again given ORDER (undefined if key == MAX_KEY)

  @return
    - MAX_KEY if no key found                        (need_sort == true)
    - MAX_KEY if quick select result order is OK     (need_sort == false)
    - key number (either index scan or quick select) (need_sort == false)

  @note
    Side effects:
    - may deallocate or deallocate and replace select->quick;
    - may set table->quick_condition_rows and table->quick_rows[...]
      to table->file->stats.records.
*/

uint get_index_for_order(ORDER_with_src *order, TABLE *table, ha_rows limit,
                         AccessPath *range_scan, bool *need_sort,
                         bool *reverse) {
  if (range_scan &&
      unique_key_range(range_scan)) {  // Single row select (always
                                       // "ordered"): Ok to use with
                                       // key field UPDATE
    *need_sort = false;
    /*
      Returning of MAX_KEY here prevents updating of used_key_is_modified
      in mysql_update(). Use AccessPath "as is".
    */
    return MAX_KEY;
  }

  if (order->empty()) {
    *need_sort = false;
    if (range_scan)
      return used_index(range_scan);  // index or MAX_KEY, use AccessPath as is
    else
      return table->file
          ->key_used_on_scan;  // MAX_KEY or index for some engines
  }

  if (!is_simple_order(order->order))  // just to cut further expensive checks
  {
    *need_sort = true;
    return MAX_KEY;
  }

  if (range_scan) {
    if (used_index(range_scan) == MAX_KEY) {
      *need_sort = true;
      return MAX_KEY;
    }

    uint used_key_parts;
    bool skip_path;
    switch (test_if_order_by_key(order, table, used_index(range_scan),
                                 &used_key_parts, &skip_path)) {
      case 1:  // desired order
        *need_sort = false;
        return used_index(range_scan);
      case 0:  // unacceptable order
        *need_sort = true;
        return MAX_KEY;
      case -1:  // desired order, but opposite direction
      {
        if (!skip_path && !make_reverse(used_key_parts, range_scan)) {
          *need_sort = false;
          return used_index(range_scan);
        } else {
          *need_sort = true;
          return MAX_KEY;
        }
      }
    }
    assert(0);
  } else if (limit != HA_POS_ERROR) {  // check if some index scan & LIMIT is
                                       // more efficient than filesort

    /*
      Update quick_condition_rows since single table UPDATE/DELETE procedures
      don't call JOIN::make_join_plan() and leave this variable uninitialized.
    */
    table->quick_condition_rows = table->file->stats.records;

    int key, direction;
    if (test_if_cheaper_ordering(nullptr, order, table,
                                 table->keys_in_use_for_order_by, -1, limit,
                                 &key, &direction, &limit)) {
      *need_sort = false;
      *reverse = (direction < 0);
      return key;
    }
  }
  *need_sort = true;
  return MAX_KEY;
}

/**
  Returns number of key parts depending on
  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS flag.

  @param  key_info  pointer to KEY structure

  @return number of key parts.
*/

uint actual_key_parts(const KEY *key_info) {
  return current_thd->optimizer_switch_flag(
             OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
             ? key_info->actual_key_parts
             : key_info->user_defined_key_parts;
}

uint actual_key_flags(const KEY *key_info) {
  return current_thd->optimizer_switch_flag(
             OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
             ? key_info->actual_flags
             : key_info->flags;
}

join_type calc_join_type(AccessPath *path) {
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
    case AccessPath::INDEX_SKIP_SCAN:
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      return JT_RANGE;
    case AccessPath::INDEX_MERGE:
    case AccessPath::ROWID_INTERSECTION:
    case AccessPath::ROWID_UNION:
      return JT_INDEX_MERGE;
    default:
      assert(false);
      return JT_RANGE;
  }
}

/**
  @} (end of group Query_Optimizer)
*/

/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "lex_string.h"
#include "memory_debugging.h"
#include "my_alloc.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // *_ACL
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/enum_query_type.h"
#include "sql/error_handler.h"  // Ignore_error_handler
#include "sql/filesort.h"       // filesort_free_buffers
#include "sql/handler.h"
#include "sql/item_func.h"
#include "sql/item_subselect.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/key.h"       // key_copy, key_cmp, key_cmp_if_same
#include "sql/lock.h"      // mysql_unlock_some_tables,
#include "sql/my_decimal.h"
#include "sql/mysqld.h"  // stage_init
#include "sql/nested_join.h"
#include "sql/opt_explain.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_hints.h"  // hint_key_state()
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/opt_trace.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/records.h"  // init_read_record, end_read_record
#include "sql/sql_base.h"
#include "sql/sql_do.h"
#include "sql/sql_executor.h"
#include "sql/sql_join_buffer.h"  // JOIN_CACHE
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_planner.h"    // calculate_condition_filter
#include "sql/sql_show.h"       // append_identifier
#include "sql/sql_sort.h"
#include "sql/sql_test.h"       // misc. debug printing utilities
#include "sql/sql_timer.h"      // thd_timer_set
#include "sql/sql_tmp_table.h"  // tmp tables
#include "sql/temp_table_param.h"
#include "sql/window.h"  // ignore_gaf_const_opt
#include "template_utils.h"
#include "thr_lock.h"

class Opt_trace_context;

using std::max;
using std::min;

const char store_key_const_item::static_name[] = "const";

static store_key *get_store_key(THD *thd, Key_use *keyuse,
                                table_map used_tables, KEY_PART_INFO *key_part,
                                uchar *key_buff, uint maybe_null);
static uint actual_key_flags(KEY *key_info);
bool const_expression_in_where(Item *conds, Item *item, Item **comp_item);

/**
  Handle data manipulation query which is not represented by Sql_cmd_dml class.
  @todo: Integrate with Sql_cmd_dml::prepare() and ::execute()

  @param thd       thread handler
  @param lex       query to be processed
  @param result    sink of result of query execution.
                   may be protocol object (for passing result to a client),
                   insert object, update object, delete object, etc.
  @param added_options additional options for detailed control over execution
  @param removed_options options that are not applicable for this command

  @returns false if success, true if error

  @details
    Processing a query goes through 5 phases (parsing is already done)
     - Preparation
     - Locking of tables
     - Optimization
     - Execution or explain
     - Cleanup

    The statements handled by this function are:

      CREATE TABLE with SELECT clause
      SHOW statements
*/

bool handle_query(THD *thd, LEX *lex, Query_result *result,
                  ulonglong added_options, ulonglong removed_options) {
  DBUG_ENTER("handle_query");

  SELECT_LEX_UNIT *const unit = lex->unit;
  SELECT_LEX *const select = unit->first_select();
  bool res;

  DBUG_ASSERT(thd == unit->thd);

  DBUG_ASSERT(!unit->is_prepared() && !unit->is_optimized() &&
              !unit->is_executed());

  const bool single_query = unit->is_simple();

  lex->used_tables = 0;  // Updated by setup_fields

  THD_STAGE_INFO(thd, stage_init);

  if (thd->lex->set_var_list.elements && resolve_var_assignments(thd, lex))
    goto err;

  if (single_query) {
    if (unit->prepare_limit(thd, unit->global_parameters()))
      goto err; /* purecov: inspected */

    select->context.resolve_in_select_list = true;
    select->set_query_result(result);
    select->make_active_options(added_options, removed_options);
    select->fields_list = select->item_list;

    if (select->prepare(thd)) goto err;

    unit->set_prepared();
  } else {
    if (unit->prepare(thd, result, SELECT_NO_UNLOCK | added_options,
                      removed_options))
      goto err;
  }

  lex->set_exec_started();

  DBUG_ASSERT(!lex->is_query_tables_locked());
  /*
    Locking of tables is done after preparation but before optimization.
    This allows to do better partition pruning and avoid locking unused
    partitions. As a consequence, in such a case, prepare stage can rely only
    on metadata about tables used and not data from them.
  */
  if (lock_tables(thd, lex->query_tables, lex->table_count, 0)) goto err;

  if (single_query) {
    if (unit->set_limit(thd, unit->global_parameters()))
      goto err; /* purecov: inspected */
    if (select->optimize(thd)) goto err;

    unit->set_optimized();
  } else {
    if (unit->optimize(thd)) goto err;
  }

  if (lex->is_explain()) {
    if (explain_query(thd, unit)) goto err; /* purecov: inspected */
  } else {
    if (single_query) {
      select->join->exec();
      unit->set_executed();
      if (thd->is_error()) goto err;
    } else {
      if (unit->execute(thd)) goto err;
    }
  }

  DBUG_ASSERT(!thd->is_error());

  thd->update_previous_found_rows();
  THD_STAGE_INFO(thd, stage_end);

  // Do partial cleanup (preserve plans for EXPLAIN).
  res = unit->cleanup(false);

  DBUG_RETURN(res);

err:
  DBUG_ASSERT(thd->is_error() || thd->killed);
  DBUG_PRINT("info", ("report_error: %d", thd->is_error()));
  THD_STAGE_INFO(thd, stage_end);

  (void)unit->cleanup(false);

  // Abort the result set (if it has been prepared).
  result->abort_result_set();

  DBUG_RETURN(thd->is_error());
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
  DBUG_ASSERT(is_timer_applicable_to_statement(thd) == true);
  DBUG_ASSERT(thd->timer == NULL);

  thd->timer = thd_timer_set(thd, thd->timer_cache, max_execution_time);
  thd->timer_cache = NULL;

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
  DBUG_ASSERT(thd->timer);
  /* Cache the timer object if it can be reused. */
  thd->timer_cache = thd_timer_reset(thd->timer);
  thd->timer = NULL;
}

/**
  Prepare a DML statement.

  @param thd       thread handler

  @returns false if success, true if error
*/
bool Sql_cmd_dml::prepare(THD *thd) {
  DBUG_ENTER("Sql_cmd_dml::prepare");

  // @todo: Move this to constructor?
  lex = thd->lex;
  result = lex->result;

  SELECT_LEX_UNIT *const unit = lex->unit;

  DBUG_ASSERT(thd == unit->thd && !is_prepared());

  DBUG_ASSERT(!unit->is_prepared() && !unit->is_optimized() &&
              !unit->is_executed());

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
    (void)unit->cleanup(false);
    DBUG_RETURN(true);
  }
#ifndef DBUG_OFF
  if (sql_command_code() == SQLCOM_SELECT) DEBUG_SYNC(thd, "after_table_open");
#endif
  if (lex->set_var_list.elements && resolve_var_assignments(thd, lex))
    goto err; /* purecov: inspected */

  /*
    @todo add consistent error tracking for all query preparation by adding
    this line: (when done, remove all other occurrences in preparation code)

    Prepare_error_tracker tracker(thd);
  */

  if (prepare_inner(thd)) goto err;

  set_prepared();

  DBUG_RETURN(false);

err:
  DBUG_ASSERT(thd->is_error());
  DBUG_PRINT("info", ("report_error: %d", thd->is_error()));

  (void)unit->cleanup(false);

  DBUG_RETURN(true);
}

/**
  Prepare a SELECT statement.
*/

bool Sql_cmd_select::prepare_inner(THD *thd) {
  lex->used_tables = 0;  // Updated by setup_fields

  if (lex->is_explain()) {
    /*
      Always use Query_result_send for EXPLAIN, even if it's an EXPLAIN for
      SELECT ... INTO OUTFILE: a user application should be able to prepend
      EXPLAIN to any query and receive output for it, even if the query itself
      redirects the output.
    */
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    result = new (thd->mem_root) Query_result_send(thd);
    if (!result) return true; /* purecov: inspected */
  } else {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    if (lex->result == NULL) {
      if (sql_command_code() == SQLCOM_SELECT)
        lex->result = new (thd->mem_root) Query_result_send(thd);
      else if (sql_command_code() == SQLCOM_DO)
        lex->result = new (thd->mem_root) Query_result_do(thd);
      if (lex->result == NULL) return true; /* purecov: inspected */
    }
    result = lex->result;
  }

  SELECT_LEX_UNIT *const unit = lex->unit;
  SELECT_LEX *parameters = unit->global_parameters();
  if (!parameters->explicit_limit) {
    parameters->select_limit =
        new (thd->mem_root) Item_uint(ulonglong(thd->variables.select_limit));
    if (parameters->select_limit == NULL) return true; /* purecov: inspected */
  }
  if (unit->prepare_limit(thd, parameters))
    return true; /* purecov: inspected */

  if (unit->is_simple()) {
    SELECT_LEX *const select = unit->first_select();
    select->context.resolve_in_select_list = true;
    select->set_query_result(result);
    select->make_active_options(0, 0);
    select->fields_list = select->item_list;

    if (select->prepare(thd)) return true;

    unit->set_prepared();
  } else {
    if (unit->prepare(thd, result, SELECT_NO_UNLOCK, 0)) return true;
  }

  return false;
}

/**
  Execute a DML statement.

  @param thd       thread handler

  @returns false if success, true if error

  @details
    Processing a statement goes through 6 phases (parsing is already done)
     - Prelocking
     - Preparation
     - Locking of tables
     - Optimization
     - Execution or explain
     - Cleanup

    If the statement is already prepared, this step is skipped.

    The queries handled by this function are:

    SELECT
    INSERT ... SELECT
    INSERT ... VALUES
    REPLACE ... SELECT
    REPLACE ... VALUES
    UPDATE (single-table and multi-table)
    DELETE (single-table and multi-table)
    DO

  @todo make this function also handle SET.
*/

bool Sql_cmd_dml::execute(THD *thd) {
  DBUG_ENTER("Sql_cmd_dml::execute");

  lex = thd->lex;
  result = lex->result;

  SELECT_LEX_UNIT *const unit = lex->unit;

  bool statement_timer_armed = false;
  bool error_handler_active = false;
  bool res;

  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;

  DBUG_ASSERT(thd == unit->thd);
  // @todo - enable when needs_explicit_preparation is changed
  // DBUG_ASSERT(!needs_explicit_preparation() || is_prepared());

  // If a timer is applicable to statement, then set it.
  if (is_timer_applicable_to_statement(thd))
    statement_timer_armed = set_statement_timer(thd);

  if (!is_prepared()) {
    prepare_only = false;  // Indicate that call is from execute
    if (prepare(thd)) goto err;
    prepare_only = true;
  } else {
    /*
      When statement is prepared, authorization check and opening of tables is
      still needed.
    */
    if (precheck(thd)) goto err;
    if (open_tables_for_query(thd, lex->query_tables, 0)) goto err;
#ifndef DBUG_OFF
    if (sql_command_code() == SQLCOM_SELECT)
      DEBUG_SYNC(thd, "after_table_open");
#endif
  }

  lex->set_exec_started();

  DBUG_EXECUTE_IF("use_attachable_trx",
                  thd->begin_attachable_ro_transaction(););

  THD_STAGE_INFO(thd, stage_init);

  thd->clear_current_query_costs();

  thd->current_changed_rows = 0;

  if (is_data_change_stmt()) {
    // Replication may require extra check of data change statements
    if (run_before_dml_hook(thd)) goto err;

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

  DBUG_ASSERT(!lex->is_query_tables_locked());
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

  DBUG_ASSERT(!thd->is_error());

  // Pop ignore / strict error handler
  if (error_handler_active) {
    thd->pop_internal_handler();
    error_handler_active = false;
  }

  THD_STAGE_INFO(thd, stage_end);

  // Do partial cleanup (preserve plans for EXPLAIN).
  res = unit->cleanup(false);
  lex->clear_values_map();

  // Perform statement-specific cleanup for Query_result
  if (result != NULL) result->cleanup();

  thd->save_current_query_costs();

  thd->update_previous_found_rows();

  DBUG_EXECUTE_IF("use_attachable_trx", thd->end_attachable_transaction(););

  if (statement_timer_armed && thd->timer) reset_statement_timer(thd);

  /*
    This sync point is normally right before thd->query_plan is reset, so
    EXPLAIN FOR CONNECTION can catch the plan. It is copied here as
    after unprepare() EXPLAIN considers the query as "not ready".
    @todo remove in WL#6570 together with unprepare().
  */
  DEBUG_SYNC(thd, "before_reset_query_plan");

  // "unprepare" this object since unit->cleanup actually unprepares.
  unprepare(thd);

  DBUG_RETURN(res);

err:
  DBUG_ASSERT(thd->is_error() || thd->killed);
  DBUG_PRINT("info", ("report_error: %d", thd->is_error()));
  THD_STAGE_INFO(thd, stage_end);
  prepare_only = true;

  (void)unit->cleanup(false);
  lex->clear_values_map();

  // Abort and cleanup the result set (if it has been prepared).
  if (result != NULL) {
    result->abort_result_set();
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

  if (is_prepared()) unprepare(thd);

  DBUG_RETURN(thd->is_error());
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
  SELECT_LEX_UNIT *unit = lex->unit;

  if (unit->is_simple()) {
    if (unit->set_limit(thd, unit->global_parameters()))
      return true; /* purecov: inspected */
    if (unit->first_select()->optimize(thd)) return true;

    unit->set_optimized();
  } else {
    if (unit->optimize(thd)) return true;
  }

  if (lex->is_explain()) {
    if (explain_query(thd, unit)) return true; /* purecov: inspected */
  } else {
    if (unit->is_simple()) {
      unit->first_select()->join->exec();
      unit->set_executed();
      if (thd->is_error()) return true;
    } else {
      if (unit->execute(thd)) return true;
    }
  }

  return false;
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
  for (TABLE_LIST *table_ref : tables)
    if (table_ref->lock_descriptor().action != THR_DEFAULT) {
      bool access_is_granted = false;
      /*
        If either of these privileges is present along with SELECT, access is
        granted.
      */
      for (uint allowed_priv : {UPDATE_ACL, DELETE_ACL, LOCK_TABLES_ACL}) {
        ulong priv = SELECT_ACL | allowed_priv;
        if (!check_table_access(thd, priv, table_ref, false, 1, true))
          access_is_granted = true;
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
  Perform an authorization precheck for a SELECT statement.
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

    @todo: The condition below should be enabled when this function is
    extended to handle SHOW statements as well.

      || (first_table && first_table->schema_table_reformed &&
       check_show_access(thd, first_table));
  */
  TABLE_LIST *tables = lex->query_tables;

  if (check_file_acl && check_global_access(thd, FILE_ACL)) return true;

  bool res;
  if (tables)
    res = check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false);
  else
    res = check_access(thd, SELECT_ACL, any_db, nullptr, nullptr, 0, 0);

  return res || check_locking_clause_access(thd, tables);
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
*/

bool types_allow_materialization(Item *outer, Item *inner)

{
  if (outer->result_type() != inner->result_type()) return false;
  switch (outer->result_type()) {
    case ROW_RESULT:
      // Materialization of rows nested inside rows is not currently supported.
      return false;
    case STRING_RESULT:
      if (outer->is_temporal_with_date() != inner->is_temporal_with_date())
        return false;
      if (!(outer->collation.collation == inner->collation.collation
            /*&& outer->max_length <= inner->max_length */))
        return false;
    /*case INT_RESULT:
      if (!(outer->unsigned_flag ^ inner->unsigned_flag))
        return false; */
    default:; /* suitable for materialization */
  }
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
  TABLE_LIST *embedding = join_tab->table_ref->embedding;
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
      (ot|nt)*  [ it ((it|nt)* it) ]  (nt)*
      +------+  +==================+  +---+
        (1)             (2)          (3)

    @endcode
    -# Prefix of outer correlated and non-correlated tables

    -# The handled range, which may contain only inner and non-correlated
       tables.

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
  DBUG_ENTER("setup_semijoin_dups_elimination");
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  if (join->select_lex->sj_nests.is_empty()) DBUG_RETURN(false);

  QEP_TAB *const qep_array = join->qep_tab;
  for (tableno = join->const_tables; tableno < join->primary_tables;) {
#ifndef DBUG_OFF
    const bool tab_in_sj_nest = join->best_ref[tableno]->emb_sj_nest != NULL;
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
        DBUG_ASSERT(false);  // Should not occur among "primary" tables
        // Do nothing
        tableno += pos->n_sj_tables;
        break;
      case SJ_OPT_LOOSE_SCAN: {
        DBUG_ASSERT(tab_in_sj_nest);  // First table must be inner
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
        if (tab->quick()) {
          DBUG_ASSERT(tab->quick()->index == pos->loosescan_key);
          tab->quick()->need_sorted_output();
        }

        const uint keyno = pos->loosescan_key;
        DBUG_ASSERT(tab->keys().is_set(keyno));
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
        DBUG_ASSERT(tab_in_sj_nest);  // First table must be inner
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

        SJ_TMP_TABLE::TAB sjtabs[MAX_TABLES];
        SJ_TMP_TABLE::TAB *last_tab = sjtabs;
        uint jt_rowid_offset =
            0;  // # tuple bytes are already occupied (w/o NULL bytes)
        uint jt_null_bits = 0;  // # null bits in tuple bytes
        /*
          Walk through the range and remember
           - tables that need their rowids to be put into temptable
           - the last outer table
        */
        for (QEP_TAB *tab_in_range = qep_array + first_table;
             tab_in_range <= last_sj_tab; tab_in_range++) {
          if (sj_table_is_included(join, join->best_ref[tab_in_range->idx()])) {
            last_tab->qep_tab = tab_in_range;
            last_tab->rowid_offset = jt_rowid_offset;
            jt_rowid_offset += tab_in_range->table()->file->ref_length;
            if (tab_in_range->table()->is_nullable()) {
              last_tab->null_byte = jt_null_bits / 8;
              last_tab->null_bit = jt_null_bits++;
            }
            last_tab++;
            tab_in_range->table()->prepare_for_position();
            tab_in_range->keep_current_rowid = true;
          }
        }

        SJ_TMP_TABLE *sjtbl;
        if (jt_rowid_offset) /* Temptable has at least one rowid */
        {
          size_t tabs_size = (last_tab - sjtabs) * sizeof(SJ_TMP_TABLE::TAB);
          if (!(sjtbl = new (thd->mem_root) SJ_TMP_TABLE) ||
              !(sjtbl->tabs = (SJ_TMP_TABLE::TAB *)thd->alloc(tabs_size)))
            DBUG_RETURN(true); /* purecov: inspected */
          memcpy(sjtbl->tabs, sjtabs, tabs_size);
          sjtbl->is_confluent = false;
          sjtbl->tabs_end = sjtbl->tabs + (last_tab - sjtabs);
          sjtbl->rowid_len = jt_rowid_offset;
          sjtbl->null_bits = jt_null_bits;
          sjtbl->null_bytes = (jt_null_bits + 7) / 8;
          sjtbl->tmp_table = create_duplicate_weedout_tmp_table(
              thd, sjtbl->rowid_len + sjtbl->null_bytes, sjtbl);
          if (sjtbl->tmp_table == nullptr) DBUG_RETURN(true);
          if (sjtbl->tmp_table->hash_field)
            sjtbl->tmp_table->file->ha_index_init(0, 0);
          join->sj_tmp_tables.push_back(sjtbl->tmp_table);
        } else {
          /*
            This is confluent case where the entire subquery predicate does
            not depend on anything at all, ie this is
              WHERE const IN (uncorrelated select)
          */
          if (!(sjtbl = new (thd->mem_root) SJ_TMP_TABLE))
            DBUG_RETURN(true); /* purecov: inspected */
          sjtbl->tmp_table = NULL;
          sjtbl->is_confluent = true;
          sjtbl->have_confluent_row = false;
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
          If there are outer tables inbetween the inner tables, we have to
          setup a "split jump": Jump from the last inner table to the last
          outer table within the range, then from the last inner table
          before the outer table(s), jump to the last outer table before
          this range of inner tables, etc.
        */
        plan_idx jump_to = tab->idx() - 1;
        DBUG_ASSERT(tab_in_sj_nest);  // First table must be inner
        for (QEP_TAB *tab_in_range = tab; tab_in_range <= last_sj_tab;
             tab_in_range++) {
          if (!join->best_ref[tab_in_range->idx()]->emb_sj_nest) {
            /*
              Let last non-correlated table be jump target for
              subsequent inner tables.
            */
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
  DBUG_RETURN(false);
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
    table->file->ha_index_or_rnd_end();
    free_tmp_table(join->thd, table);
  }
  join->sj_tmp_tables.empty();
}

/**
  Remove all rows from all temp tables used by NL-semijoin runtime

  @param join  The join to remove tables for

  All rows must be removed from all temporary tables before every join
  re-execution.
*/

static int clear_sj_tmp_tables(JOIN *join) {
  int res;
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table = it++)) {
    if ((res = table->file->ha_delete_all_rows()))
      return res; /* purecov: inspected */
  }
  if (join->qep_tab) {
    Semijoin_mat_exec *sjm;
    List_iterator<Semijoin_mat_exec> it2(join->sjm_exec_list);
    while ((sjm = it2++)) {
      {
        QEP_TAB *const tab = &join->qep_tab[sjm->mat_table_index];
        /*
          If zero_result_cause is set, we have not gone through
          pick_table_access_method() which sets materialize_table, so the
          assertion is disabled in this case.
        */
        DBUG_ASSERT(join->zero_result_cause || tab->materialize_table);
        tab->table()->materialized = false;
      }
    }
  }
  return 0;
}

/**
  Reset the state of this join object so that it is ready for a
  new execution.
*/

void JOIN::reset() {
  DBUG_ENTER("JOIN::reset");

  if (!executed) DBUG_VOID_RETURN;

  unit->offset_limit_cnt = (ha_rows)(
      select_lex->offset_limit ? select_lex->offset_limit->val_uint() : 0ULL);

  first_record = false;
  group_sent = false;
  recursive_iteration_count = 0;
  executed = false;

  List_iterator<Window> li(select_lex->m_windows);
  Window *w;
  while ((w = li++)) {
    w->reset_round();
  }

  if (tmp_tables) {
    for (uint tmp = primary_tables; tmp < primary_tables + tmp_tables; tmp++) {
      TABLE *const tmp_table = qep_tab[tmp].table();
      if (!tmp_table->is_created()) continue;
      tmp_table->file->extra(HA_EXTRA_RESET_STATE);
      tmp_table->file->ha_delete_all_rows();
      free_io_cache(tmp_table);
      filesort_free_buffers(tmp_table, 0);
    }
  }
  clear_sj_tmp_tables(this);
  set_ref_item_slice(REF_SLICE_SAVE);

  /* need to reset ref access state (see join_read_key) */
  if (qep_tab) {
    for (uint i = 0; i < tables; i++) {
      QEP_TAB *const tab = &qep_tab[i];
      /*
        If qep_tab==NULL, we may still have done ref access (to read a const
        table); const tables will not be re-read in the next execution of this
        subquery, so resetting key_err is not needed.
      */
      tab->ref().key_err = true;
      /*
        If the finished execution used "filesort", it may have reset "quick"
        or "condition" when it didn't need them anymore. Restore them for the
        new execution (the new filesort will need them when it starts).
      */
      tab->restore_quick_optim_and_condition();
      tab->m_fetched_rows = 0;
    }
  }

  /* Reset of sum functions */
  if (sum_funcs) {
    Item_sum *func, **func_ptr = sum_funcs;
    while ((func = *(func_ptr++))) func->clear();
  }

  if (select_lex->has_ft_funcs()) {
    /* TODO: move the code to JOIN::exec */
    (void)init_ftfuncs(thd, select_lex);
  }

  DBUG_VOID_RETURN;
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
  DBUG_ENTER("JOIN::prepare_result");

  error = 0;
  // Create result tables for materialized views/derived tables
  if ((select_lex->materialized_derived_table_count > 0 ||
       select_lex->table_func_count > 0) &&
      !zero_result_cause) {
    for (TABLE_LIST *tl = select_lex->leaf_tables; tl; tl = tl->next_leaf) {
      if ((tl->is_view_or_derived() || tl->is_table_function()) &&
          tl->create_materialized_table(thd))
        goto err; /* purecov: inspected */
    }
  }

  if (select_lex->query_result()->start_execution()) goto err;

  if ((select_lex->active_options() & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(this, PROCESSED_BY_JOIN_EXEC))
    goto err;

  DBUG_RETURN(false);

err:
  error = 1;
  DBUG_RETURN(true);
}

/**
  Clean up and destroy join object.

  @return false if previous execution was successful, and true otherwise
*/

bool JOIN::destroy() {
  DBUG_ENTER("JOIN::destroy");

  cond_equal = 0;

  set_plan_state(NO_PLAN);

  if (qep_tab) {
    DBUG_ASSERT(!join_tab);
    for (uint i = 0; i < tables; i++) qep_tab[i].cleanup();
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

  /*
    We can't call delete_elements() on copy_funcs as this will cause
    problems in free_elements() as some of the elements are then deleted.
  */
  tmp_table_param.copy_funcs.empty();
  tmp_table_param.cleanup();

  /* Cleanup items referencing temporary table columns */
  if (tmp_all_fields != nullptr) {
    cleanup_item_list(tmp_all_fields[REF_SLICE_TMP1]);
    cleanup_item_list(tmp_all_fields[REF_SLICE_TMP2]);
    cleanup_item_list(tmp_all_fields[REF_SLICE_TMP3]);
    for (uint widx = 0; widx < m_windows.elements; widx++) {
      cleanup_item_list(tmp_all_fields[REF_SLICE_WIN_1 + widx]);
      cleanup_item_list(tmp_all_fields[REF_SLICE_WIN_1 + widx +
                                       m_windows.elements]);  // frame buffer
    }
  }
  destroy_sj_tmp_tables(this);

  List_iterator<Semijoin_mat_exec> sjm_list_it(sjm_exec_list);
  Semijoin_mat_exec *sjm;
  while ((sjm = sjm_list_it++)) ::destroy(sjm);
  sjm_exec_list.empty();

  keyuse_array.clear();
  DBUG_RETURN(error);
}

void JOIN::cleanup_item_list(List<Item> &items) const {
  if (!items.is_empty()) {
    List_iterator_fast<Item> it(items);
    Item *item;
    while ((item = it++)) item->cleanup();
  }
}

/**
  Optimize a query block and all inner query expressions

  @param thd    thread handler
  @returns false if success, true if error
*/

bool SELECT_LEX::optimize(THD *thd) {
  DBUG_ENTER("SELECT_LEX::optimize");

  DBUG_ASSERT(join == NULL);
  JOIN *const join_local = new (*THR_MALLOC) JOIN(thd, this);
  if (!join_local) DBUG_RETURN(true); /* purecov: inspected */

  set_join(join_local);

  if (join->optimize()) DBUG_RETURN(true);

  if (join->zero_result_cause && !is_implicitly_grouped()) DBUG_RETURN(false);

  for (SELECT_LEX_UNIT *unit = first_inner_unit(); unit;
       unit = unit->next_unit()) {
    // Derived tables and const subqueries are already optimized
    if (!unit->is_optimized() && unit->optimize(thd)) DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

/*****************************************************************************
  Go through all combinations of not marked tables and find the one
  which uses least records
*****************************************************************************/

/**
  Find how much space the prevous read not const tables takes in cache.
*/

void calc_used_field_length(TABLE *table, bool keep_current_rowid,
                            uint *p_used_fields, uint *p_used_fieldlength,
                            uint *p_used_blobs, bool *p_used_null_fields,
                            bool *p_used_uneven_bit_fields) {
  uint null_fields, blobs, fields, rec_length;
  Field **f_ptr, *field;
  uint uneven_bit_fields;
  MY_BITMAP *read_set = table->read_set;

  uneven_bit_fields = null_fields = blobs = fields = rec_length = 0;
  for (f_ptr = table->field; (field = *f_ptr); f_ptr++) {
    if (bitmap_is_set(read_set, field->field_index)) {
      uint flags = field->flags;
      fields++;
      rec_length += field->pack_length();
      if (flags & BLOB_FLAG) blobs++;
      if (!(flags & NOT_NULL_FLAG)) null_fields++;
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

  if (keep_current_rowid) {
    rec_length += table->file->ref_length;
    fields++;
  }

  *p_used_fields = fields;
  *p_used_fieldlength = rec_length;
  *p_used_blobs = blobs;
  *p_used_null_fields = null_fields;
  *p_used_uneven_bit_fields = uneven_bit_fields;
}

bool JOIN::init_ref_access() {
  DBUG_ENTER("JOIN::init_ref_access");
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);

  for (uint tableno = const_tables; tableno < tables; tableno++) {
    JOIN_TAB *const tab = best_ref[tableno];

    if (tab->type() == JT_REF)  // Here JT_REF means all kinds of ref access
    {
      DBUG_ASSERT(tab->position() && tab->position()->key);
      if (create_ref_for_key(this, tab, tab->position()->key,
                             tab->prefix_tables()))
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}

/**
  Set the first_sj_inner_tab and last_sj_inner_tab fields for all tables
  inside the semijoin nests of the query.
*/
void JOIN::set_semijoin_info() {
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);
  if (select_lex->sj_nests.is_empty()) return;

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
  DBUG_ASSERT(!dep_map || maybe_null);
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
      keypart, as TABLE_REF::null_ref_key is only one byte.
    */
    if (!(~used_tables & keyuse->used_tables) && keyparts == keyuse->keypart &&
        !(found_part_ref_or_null & keyuse->optimize)) {
      DBUG_ASSERT(keyparts <= MAX_REF_PARTS);
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
  DBUG_ASSERT(keyparts > 0);
  *length_out = length;
  *keyparts_out = keyparts;
}

/**
  Setup a ref access for looking up rows via an index (a key).

  @param join          The join object being handled
  @param j             The join_tab which will have the ref access populated
  @param org_keyuse  First key part of (possibly multi-part) key
  @param used_tables   Bitmap of available tables

  @return False if success, True if error

  Given a Key_use structure that specifies the fields that can be used
  for index access, this function creates and set up the structure
  used for index look up via one of the access methods {JT_FT,
  JT_CONST, JT_REF_OR_NULL, JT_REF, JT_EQ_REF} for the plan operator
  'j'. Generally the function sets up the structure j->ref (of type
  TABLE_REF), and the access method j->type.

  @note We cannot setup fields used for ref access before we have sorted
        the items within multiple equalities according to the final order of
        the tables involved in the join operation. Currently, this occurs in
        @see substitute_for_best_equal_field().
        The exception is ref access for const tables, which are fixed
        before the greedy search planner is invoked.
*/

bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
                        table_map used_tables) {
  DBUG_ENTER("create_ref_for_key");

  Key_use *keyuse = org_keyuse;
  const uint key = keyuse->key;
  const bool ftkey = (keyuse->keypart == FT_KEYPART);
  THD *const thd = join->thd;
  uint keyparts, length;
  TABLE *const table = j->table();
  KEY *const keyinfo = table->key_info + key;
  Key_use *chosen_keyuses[MAX_REF_PARTS];

  DBUG_ASSERT(j->keys().is_set(org_keyuse->key));

  /* Calculate the length of the used key. */
  if (ftkey) {
    Item_func_match *ifm = (Item_func_match *)keyuse->val;

    length = 0;
    keyparts = 1;
    ifm->get_master()->join_key = 1;
  } else /* not ftkey */
    calc_length_and_keyparts(keyuse, j, key, used_tables, chosen_keyuses,
                             &length, &keyparts, NULL, NULL);
  /* set up fieldref */
  j->ref().key_parts = keyparts;
  j->ref().key_length = length;
  j->ref().key = (int)key;
  if (!(j->ref().key_buff = (uchar *)thd->mem_calloc(ALIGN_SIZE(length) * 2)) ||
      !(j->ref().key_copy =
            (store_key **)thd->alloc((sizeof(store_key *) * (keyparts)))) ||
      !(j->ref().items = (Item **)thd->alloc(sizeof(Item *) * keyparts)) ||
      !(j->ref().cond_guards =
            (bool **)thd->alloc(sizeof(uint *) * keyparts))) {
    DBUG_RETURN(true);
  }
  j->ref().key_buff2 = j->ref().key_buff + ALIGN_SIZE(length);
  j->ref().key_err = 1;
  j->ref().null_rejecting = 0;
  j->ref().use_count = 0;
  j->ref().disable_cache = false;
  keyuse = org_keyuse;

  uchar *key_buff = j->ref().key_buff;
  uchar *null_ref_key = NULL;
  bool keyuse_uses_no_tables = true;
  if (ftkey) {
    j->ref().items[0] = ((Item_func *)(keyuse->val))->key_item();
    /* Predicates pushed down into subquery can't be used FT access */
    j->ref().cond_guards[0] = NULL;
    if (keyuse->used_tables) DBUG_RETURN(true);  // not supported yet. SerG

    j->set_type(JT_FT);
    j->set_ft_func((Item_func_match *)keyuse->val);
    memset(j->ref().key_copy, 0, sizeof(j->ref().key_copy[0]) * keyparts);
  } else {
    // Set up TABLE_REF based on chosen Key_use-s.
    for (uint part_no = 0; part_no < keyparts; part_no++) {
      keyuse = chosen_keyuses[part_no];
      bool maybe_null = keyinfo->key_part[part_no].null_bit;

      if (keyuse->val->type() == Item::FIELD_ITEM) {
        // Look up the most appropriate field to base the ref access on.
        keyuse->val = get_best_field(static_cast<Item_field *>(keyuse->val),
                                     join->cond_equal);
        keyuse->used_tables = keyuse->val->used_tables();
      }
      j->ref().items[part_no] = keyuse->val;  // Save for cond removal
      j->ref().cond_guards[part_no] = keyuse->cond_guard;
      if (keyuse->null_rejecting)
        j->ref().null_rejecting |= (key_part_map)1 << part_no;
      keyuse_uses_no_tables = keyuse_uses_no_tables && !keyuse->used_tables;

      store_key *s_key =
          get_store_key(thd, keyuse, join->const_table_map,
                        &keyinfo->key_part[part_no], key_buff, maybe_null);
      if (unlikely(!s_key || thd->is_fatal_error)) DBUG_RETURN(true);

      if (keyuse->used_tables) /* Comparing against a non-constant. */
        j->ref().key_copy[part_no] = s_key;
      else {
        /**
           The outer reference is to a const table, so we copy the value
           straight from that table now (during optimization), instead of from
           the temporary table created during execution.

           TODO: Synchronize with the temporary table creation code, so that
           there is no need to create a column for this value.
        */
        bool dummy_value = false;
        keyuse->val->walk(&Item::repoint_const_outer_ref, Item::WALK_PREFIX,
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
          j->ref().key_copy[part_no] = s_key;  // Reevaluate in JOIN::exec()
        else
          j->ref().key_copy[part_no] = NULL;
      }
      /*
        Remember if we are going to use REF_OR_NULL
        But only if field _really_ can be null i.e. we force JT_REF
        instead of JT_REF_OR_NULL in case if field can't be null
      */
      if ((keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL) && maybe_null) {
        DBUG_ASSERT(null_ref_key == NULL);  // or we would overwrite it below
        null_ref_key = key_buff;
      }
      key_buff += keyinfo->key_part[part_no].store_length;
    }
  } /* not ftkey */
  if (j->type() == JT_FT) DBUG_RETURN(false);
  if (j->type() == JT_CONST)
    j->table()->const_table = 1;
  else if (((actual_key_flags(keyinfo) & (HA_NOSAME | HA_NULL_PART_KEY)) !=
            HA_NOSAME) ||
           keyparts != actual_key_parts(keyinfo) || null_ref_key) {
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

  DBUG_RETURN(false);
}

static store_key *get_store_key(THD *thd, Key_use *keyuse,
                                table_map used_tables, KEY_PART_INFO *key_part,
                                uchar *key_buff, uint maybe_null) {
  if (!((~used_tables) & keyuse->used_tables))  // if const item
  {
    return new (*THR_MALLOC) store_key_const_item(
        thd, key_part->field, key_buff + maybe_null, maybe_null ? key_buff : 0,
        key_part->length, keyuse->val);
  }

  Item_field *field_item = NULL;
  if (keyuse->val->type() == Item::FIELD_ITEM)
    field_item = static_cast<Item_field *>(keyuse->val->real_item());
  else if (keyuse->val->type() == Item::REF_ITEM) {
    Item_ref *item_ref = static_cast<Item_ref *>(keyuse->val);
    if (item_ref->ref_type() == Item_ref::OUTER_REF) {
      if ((*item_ref->ref)->type() == Item::FIELD_ITEM)
        field_item = static_cast<Item_field *>(item_ref->real_item());
    }
  }
  if (field_item)
    return new (*THR_MALLOC) store_key_field(
        thd, key_part->field, key_buff + maybe_null, maybe_null ? key_buff : 0,
        key_part->length, field_item->field, keyuse->val->full_name());

  return new (*THR_MALLOC)
      store_key_item(thd, key_part->field, key_buff + maybe_null,
                     maybe_null ? key_buff : 0, key_part->length, keyuse->val);
}

enum store_key::store_key_result store_key_hash_item::copy_inner() {
  enum store_key_result res = store_key_item::copy_inner();
  if (res != STORE_KEY_FATAL) *hash = unique_hash(to_field, hash);
  return res;
}

/**
  Extend e1 by AND'ing e2 to the condition e1 points to. The resulting
  condition is fixed. Requirement: the input Items must already have
  been fixed.

  @param[in,out]   e1 Pointer to condition that will be extended with e2
  @param           e2 Condition that will extend e1

  @retval true   if there was a memory allocation error, in which case
                 e1 remains unchanged
  @retval false  otherwise
*/

bool and_conditions(Item **e1, Item *e2) {
  DBUG_ASSERT(!(*e1) || (*e1)->fixed);
  DBUG_ASSERT(!e2 || e2->fixed);
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
  DBUG_ASSERT(cond != NULL);

  if (cond->type() == Item::COND_ITEM) {
    uint n_marked = 0;
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      table_map used_tables = 0;
      Item_cond_and *new_cond = new Item_cond_and;
      if (!new_cond) return NULL;
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
          return NULL;
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
      if (!new_cond) return NULL;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_for_index(item, table, keyno, other_tbls_ok);
        if (!fix) return NULL;
        new_cond->argument_list()->push_back(fix);
        n_marked += (item->marker == Item::MARKER_ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked == ((Item_cond *)cond)->argument_list()->elements)
        cond->marker = Item::MARKER_ICP_COND_USES_INDEX_ONLY;
      new_cond->quick_fix_field();
      new_cond->set_used_tables(cond->used_tables());
      new_cond->top_level_item();
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
    return NULL;
  }
  cond->marker = Item::MARKER_ICP_COND_USES_INDEX_ONLY;
  return cond;
}

static Item *make_cond_remainder(Item *cond, bool exclude_index) {
  if (exclude_index && cond->marker == Item::MARKER_ICP_COND_USES_INDEX_ONLY)
    return 0; /* Already checked */

  if (cond->type() == Item::COND_ITEM) {
    table_map tbl_map = 0;
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      /* Create new top level AND item */
      Item_cond_and *new_cond = new Item_cond_and;
      if (!new_cond) return (Item *)0;
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
          return (Item *)0;
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
      if (!new_cond) return (Item *)0;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_remainder(item, false);
        if (!fix) return (Item *)0;
        new_cond->argument_list()->push_back(fix);
        tbl_map |= fix->used_tables();
      }
      new_cond->quick_fix_field();
      new_cond->set_used_tables(tbl_map);
      new_cond->top_level_item();
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
  DBUG_ENTER("push_index_cond");

  ASSERT_BEST_REF_IN_JOIN_ORDER(join_);
  DBUG_ASSERT(join_tab == join_->best_ref[idx()]);

  if (join_tab->reversed_access)  // @todo: historical limitation, lift it!
    DBUG_VOID_RETURN;

  TABLE *const tbl = table();

  // Disable ICP for Innodb intrinsic temp table because of performance
  if (internal_tmp_disk_storage_engine == TMP_TABLE_INNODB &&
      tbl->s->db_type() == innodb_hton && tbl->s->tmp_table != NO_TMP_TABLE &&
      tbl->s->tmp_table != TRANSACTIONAL_TMP_TABLE)
    DBUG_VOID_RETURN;

  // TODO: Currently, index on virtual generated column doesn't support ICP
  if (tbl->vfield && tbl->index_contains_some_virtual_gcol(keyno))
    DBUG_VOID_RETURN;

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
       @see subselect_single_select_engine::exec()
       @see TABLE_REF::cond_guards
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
      tbl->file->index_flags(keyno, 0, 1) & HA_DO_INDEX_COND_PUSHDOWN &&
      hint_key_state(join_->thd, table_ref, keyno, ICP_HINT_ENUM,
                     OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN) &&
      join_->thd->lex->sql_command != SQLCOM_UPDATE_MULTI &&
      join_->thd->lex->sql_command != SQLCOM_DELETE_MULTI &&
      !has_guarded_conds() && type() != JT_CONST && type() != JT_SYSTEM &&
      !(keyno == tbl->s->primary_key &&
        tbl->file->primary_key_is_clustered())) {
    DBUG_EXECUTE("where", print_where(condition(), "full cond", QT_ORDINARY););
    Item *idx_cond =
        make_cond_for_index(condition(), tbl, keyno, other_tbls_ok);
    DBUG_EXECUTE("where", print_where(idx_cond, "idx cond", QT_ORDINARY););
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
        DBUG_ASSERT(other_tbls_ok ||                              // 1
                    idx_cond->const_item() ||                     // 2
                    (idx_cond->used_tables() & RAND_TABLE_BIT));  // 3
        DBUG_VOID_RETURN;
      }

      Item *idx_remainder_cond = 0;

      /*
        For BKA cache we store condition to special BKA cache field
        because evaluation of the condition requires additional operations
        before the evaluation. This condition is used in
        JOIN_CACHE_BKA[_UNIQUE]::skip_index_tuple() functions.
      */
      if (join_tab->use_join_cache() &&
          /*
            if cache is used then the value is true only
            for BKA[_UNIQUE] cache (see setup_join_buffering() func).
            In this case other_tbls_ok is an equivalent of
            cache->is_key_access().
          */
          other_tbls_ok &&
          (idx_cond->used_tables() &
           ~(table_ref->map() | join_->const_table_map))) {
        cache_idx_cond = idx_cond;
        trace_obj->add("pushed_to_BKA", true);
      } else {
        idx_remainder_cond = tbl->file->idx_cond_push(keyno, idx_cond);
        DBUG_EXECUTE("where", print_where(tbl->file->pushed_idx_cond,
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
      DBUG_EXECUTE("where",
                   print_where(row_cond, "remainder cond", QT_ORDINARY););

      if (row_cond) {
        if (idx_remainder_cond) and_conditions(&row_cond, idx_remainder_cond);
        idx_remainder_cond = row_cond;
      }
      set_condition(idx_remainder_cond);
      trace_obj->add("table_condition_attached", idx_remainder_cond);
    }
  }
  DBUG_VOID_RETURN;
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
    - Create the materialization temporary table, including TABLE_LIST object.
    - Create a list of Item_field objects per column in the temporary table.
    - Create a keyuse array describing index lookups into the table
      (for MaterializeLookup)

  @return False if OK, True if error
*/

bool JOIN::setup_semijoin_materialized_table(JOIN_TAB *tab, uint tableno,
                                             const POSITION *inner_pos,
                                             POSITION *sjm_pos) {
  DBUG_ENTER("JOIN::setup_semijoin_materialized_table");
  const TABLE_LIST *const emb_sj_nest = inner_pos->table->emb_sj_nest;
  Semijoin_mat_optimize *const sjm_opt = &emb_sj_nest->nested_join->sjm;
  Semijoin_mat_exec *const sjm_exec = tab->sj_mat_exec();
  const uint field_count = emb_sj_nest->nested_join->sj_inner_exprs.elements;

  DBUG_ASSERT(inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP ||
              inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN);

  /*
    Set up the table to write to, do as
    Query_result_union::create_result_table does
  */
  sjm_exec->table_param = Temp_table_param();
  count_field_types(select_lex, &sjm_exec->table_param,
                    emb_sj_nest->nested_join->sj_inner_exprs, false, true);
  sjm_exec->table_param.bit_fields_as_long = true;

  char buffer[NAME_LEN];
  const size_t len = snprintf(buffer, sizeof(buffer) - 1, "<subquery%u>",
                              emb_sj_nest->nested_join->query_block_id);
  char *name = (char *)alloc_root(thd->mem_root, len + 1);
  if (name == NULL) DBUG_RETURN(true); /* purecov: inspected */

  memcpy(name, buffer, len);
  name[len] = '\0';
  TABLE *table;
  if (!(table =
            create_tmp_table(thd, &sjm_exec->table_param,
                             emb_sj_nest->nested_join->sj_inner_exprs, NULL,
                             true /* distinct */, true /* save_sum_fields */,
                             thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS,
                             HA_POS_ERROR /* rows_limit */, name)))
    DBUG_RETURN(true); /* purecov: inspected */
  sjm_exec->table = table;
  map2table[tableno] = tab;
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  sj_tmp_tables.push_back(table);
  sjm_exec_list.push_back(sjm_exec);

  /*
    Hash_field is not applicable for MATERIALIZE_LOOKUP. If hash_field is
    created for temporary table, semijoin_types_allow_materialization must
    assure that MATERIALIZE_LOOKUP can't be chosen.
  */
  DBUG_ASSERT((inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP &&
               !table->hash_field) ||
              inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN);

  TABLE_LIST *tl = new (thd->mem_root)
      TABLE_LIST(table, "", 0, name, strlen(name), name, TL_IGNORE);
  if (tl == nullptr) DBUG_RETURN(true); /* purecov: inspected */
  // TODO: May have to setup outer-join info for this TABLE_LIST !!!

  tl->set_tableno(tableno);

  table->pos_in_table_list = tl;

  if (!(sjm_opt->mat_fields = (Item_field **)alloc_root(
            thd->mem_root, field_count * sizeof(Item_field **))))
    DBUG_RETURN(true);

  for (uint fieldno = 0; fieldno < field_count; fieldno++) {
    if (!(sjm_opt->mat_fields[fieldno] =
              new Item_field(table->visible_field_ptr()[fieldno])))
      DBUG_RETURN(true);
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
  if (!keyuse) DBUG_RETURN(true);

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
    sjm_pos->key = NULL;  // No index use for MaterializeScan
    sjm_pos->read_cost = tab->read_time * fanout;
    sjm_pos->rows_fetched = static_cast<double>(tab->records());
    tab->set_type(JT_ALL);
  }
  sjm_pos->set_prefix_join_cost((tab - join_tab), cost_model());

  DBUG_RETURN(false);
}

/**
  A helper function that allocates appropriate join cache object and
  sets next_select function of previous tab.
*/

void QEP_TAB::init_join_cache(JOIN_TAB *join_tab) {
  JOIN *const join_ = join();
  DBUG_ASSERT(idx() > 0);
  ASSERT_BEST_REF_IN_JOIN_ORDER(join_);
  DBUG_ASSERT(join_tab == join_->best_ref[idx()]);

  JOIN_CACHE *prev_cache = NULL;
  if ((uint)idx() > join_->const_tables) {
    QEP_TAB *prev_tab = this - 1;
    /*
      Link with the previous join cache, but make sure that we do not link
      join caches of two different operations when the previous operation was
      MaterializeLookup or MaterializeScan, ie if:
       1. the previous join_tab has join buffering enabled, and
       2. the previous join_tab belongs to a materialized semi-join nest, and
       3. this join_tab represents a regular table, or is part of a different
          semi-join interval than the previous join_tab.
    */
    prev_cache = (JOIN_CACHE *)prev_tab->op;
    if (prev_cache != NULL &&                                       // 1
        sj_is_materialize_strategy(prev_tab->get_sj_strategy()) &&  // 2
        first_sj_inner() != prev_tab->first_sj_inner())             // 3
      prev_cache = NULL;
  }
  switch (join_tab->use_join_cache()) {
    case JOIN_CACHE::ALG_BNL:
      op = new (*THR_MALLOC) JOIN_CACHE_BNL(join_, this, prev_cache);
      break;
    case JOIN_CACHE::ALG_BKA:
      op = new (*THR_MALLOC)
          JOIN_CACHE_BKA(join_, this, join_tab->join_cache_flags, prev_cache);
      break;
    case JOIN_CACHE::ALG_BKA_UNIQUE:
      op = new (*THR_MALLOC) JOIN_CACHE_BKA_UNIQUE(
          join_, this, join_tab->join_cache_flags, prev_cache);
      break;
    default:
      DBUG_ASSERT(0);
  }
  DBUG_EXECUTE_IF("jb_alloc_with_prev_fail", if (prev_cache) {
    DBUG_SET("+d,jb_alloc_fail");
    DBUG_SET("-d,jb_alloc_with_prev_fail");
  });
  if (!op || op->init()) {
    /*
      OOM. If it's in creation of "op" it has thrown error.
      If it's in init() (allocation of the join buffer) it has not,
      and there's a chance to execute the query:
      we remove this join buffer, and all others (as there may be
      dependencies due to outer joins).
      @todo Consider sending a notification of this problem (a warning to the
      client, or to the error log).
    */
    for (uint i = join_->const_tables; i < join_->tables; i++) {
      QEP_TAB *const q = &join_->qep_tab[i];
      if (!q->position()) continue;
      JOIN_TAB *const t = join_->best_ref[i];
      if (t->use_join_cache() == JOIN_CACHE::ALG_NONE) continue;
      t->set_use_join_cache(JOIN_CACHE::ALG_NONE);
      /*
        Detach the join buffer from QEP_TAB so that EXPLAIN doesn't show
        'Using join buffer'. Destroy the join buffer.
      */
      if (q->op) {
        q->op->mem_free();
        destroy(q->op);
        q->op = NULL;
      }
      DBUG_ASSERT(i > 0);
      /*
        Make the immediately preceding QEP_TAB channel the work to the
        non-buffered nested loop algorithm:
      */
      q[-1].next_select = sub_select;
    }
  } else
    this[-1].next_select = sub_select_op;
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
                            join->select_distinct || join->group_list ||
                            join->order || join->m_windows.elements > 0;

  DBUG_ENTER("make_join_readinfo");
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  Opt_trace_context *const trace = &join->thd->opt_trace;
  Opt_trace_object wrapper(trace);
  Opt_trace_array trace_refine_plan(trace, "refine_plan");

  if (setup_semijoin_dups_elimination(join, no_jbuf_after))
    DBUG_RETURN(true); /* purecov: inspected */

  for (uint i = join->const_tables; i < join->tables; i++) {
    QEP_TAB *const qep_tab = &join->qep_tab[i];

    if (!qep_tab->position()) continue;

    JOIN_TAB *const tab = join->best_ref[i];
    TABLE *const table = qep_tab->table();
    /*
     Need to tell handlers that to play it safe, it should fetch all
     columns of the primary key of the tables: this is because MySQL may
     build row pointers for the rows, and for all columns of the primary key
     the read set has not necessarily been set by the server code.
    */
    if (prep_for_pos) table->prepare_for_position();

    qep_tab->read_record.table = table;
    qep_tab->next_select = sub_select; /* normal select */
    qep_tab->cache_idx_cond = NULL;

    DBUG_ASSERT(!qep_tab->read_first_record);
    qep_tab->read_record.read_record = NULL;
    qep_tab->read_record.unlock_row = rr_unlock_row;

    Opt_trace_object trace_refine_table(trace);
    trace_refine_table.add_utf8_table(qep_tab->table_ref);

    if (qep_tab->do_loosescan()) {
      if (!(qep_tab->loosescan_buf =
                (uchar *)join->thd->alloc(qep_tab->loosescan_key_len)))
        DBUG_RETURN(true); /* purecov: inspected */
    }

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
      /* Fall through */
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
          table->pos_in_table_list->fetch_number_of_rows();
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
        if (tab->use_quick == QS_DYNAMIC_RANGE) {
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
        if (statistics) {
          if (i == join->const_tables)
            join->thd->inc_status_select_range();
          else
            join->thd->inc_status_select_full_range_join();
        }
        if (!table->no_keyread && qep_tab->type() == JT_RANGE) {
          if (table->covering_keys.is_set(qep_tab->quick()->index)) {
            DBUG_ASSERT(qep_tab->quick()->index != MAX_KEY);
            table->set_keyread(true);
          }
          if (!table->key_read)
            qep_tab->push_index_cond(tab, qep_tab->quick()->index,
                                     &trace_refine_table);
        }
        if (tab->position()->filter_effect != COND_FILTER_STALE_NO_CONST) {
          double rows_w_const_cond = qep_tab->position()->rows_fetched;
          qep_tab->position()->rows_fetched =
              rows2double(tab->quick()->records);
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
        DBUG_ASSERT(0);
        break; /* purecov: deadcode */
    }

    if (tab->position()->filter_effect <= COND_FILTER_STALE) {
      /*
        Give a proper value for EXPLAIN.
        For performance reasons, we do not recalculate the filter for
        non-EXPLAIN queries; thus, EXPLAIN CONNECTION may show 100%
        for a query.
      */
      tab->position()->filter_effect =
          join->thd->lex->is_explain()
              ? calculate_condition_filter(
                    tab, (tab->ref().key != -1) ? tab->position()->key : NULL,
                    tab->prefix_tables() & ~tab->table_ref->map(),
                    tab->position()->rows_fetched, false, false,
                    trace_refine_table)
              : COND_FILTER_ALLPASS;
    }

    DBUG_ASSERT(!qep_tab->table_ref->is_recursive_reference() ||
                qep_tab->type() == JT_ALL);

    qep_tab->pick_table_access_method(tab);

    // Materialize derived tables prior to accessing them.
    if (tab->table_ref->is_table_function()) {
      qep_tab->materialize_table = join_materialize_table_function;
      if (tab->dependent) qep_tab->rematerialize = true;
    } else if (tab->table_ref->uses_materialization())
      qep_tab->materialize_table = join_materialize_derived;

    if (qep_tab->sj_mat_exec())
      qep_tab->materialize_table = join_materialize_semijoin;

    qep_tab->set_reversed_access(tab->reversed_access);
  }

  DBUG_RETURN(false);
}

/**
  Give error if we some tables are done with a full join.

  This is used by multi_table_update and multi_table_delete when running
  in safe mode.

  @param join		Join condition

  @retval
    0	ok
  @retval
    1	Error (full join used)
*/

bool error_if_full_join(JOIN *join) {
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);
  for (uint i = 0; i < join->primary_tables; i++) {
    JOIN_TAB *const tab = join->best_ref[i];

    if (tab->type() == JT_ALL && (!tab->quick())) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
      return true;
    }
  }
  return false;
}

/**
  Cleanup table of join operation.
*/

void JOIN_TAB::cleanup() {
  // Delete parts specific of JOIN_TAB:

  if (table()) table()->reginfo.join_tab = NULL;

  // Delete shared parts:
  if (join()->qep_tab) {
    // deletion will be done by QEP_TAB
  } else
    qs_cleanup();

  TRASH(this, sizeof(*this));
}

void QEP_TAB::cleanup() {
  // Delete parts specific of QEP_TAB:
  destroy(filesort);
  filesort = NULL;
  end_read_record(&read_record);
  if (quick_optim() != quick()) delete quick_optim();

  TABLE *const t = table();

  if (t) t->reginfo.qep_tab = NULL;

  // Delete shared parts:
  qs_cleanup();

  // Order of qs_cleanup() and this, matters:
  if (op) {
    if (op->type() == QEP_operation::OT_TMP_TABLE) {
      if (t)  // Check tmp table is not yet freed.
        free_tmp_table(current_thd, t);
      destroy(tmp_table_param);
      tmp_table_param = NULL;
    }
    op->mem_free();
  }

  TRASH(this, sizeof(*this));
}

void QEP_shared_owner::qs_cleanup() {
  /* Skip non-existing derived tables/views result tables */
  if (table() &&
      (table()->s->tmp_table != INTERNAL_TMP_TABLE || table()->is_created())) {
    table()->set_keyread(false);
    table()->file->ha_index_or_rnd_end();
    free_io_cache(table());
    filesort_free_buffers(table(), true);
    TABLE_LIST *const table_ref = table()->pos_in_table_list;
    if (table_ref) {
      table_ref->derived_keys_ready = false;
      table_ref->derived_key_list.empty();
    }
  }
  delete quick();
  TRASH(this, sizeof(*this));
}

uint QEP_TAB::sjm_query_block_id() const {
  DBUG_ASSERT(sj_is_materialize_strategy(get_sj_strategy()));
  for (uint i = 0; i < join()->primary_tables; ++i) {
    // Find the sj-mat tmp table whose sj nest contains us:
    Semijoin_mat_exec *const sjm = join()->qep_tab[i].sj_mat_exec();
    if (sjm && (uint)idx() >= sjm->inner_table_index &&
        (uint)idx() < sjm->inner_table_index + sjm->table_count)
      return sjm->sj_nest->nested_join->query_block_id;
  }
  DBUG_ASSERT(false);
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
    into the temprorary table.
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
  SELECT_LEX_UNIT *tmp_unit;
  SELECT_LEX *sl;
  /*
    Optimization: if not EXPLAIN and we are done with the JOIN,
    free all tables.
  */
  bool full = (!select_lex->uncacheable && !thd->lex->is_explain());
  bool can_unlock = full;
  DBUG_ENTER("JOIN::join_free");

  cleanup();

  for (tmp_unit = select_lex->first_inner_unit(); tmp_unit;
       tmp_unit = tmp_unit->next_unit())
    for (sl = tmp_unit->first_select(); sl; sl = sl->next_select()) {
      Item_subselect *subselect = sl->master_unit()->item;
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
      !(select_lex->active_options() & SELECT_NO_UNLOCK) &&
      !select_lex->subquery_in_having &&
      (select_lex == (thd->lex->unit->fake_select_lex
                          ? thd->lex->unit->fake_select_lex
                          : thd->lex->select_lex))) {
    /*
      TODO: unlock tables even if the join isn't top level select in the
      tree.
    */
    mysql_unlock_read_tables(thd, lock);  // Don't free join->lock
    DEBUG_SYNC(thd, "after_join_free_unlock");
    lock = 0;
  }

  DBUG_VOID_RETURN;
}

/**
  Free resources of given join.

  @note
    With subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/

void JOIN::cleanup() {
  DBUG_ENTER("JOIN::cleanup");

  DBUG_ASSERT(const_tables <= primary_tables && primary_tables <= tables);

  if (qep_tab || join_tab || best_ref) {
    for (uint i = 0; i < tables; i++) {
      QEP_TAB *qtab;
      TABLE *table;
      if (qep_tab) {
        DBUG_ASSERT(!join_tab);
        qtab = &qep_tab[i];
        table = qtab->table();
      } else {
        qtab = NULL;
        table = (join_tab ? &join_tab[i] : best_ref[i])->table();
      }
      if (!table) continue;
      if (table->is_created()) {
        table->file->ha_index_or_rnd_end();
      }
      free_io_cache(table);
      filesort_free_buffers(table, false);
    }
  }

  /* Restore ref array to original state */
  set_ref_item_slice(REF_SLICE_SAVE);

  DBUG_VOID_RETURN;
}

/**
  Filter out ORDER items those are equal to constants in WHERE

  This function is a limited version of remove_const() for use
  with non-JOIN statements (i.e. single-table UPDATE and DELETE).


  @param order            Linked list of ORDER BY arguments.
  @param where            Where condition.

  @return pointer to new filtered ORDER list or NULL if whole list eliminated

  @note
    This function overwrites input order list.
*/

ORDER *simple_remove_const(ORDER *order, Item *where) {
  if (!order || !where) return order;

  ORDER *first = NULL, *prev = NULL;
  for (; order; order = order->next) {
    DBUG_ASSERT(!order->item[0]->has_aggregation());  // should never happen
    if (!const_expression_in_where(where, order->item[0])) {
      if (!first) first = order;
      if (prev) prev->next = order;
      prev = order;
    }
  }
  if (prev) prev->next = NULL;
  return first;
}

/*
  Check if equality can be used in removing components of GROUP BY/DISTINCT

  SYNOPSIS
    test_if_equality_guarantees_uniqueness()
      l          the left comparison argument (a field if any)
      r          the right comparison argument (a const of any)

  DESCRIPTION
    Checks if an equality predicate can be used to take away
    DISTINCT/GROUP BY because it is known to be true for exactly one
    distinct value (e.g. <expr> == <const>).
    Arguments must be of the same type because e.g.
    <string_field> = <int_const> may match more than 1 distinct value from
    the column.
    We must take into consideration and the optimization done for various
    string constants when compared to dates etc (see Item_int_with_ref) as
    well as the collation of the arguments.

  RETURN VALUE
    true    can be used
    false   cannot be used
*/
static bool test_if_equality_guarantees_uniqueness(Item *l, Item *r) {
  return r->const_item() &&
         /* elements must be compared as dates */
         (Arg_comparator::can_compare_as_dates(l, r, 0) ||
          /* or of the same result type */
          (r->result_type() == l->result_type() &&
           /* and must have the same collation if compared as strings */
           (l->result_type() != STRING_RESULT ||
            l->collation.collation == r->collation.collation)));
}

/*
  Return true if i1 and i2 (if any) are equal items,
  or if i1 is a wrapper item around the f2 field.
*/

static bool equal(Item *i1, Item *i2, Field *f2) {
  DBUG_ASSERT((i2 == NULL) ^ (f2 == NULL));

  if (i2 != NULL)
    return i1->eq(i2, 1);
  else if (i1->type() == Item::FIELD_ITEM)
    return f2->eq(((Item_field *)i1)->field);
  else
    return false;
}

/**
  Test if a field or an item is equal to a constant value in WHERE

  @param        cond            WHERE clause expression
  @param        comp_item       Item to find in WHERE expression
                                (if comp_field != NULL)
  @param        comp_field      Field to find in WHERE expression
                                (if comp_item != NULL)
  @param[out]   const_item      intermediate arg, set to Item pointer to NULL

  @return true if the field is a constant value in WHERE

  @note
    comp_item and comp_field parameters are mutually exclusive.
*/
bool const_expression_in_where(Item *cond, Item *comp_item, Field *comp_field,
                               Item **const_item) {
  DBUG_ASSERT((comp_item == NULL) ^ (comp_field == NULL));

  Item *intermediate = NULL;
  if (const_item == NULL) const_item = &intermediate;

  if (cond->type() == Item::COND_ITEM) {
    bool and_level =
        (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC);
    List_iterator_fast<Item> li(*((Item_cond *)cond)->argument_list());
    Item *item;
    while ((item = li++)) {
      bool res =
          const_expression_in_where(item, comp_item, comp_field, const_item);
      if (res)  // Is a const value
      {
        if (and_level) return 1;
      } else if (!and_level)
        return 0;
    }
    return and_level ? 0 : 1;
  } else if (cond->eq_cmp_result() !=
             Item::COND_OK) {  // boolean compare function
    Item_func *func = (Item_func *)cond;
    if (func->functype() != Item_func::EQUAL_FUNC &&
        func->functype() != Item_func::EQ_FUNC)
      return 0;
    Item *left_item = ((Item_func *)cond)->arguments()[0];
    Item *right_item = ((Item_func *)cond)->arguments()[1];
    if (equal(left_item, comp_item, comp_field)) {
      if (test_if_equality_guarantees_uniqueness(left_item, right_item)) {
        if (*const_item) return right_item->eq(*const_item, 1);
        *const_item = right_item;
        return 1;
      }
    } else if (equal(right_item, comp_item, comp_field)) {
      if (test_if_equality_guarantees_uniqueness(right_item, left_item)) {
        if (*const_item) return left_item->eq(*const_item, 1);
        *const_item = left_item;
        return 1;
      }
    }
  }
  return 0;
}

/**
  Update TMP_TABLE_PARAM with count of the different type of fields.

  This function counts the number of fields, functions and sum
  functions (items with type SUM_FUNC_ITEM) for use by
  create_tmp_table() and stores it in the Temp_table_param object. It
  also resets and calculates the quick_group property, which may have
  to be reverted if this function is called after deciding to use
  ROLLUP (see JOIN::optimize_rollup()).

  @param select_lex           SELECT_LEX of query
  @param param                Description of temp table
  @param fields               List of fields to count
  @param reset_with_sum_func  Whether to reset with_sum_func of func items
  @param save_sum_fields      Count in the way create_tmp_table() expects when
                              given the same parameter.
*/

void count_field_types(SELECT_LEX *select_lex, Temp_table_param *param,
                       List<Item> &fields, bool reset_with_sum_func,
                       bool save_sum_fields) {
  DBUG_ENTER("count_field_types");
  List_iterator<Item> li(fields);
  Item *field;

  param->field_count = 0;
  param->sum_func_count = 0;
  param->func_count = 0;
  param->hidden_field_count = 0;
  param->outer_sum_func_count = 0;
  param->quick_group = 1;
  /*
    Loose index scan guarantees that all grouping is done and MIN/MAX
    functions are computed, so create_tmp_table() treats this as if
    save_sum_fields is set.
  */
  save_sum_fields |= param->precomputed_group_by;

  while ((field = li++)) {
    Item *real = field->real_item();
    Item::Type real_type = real->type();

    if (real_type == Item::FIELD_ITEM)
      param->field_count++;
    else if (real_type == Item::SUM_FUNC_ITEM && !real->m_is_window_function) {
      if (!field->const_item()) {
        Item_sum *sum_item = down_cast<Item_sum *>(field->real_item());
        if (sum_item->aggr_select == select_lex) {
          if (!sum_item->quick_group)
            param->quick_group = 0;  // UDF SUM function
          param->sum_func_count++;

          for (uint i = 0; i < sum_item->get_arg_count();
               i++) {  // Add one column per argument
            if (sum_item->get_arg(i)->real_item()->type() == Item::FIELD_ITEM)
              param->field_count++;
            else
              param->func_count++;
          }
        }
        param->func_count++;  // A group aggregate function is a function!
      } else if (save_sum_fields) {
        /*
          Count the way create_tmp_table() does if asked to preserve
          Item_sum_* functions in fields list.

          Item field is an Item_sum_* or a reference to such an
          item. We need to distinguish between these two cases since
          they are treated differently by create_tmp_table().
        */
        if (field->type() == Item::SUM_FUNC_ITEM)  // An Item_sum_*
          param->field_count++;
        else  // A reference to an Item_sum_*
        {
          param->func_count++;
          param->sum_func_count++;
        }
      }
    } else if (real_type == Item::SUM_FUNC_ITEM) {
      DBUG_ASSERT(real->m_is_window_function);
      param->func_count++;

      Item_sum *window_item = down_cast<Item_sum *>(real);
      for (uint i = 0; i < window_item->get_arg_count(); i++) {
        if (window_item->get_arg(i)->real_item()->type() == Item::FIELD_ITEM)
          param->field_count++;
        else
          param->func_count++;
      }
    } else {
      param->func_count++;
      if (reset_with_sum_func) field->reset_aggregation();
      if (field->has_aggregation()) param->outer_sum_func_count++;
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Return 1 if second is a subpart of first argument.

  If first parts has different direction, change it to second part
  (group is sorted like order)
*/

bool test_if_subpart(ORDER *a, ORDER *b) {
  for (; a && b; a = a->next, b = b->next) {
    if ((*a->item)->eq(*b->item, 1))
      a->direction = b->direction;
    else
      return 0;
  }
  return !b;
}

/**
  calc how big buffer we need for comparing group entries.
*/

void calc_group_buffer(JOIN *join, ORDER *group) {
  DBUG_ENTER("calc_group_buffer");
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
          /* This case should never be choosen */
          DBUG_ASSERT(0);
          my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      }
    }
    parts++;
    if (group_item->maybe_null) null_parts++;
  }
  join->tmp_table_param.group_length = key_length + null_parts;
  join->tmp_table_param.group_parts = parts;
  join->tmp_table_param.group_null_parts = null_parts;
  DBUG_VOID_RETURN;
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
  DBUG_ENTER("alloc_func_list");

  func_count = tmp_table_param.sum_func_count;
  /*
    If we are using rollup, we need a copy of the summary functions for
    each level
  */
  if (rollup.state != ROLLUP::STATE_NONE) func_count *= (send_group_parts + 1);

  group_parts = send_group_parts;
  /*
    If distinct, reserve memory for possible
    disctinct->group_by optimization
  */
  if (select_distinct) {
    group_parts += fields_list.elements;
    /*
      If the ORDER clause is specified then it's possible that
      it also will be optimized, so reserve space for it too
    */
    if (order) {
      ORDER *ord;
      for (ord = order; ord; ord = ord->next) group_parts++;
    }
  }

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs =
      (Item_sum **)thd->mem_calloc(sizeof(Item_sum **) * (func_count + 1) +
                                   sizeof(Item_sum ***) * (group_parts + 1));
  sum_funcs_end = (Item_sum ***)(sum_funcs + func_count + 1);
  DBUG_RETURN(sum_funcs == 0);
}

/**
  Initialize 'sum_funcs' array with all Item_sum objects.

  @param field_list        All items
  @param send_result_set_metadata       Items in select list
  @param before_group_by   Set to 1 if this is called before GROUP BY handling
  @param recompute         Set to true if sum_funcs must be recomputed

  @retval
    0  ok
  @retval
    1  error
*/

bool JOIN::make_sum_func_list(List<Item> &field_list,
                              List<Item> &send_result_set_metadata,
                              bool before_group_by, bool recompute) {
  List_iterator_fast<Item> it(field_list);
  Item_sum **func;
  Item *item;
  DBUG_ENTER("make_sum_func_list");

  if (*sum_funcs && !recompute)
    DBUG_RETURN(false); /* We have already initialized sum_funcs. */

  func = sum_funcs;
  while ((item = it++)) {
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
        down_cast<Item_sum *>(item)->aggr_select == select_lex) {
      DBUG_ASSERT(!item->m_is_window_function);
      *func++ = down_cast<Item_sum *>(item);
    }
  }
  if (before_group_by && rollup.state == ROLLUP::STATE_INITED) {
    rollup.state = ROLLUP::STATE_READY;
    if (rollup_make_fields(field_list, send_result_set_metadata, &func))
      DBUG_RETURN(true);  // Should never happen
  } else if (rollup.state == ROLLUP::STATE_NONE) {
    for (uint i = 0; i <= send_group_parts; i++) sum_funcs_end[i] = func;
  } else if (rollup.state == ROLLUP::STATE_READY)
    DBUG_RETURN(false);  // Don't put end marker
  *func = 0;             // End marker
  DBUG_RETURN(false);
}

/**
  Free joins of subselect of this select.

  @param select   pointer to SELECT_LEX which subselects joins we will free

  @todo when the final use of this function (from SET statements) is removed,
  this function can be deleted.
*/

void free_underlaid_joins(SELECT_LEX *select) {
  for (SELECT_LEX_UNIT *unit = select->first_inner_unit(); unit;
       unit = unit->next_unit())
    unit->cleanup(false);
}

/****************************************************************************
  ROLLUP handling
****************************************************************************/

/**
   Wrap all constant Items in GROUP BY list.

   For ROLLUP queries each constant item referenced in GROUP BY list
   is wrapped up into an Item_func object yielding the same value
   as the constant item. The objects of the wrapper class are never
   considered as constant items and besides they inherit all
   properties of the Item_result_field class.
   This wrapping allows us to ensure writing constant items
   into temporary tables whenever the result of the ROLLUP
   operation has to be written into a temporary table, e.g. when
   ROLLUP is used together with DISTINCT in the SELECT list.
   Usually when creating temporary tables for a intermidiate
   result we do not include fields for constant expressions.

   @retval
     0  if ok
   @retval
     1  on error
*/

bool JOIN::rollup_process_const_fields() {
  ORDER *group_tmp;
  Item *item;
  List_iterator<Item> it(all_fields);

  for (group_tmp = group_list; group_tmp; group_tmp = group_tmp->next) {
    if (!(*group_tmp->item)->const_item()) continue;
    while ((item = it++)) {
      if (*group_tmp->item == item) {
        Item *new_item = new Item_func_rollup_const(item);
        if (!new_item) return 1;
        new_item->fix_fields(thd, (Item **)0);
        thd->change_item_tree(it.ref(), new_item);
        for (ORDER *tmp = group_tmp; tmp; tmp = tmp->next) {
          if (*tmp->item == item) thd->change_item_tree(tmp->item, new_item);
        }
        break;
      }
    }
    it.rewind();
  }
  return 0;
}

/**
  Fill up rollup structures with pointers to fields to use.

  Creates copies of item_sum items for each sum level.

  @param fields_arg		List of all fields (hidden and real ones)
  @param sel_fields		Pointer to selected fields
  @param func			Store here a pointer to all fields

  @retval
    0	if ok;
    In this case func is pointing to next not used element.
  @retval
    1    on error
*/

bool JOIN::rollup_make_fields(List<Item> &fields_arg, List<Item> &sel_fields,
                              Item_sum ***func) {
  List_iterator_fast<Item> it(fields_arg);
  Item *first_field = sel_fields.head();
  uint level;

  /*
    Create field lists for the different levels

    The idea here is to have a separate field list for each rollup level to
    avoid all runtime checks of which columns should be NULL.

    The list is stored in reverse order to get sum function in such an order
    in func that it makes it easy to reset them with init_sum_functions()

    Assuming:  SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP

    rollup.fields[0] will contain list where a,b,c is NULL
    rollup.fields[1] will contain list where b,c is NULL
    ...
    rollup.ref_item_array[#] points to fields for rollup.fields[#]
    ...
    sum_funcs_end[0] points to all sum functions
    sum_funcs_end[1] points to all sum functions, except grand totals
    ...
  */

  for (level = 0; level < send_group_parts; level++) {
    uint i;
    uint pos = send_group_parts - level - 1;
    bool real_fields = 0;
    Item *item;
    List_iterator<Item> new_it_fields_list(rollup.fields_list[pos]);
    List_iterator<Item> new_it_all_fields(rollup.all_fields[pos]);
    Ref_item_array ref_array_start = rollup.ref_item_arrays[pos];
    ORDER *start_group;

    /* Point to first hidden field */
    uint ref_array_ix = fields_arg.elements - 1;

    /* Remember where the sum functions ends for the previous level */
    sum_funcs_end[pos + 1] = *func;

    /* Find the start of the group for this level */
    for (i = 0, start_group = group_list; i++ < pos;
         start_group = start_group->next)
      ;

    it.rewind();
    while ((item = it++)) {
      if (item == first_field) {
        real_fields = 1;  // End of hidden fields
        ref_array_ix = 0;
      }

      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
          down_cast<Item_sum *>(item)->aggr_select == select_lex) {
        DBUG_ASSERT(!item->m_is_window_function);
        /*
          This is a top level summary function that must be replaced with
          a sum function that is reset for this level.

          NOTE: This code creates an object which is not that nice in a
          sub select.  Fortunately it's not common to have rollup in
          sub selects.
        */
        item = item->copy_or_same(thd);
        ((Item_sum *)item)->make_unique();
        *(*func) = (Item_sum *)item;
        (*func)++;
      } else {
        /* Check if this is something that is part of this group by */
        ORDER *group_tmp;
        for (group_tmp = start_group, i = pos; group_tmp;
             group_tmp = group_tmp->next, i++) {
          /*
            Query
              SELECT SUM(k) OVER() FROM t GROUP BY k WITH ROLLUP
            will add 'k' to select list twice, first one from GROUP BY, 2nd
            from SUM(). ROLLUP code should find and set both NULL in order
            to get correct result.
          */
          if (item == *group_tmp->item || item->eq(*group_tmp->item, false)) {
            /*
              This is an element that is used by the GROUP BY and should be
              set to NULL in this level
            */
            Item_null_result *null_item = new (thd->mem_root)
                Item_null_result(item->data_type(), item->result_type());
            if (!null_item) return 1;
            item->maybe_null = 1;  // Value will be null sometimes
            null_item->result_field = item->get_tmp_table_field();
            item = null_item;
            break;
          }
        }
      }
      ref_array_start[ref_array_ix] = item;
      if (real_fields) {
        (void)new_it_fields_list++;  // Point to next item
        (void)new_it_all_fields++;
        new_it_fields_list.replace(item);  // Replace previous
        new_it_all_fields.replace(item);
        ref_array_ix++;
      } else {
        rollup.all_fields[pos].replace(ref_array_ix, item);
        ref_array_ix--;
      }
    }
  }
  sum_funcs_end[0] = *func;  // Point to last function
  return 0;
}

/**
  Switch the ref item slice for rollup structures which need to use
  fields from the first temp table to evaluate functions and
  having_condition correctly.
  ROLLUP has a ref_item_slice which is pointing to the output
  of join operation. Super aggregates are calculated with the regular
  aggregations using the join output.
  In rollup_make_fields, we create a ref_item_array where
  1. Fields which are part of group by are replaced with Item_null_result
  objects.
  2. New aggregation functions to calculate super aggregates are added to
  the list of sum_funcs.
  3. The remaining objects point to join output.

  For operations like order by, distinct, windowing functions
  that are done post rollup, output of rollup data needs to be written
  into a temp table. For evaluation of having conditions and functions,
  we need the objects which are not dependent on ROLLUP NULL's to
  point to temp table fields. So we switch the ref_array pointers
  to refer to REF_ITEM_SLICE_1 (contents of first temp table).

  @param fields_arg		List of all fields (hidden and real ones)
  @param sel_fields		Pointer to selected fields
  @param func			Store here a pointer to all fields

  @retval
    0	if ok;
    In this case func is pointing to next not used element.
  @retval
    1    on error
*/

bool JOIN::switch_slice_for_rollup_fields(List<Item> &curr_all_fields,
                                          List<Item> &curr_sel_fields) {
  List_iterator_fast<Item> it(curr_all_fields);
  Item *first_field = curr_sel_fields.head();
  uint level;

  for (level = 0; level < send_group_parts; level++) {
    uint pos = send_group_parts - level - 1;
    bool real_fields = false;
    Item *item;
    List_iterator<Item> new_it_fields_list(rollup.fields_list[pos]);
    List_iterator<Item> new_it_all_fields(rollup.all_fields[pos]);
    Ref_item_array ref_array_start = rollup.ref_item_arrays[pos];

    /* Point to first hidden field */
    uint ref_array_ix = curr_all_fields.elements - 1;

    it.rewind();
    while ((item = it++)) {
      bool has_rollup_fields = false;
      if (item == first_field) {
        real_fields = true;  // End of hidden fields
        ref_array_ix = 0;
      }
      /*
        Check if the existing ref_array_item is a group by field or
        a function which uses group by fields or an aggregation function.
        We do not replace these items with the items in temp table as they
        need the ROLLUP NULL's.
      */
      Item *ref_array_item = ref_array_start[ref_array_ix];
      if (ref_array_item->type() == Item::NULL_RESULT_ITEM ||
          ref_array_item->has_rollup_field() ||
          (ref_array_item->type() == Item::SUM_FUNC_ITEM &&
           !ref_array_item->m_is_window_function)) {
        has_rollup_fields = true;
      }
      if (real_fields) {
        (void)new_it_fields_list++;  // Point to next item
        (void)new_it_all_fields++;
        // Replace all the items which do not need ROLLUP nulls for evaluation
        if (!has_rollup_fields) {
          ref_array_start[ref_array_ix] = item;
          new_it_fields_list.replace(item);  // Replace previous
          new_it_all_fields.replace(item);
        }
        ref_array_ix++;
      } else {
        // Replace all the items which do not need ROLLUP nulls for evaluation
        if (!has_rollup_fields) {
          ref_array_start[ref_array_ix] = item;
          rollup.all_fields[pos].replace(ref_array_ix, item);
        }
        ref_array_ix--;
      }
    }
  }
  return 0;
}

/**
  Change the Query_result object of the query block.

  If old_result is not used, forward the call to the current
  Query_result in case it is a wrapper around old_result.

  Call prepare() on the new Query_result if we decide to use it.

  @param new_result New Query_result object
  @param old_result Old Query_result object (NULL to force change)

  @retval false Success
  @retval true  Error
*/

bool SELECT_LEX::change_query_result(Query_result_interceptor *new_result,
                                     Query_result_interceptor *old_result) {
  DBUG_ENTER("SELECT_LEX::change_query_result");
  if (old_result == NULL || query_result() == old_result) {
    set_query_result(new_result);
    if (query_result()->prepare(fields_list, master_unit()))
      DBUG_RETURN(true); /* purecov: inspected */
    DBUG_RETURN(false);
  } else {
    const bool ret = query_result()->change_query_result(new_result);
    DBUG_RETURN(ret);
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

  DBUG_ENTER("JOIN::add_having_as_tmp_table_cond");

  if (curr_table->table_ref)
    used_tables = curr_table->table_ref->map();
  else {
    /*
      Pushing parts of HAVING to an internal temporary table.
      Fields in HAVING condition may have been replaced with fields in an
      internal temporary table. This table has map=1, hence we check that
      we have no fields from other tables (outer references are fine).
      Unfortunaly, update_used_tables() is not reliable for subquery
      items, which could thus still have other tables in their
      used_tables() information.
    */
    DBUG_ASSERT(having_cond->has_subquery() ||
                !(having_cond->used_tables() & ~(1 | PSEUDO_TABLE_BITS)));
    used_tables = 1;
  }

  /*
    All conditions which can be applied after reading from used_tables are
    added as filter conditions of curr_tmp_table. If condition's used_tables is
    not read yet for example subquery in having, then it will be kept as it is
    in original having_cond of join.
    If ROLLUP, having condition needs to be tested after writing rollup data.
    So do not move the having condition.
  */
  Item *sort_table_cond =
      (rollup.state == ROLLUP::STATE_NONE)
          ? make_cond_for_table(thd, having_cond, used_tables, (table_map)0,
                                false)
          : nullptr;
  if (sort_table_cond) {
    if (!curr_table->condition())
      curr_table->set_condition(sort_table_cond);
    else {
      curr_table->set_condition(
          new Item_cond_and(curr_table->condition(), sort_table_cond));
      if (curr_table->condition()->fix_fields(thd, 0)) DBUG_RETURN(true);
    }
    curr_table->condition()->top_level_item();
    DBUG_EXECUTE("where", print_where(curr_table->condition(),
                                      "select and having", QT_ORDINARY););

    having_cond = make_cond_for_table(thd, having_cond, ~(table_map)0,
                                      ~used_tables, false);
    DBUG_EXECUTE("where",
                 print_where(having_cond, "having after sort", QT_ORDINARY););

    Opt_trace_object trace_wrapper(trace);
    Opt_trace_object(trace, "sort_using_internal_table")
        .add("condition_for_sort", sort_table_cond)
        .add("having_after_sort", having_cond);
  }

  DBUG_RETURN(false);
}

/**
  todo: remove this explanation after reviewers have seen it.
  More info about the problem that required this function:
  select from (select WF1 over w1, WF2 over w2) dt;
  where "dt" is materialized. First the "dt" table structure is created with
  create_tmp_table() and that sets WF{1,2}->result_field (pointing into
  columns of "dt"). Then the inner subquery is optimized, that calls
  create_tmp_table() for the two windows. First for w1: WF1 is to be
  calculated in w1 so a column is added for its result in the tmp table; so
  its result_field gets re-set to point there, all fine. Continuing with the
  creation of wf1, WF2 is skipped. Then change_to_use_tmp_fields() sees that
  WF1 and WF2 have a result_field (see test
  'else if ((field= item->get_tmp_table_field()))'), so concludes that the ref
  slice used to read the tmp table of w1 should contain Item_fields for WF1
  and WF2; that's incorrect for WF2, and leads to WF2 never being calculated.
  My fix: in create_tmp_table(), when the destination table is to materialize
  a derived table / UNION (i.e. is not a group-by/windowing table), there's no
  reason to set result_field (results are not saved by this means anyway, but
  by Query_result_union::send_data() which reads the last table of the query
  and writes that to the materialized table), so don't set it.

  In subqueries, this state may not be clean before we start creating tmp files
  for windowing passes. This is needed because only the first window gets its
  result field set in the first tmp file pass, other result fields should be
  empty until the corresponding windowing tmp file pass happens, and an outer
  query breaks this presumption unless we reset.
  For example this query would have already set the result fields before we
  set up the windowing:

  SELECT * FROM (SELECT COUNT(*) OVER (), SUM(c) OVER () AS sum1, a FROM t) AS
  alias;

  causing the second windowing to fail, i.e. SUM.
*/
// static void reset_wf_result_fields(List<Item> *curr_all_fields)

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
  DBUG_ASSERT(!join_tab);
  List<Item> *curr_all_fields = &all_fields;
  List<Item> *curr_fields_list = &fields_list;
  bool materialize_join = false;
  uint curr_tmp_table = const_tables;
  TABLE *exec_tmp_table = NULL;
  /*
    If the plan is constant, we will not do window tmp table processing
    cf. special code path in do_select.
  */
  m_windowing_steps = m_windows.elements > 0 && !plan_is_const() &&
                      !implicit_grouping && !group_optimized_away;
  const bool may_trace =  // just to avoid an empty trace block
      need_tmp_before_win || implicit_grouping || m_windowing_steps ||
      group_list || order;

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_disable_I_S trace_disabled(trace, !may_trace);
  Opt_trace_object wrapper(trace);
  Opt_trace_array trace_tmp(trace, "considering_tmp_tables");

  DBUG_ENTER("JOIN::make_tmp_tables_info");

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
  if (qep_tab && qep_tab[0].quick() &&
      qep_tab[0].quick()->is_loose_index_scan())
    tmp_table_param.precomputed_group_by =
        !qep_tab[0].quick()->is_agg_loose_index_scan();

  uint last_slice_before_windowing = REF_SLICE_BASE;

  /*
    Create the first temporary table if distinct elimination is requested or
    if the sort is too complicated to be evaluated as a filesort.
  */
  if (need_tmp_before_win) {
    curr_tmp_table = primary_tables;
    Opt_trace_object trace_this_tbl(trace);
    trace_this_tbl.add("adding_tmp_table_in_plan_at_position", curr_tmp_table);
    tmp_tables++;
    if (plan_is_const()) first_select = sub_select_op;

    /*
      Make a copy of the base slice in the save slice.
      This is needed because later steps will overwrite the base slice with
      another slice (1-3).
      After this slice has been used, overwrite the base slice again with
      the copy in the save slice.
    */
    if (alloc_ref_item_slice(thd, REF_SLICE_SAVE)) DBUG_RETURN(true);

    copy_ref_item_slice(REF_SLICE_SAVE, REF_SLICE_BASE);
    current_ref_item_slice = REF_SLICE_SAVE;

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

    tmp_table_param.hidden_field_count =
        all_fields.elements - fields_list.elements;

    if (create_intermediate_table(&qep_tab[curr_tmp_table], &all_fields,
                                  tmp_group, group_list && simple_group))
      DBUG_RETURN(true);
    exec_tmp_table = qep_tab[curr_tmp_table].table();

    if (exec_tmp_table->is_distinct) optimize_distinct();

    /*
      If there is no sorting or grouping, 'use_order'
      index result should not have been requested.
      Exception: LooseScan strategy for semijoin requires
      sorted access even if final result is not to be sorted.
    */
    DBUG_ASSERT(
        !(m_ordered_index_usage == ORDERED_INDEX_VOID && !plan_is_const() &&
          qep_tab[const_tables].position()->sj_strategy != SJ_OPT_LOOSE_SCAN &&
          qep_tab[const_tables].use_order()));

    /*
      Allocate a slice of ref items that describe the items to be copied
      from the first temporary table.
    */
    if (alloc_ref_item_slice(thd, REF_SLICE_TMP1)) DBUG_RETURN(true);

    // Change sum_fields reference to calculated fields in tmp_table
    if (sort_and_group || qep_tab[curr_tmp_table].table()->group ||
        tmp_table_param.precomputed_group_by) {
      if (change_to_use_tmp_fields(
              thd, ref_items[REF_SLICE_TMP1], tmp_fields_list[REF_SLICE_TMP1],
              tmp_all_fields[REF_SLICE_TMP1], fields_list.elements, all_fields))
        DBUG_RETURN(true);
    } else {
      if (change_refs_to_tmp_fields(
              thd, ref_items[REF_SLICE_TMP1], tmp_fields_list[REF_SLICE_TMP1],
              tmp_all_fields[REF_SLICE_TMP1], fields_list.elements, all_fields))
        DBUG_RETURN(true);
    }
    curr_all_fields = &tmp_all_fields[REF_SLICE_TMP1];
    curr_fields_list = &tmp_fields_list[REF_SLICE_TMP1];
    // Need to set them now for correct group_fields setup, reset at the end.
    set_ref_item_slice(REF_SLICE_TMP1);
    qep_tab[curr_tmp_table].ref_item_slice = REF_SLICE_TMP1;
    setup_tmptable_write_func(&qep_tab[curr_tmp_table], &trace_this_tbl);
    last_slice_before_windowing = REF_SLICE_TMP1;

    if (rollup.state == ROLLUP::STATE_READY)
      switch_slice_for_rollup_fields(*curr_all_fields, *curr_fields_list);

    /*
      If having is not handled here, it will be checked before the row is sent
      to the client.
    */
    if (having_cond &&
        (sort_and_group || (exec_tmp_table->is_distinct && !group_list))) {
      /*
        If there is no select distinct or rollup, then move the having to table
        conds of tmp table.
        NOTE : We cannot apply having after distinct. If columns of having are
               not part of select distinct, then distinct may remove rows
               which can satisfy having.

        As this condition will read the tmp table, it is appropriate that
        REF_SLICE_TMP1 is in effect when we create it below.
      */
      if ((!select_distinct && rollup.state == ROLLUP::STATE_NONE) &&
          add_having_as_tmp_table_cond(curr_tmp_table))
        DBUG_RETURN(true);

      /*
        Having condition which we are not able to add as tmp table conds are
        kept as before. And, this will be applied before storing the rows in
        tmp table.
      */
      qep_tab[curr_tmp_table].having = having_cond;
      having_cond = NULL;  // Already done
    }

    tmp_table_param.func_count = 0;

    if (sort_and_group || qep_tab[curr_tmp_table].table()->group) {
      tmp_table_param.field_count += tmp_table_param.sum_func_count;
      tmp_table_param.sum_func_count = 0;
    }

    if (exec_tmp_table->group) {  // Already grouped
      if (!order && !no_order && !skip_sort_order) {
        if (!group_list.can_ignore_order())
          order = group_list; /* order by group */
        else {
          /*
            Check whether an order was explicitly specified on a GROUP BY
            column. If so, we have to use filesort.
          */
          for (ORDER *ord = group_list; ord; ord = ord->next)
            if (ord->is_explicit) {
              order = group_list; /* order by group */
              break;
            }
        }
      }
      group_list = NULL;
    }
    /*
      If we have different sort & group then we must sort the data by group
      and copy it to a second temporary table.
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)) or when distinct is used with rollup.
    */

    if ((group_list &&
         (!test_if_subpart(group_list, order) || select_distinct ||
          m_windowing_steps || rollup.state != ROLLUP::STATE_NONE)) ||
        (select_distinct && (tmp_table_param.using_outer_summary_function ||
                             rollup.state != ROLLUP::STATE_NONE))) {
      DBUG_PRINT("info", ("Creating group table"));

      calc_group_buffer(this, group_list);
      count_field_types(select_lex, &tmp_table_param,
                        tmp_all_fields[REF_SLICE_TMP1],
                        select_distinct && !group_list, false);
      tmp_table_param.hidden_field_count =
          tmp_all_fields[REF_SLICE_TMP1].elements -
          tmp_fields_list[REF_SLICE_TMP1].elements;
      sort_and_group = false;
      if (!exec_tmp_table->group && !exec_tmp_table->is_distinct) {
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
      if (qep_tab[0].quick() && qep_tab[0].quick()->is_loose_index_scan())
        tmp_table_param.precomputed_group_by = true;

      ORDER_with_src dummy = NULL;  // TODO can use table->group here also

      if (create_intermediate_table(&qep_tab[curr_tmp_table], curr_all_fields,
                                    dummy, true))
        DBUG_RETURN(true);

      if (group_list) {
        explain_flags.set(group_list.src, ESP_USING_TMPTABLE);
        if (!plan_is_const())  // No need to sort a single row
        {
          if (add_sorting_to_table(curr_tmp_table - 1, &group_list))
            DBUG_RETURN(true);
        }

        if (make_group_fields(this, this)) DBUG_RETURN(true);
      }

      // Setup sum funcs only when necessary, otherwise we might break info
      // for the first table
      if (group_list || tmp_table_param.sum_func_count) {
        if (make_sum_func_list(*curr_all_fields, *curr_fields_list, true, true))
          DBUG_RETURN(true);
        const bool need_distinct =
            !(qep_tab[0].quick() &&
              qep_tab[0].quick()->is_agg_loose_index_scan());
        if (prepare_sum_aggregators(sum_funcs, need_distinct))
          DBUG_RETURN(true);
        group_list = NULL;
        if (setup_sum_funcs(thd, sum_funcs)) DBUG_RETURN(true);
      }

      /*
        Allocate a slice of ref items that describe the items to be copied
        from the second temporary table.
      */
      if (alloc_ref_item_slice(thd, REF_SLICE_TMP2)) DBUG_RETURN(true);

      // No sum funcs anymore
      if (change_to_use_tmp_fields(
              thd, ref_items[REF_SLICE_TMP2], tmp_fields_list[REF_SLICE_TMP2],
              tmp_all_fields[REF_SLICE_TMP2], fields_list.elements,
              tmp_all_fields[REF_SLICE_TMP1]))
        DBUG_RETURN(true);

      curr_fields_list = &tmp_fields_list[REF_SLICE_TMP2];
      curr_all_fields = &tmp_all_fields[REF_SLICE_TMP2];
      set_ref_item_slice(REF_SLICE_TMP2);
      qep_tab[curr_tmp_table].ref_item_slice = REF_SLICE_TMP2;
      setup_tmptable_write_func(&qep_tab[curr_tmp_table], &trace_this_tbl);
      last_slice_before_windowing = REF_SLICE_TMP2;
    }
    if (qep_tab[curr_tmp_table].table()->is_distinct)
      select_distinct = false; /* Each row is unique */

    if (select_distinct && !group_list && !m_windowing_steps) {
      if (having_cond) {
        qep_tab[curr_tmp_table].having = having_cond;
        having_cond->update_used_tables();
        having_cond = NULL;
      }
      qep_tab[curr_tmp_table].needs_duplicate_removal = true;
      trace_this_tbl.add("reading_from_table_eliminates_duplicates", true);
      explain_flags.set(ESC_DISTINCT, ESP_DUPS_REMOVAL);
      select_distinct = false;
    }
    /* Clean tmp_table_param for the next tmp table. */
    tmp_table_param.field_count = tmp_table_param.sum_func_count =
        tmp_table_param.func_count = 0;

    tmp_table_param.copy_field = tmp_table_param.copy_field_end = 0;
    first_record = sort_and_group = 0;

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
      DBUG_ASSERT(select_lex->active_options() & OPTION_BUFFER_RESULT ||
                  m_windowing_steps);
      // the temporary table does not have a grouping expression
      DBUG_ASSERT(!qep_tab[curr_tmp_table].table()->group);
    }
    calc_group_buffer(this, group_list);
    count_field_types(select_lex, &tmp_table_param, *curr_all_fields, false,
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
    if (make_group_fields(this, this)) DBUG_RETURN(true);

    // "save" slice of ref_items array is needed due to overwriting strategy.
    if (ref_items[REF_SLICE_SAVE].is_null()) {
      if (alloc_ref_item_slice(thd, REF_SLICE_SAVE)) DBUG_RETURN(true);

      copy_ref_item_slice(REF_SLICE_SAVE, REF_SLICE_BASE);
      current_ref_item_slice = REF_SLICE_SAVE;
    }

    /*
      Allocate a slice of ref items that describe the items to be copied
      from the record buffer for this temporary table.
    */
    if (alloc_ref_item_slice(thd, REF_SLICE_TMP3)) DBUG_RETURN(true);
    setup_copy_fields(thd, &tmp_table_param, ref_items[REF_SLICE_TMP3],
                      tmp_fields_list[REF_SLICE_TMP3],
                      tmp_all_fields[REF_SLICE_TMP3],
                      curr_fields_list->elements, *curr_all_fields);

    curr_fields_list = &tmp_fields_list[REF_SLICE_TMP3];
    curr_all_fields = &tmp_all_fields[REF_SLICE_TMP3];
    last_slice_before_windowing = REF_SLICE_TMP3;

    if (qep_tab)  // remember when to switch to REF_SLICE_TMP3 in execution
      before_ref_item_slice_tmp3 = &qep_tab[primary_tables + tmp_tables - 1];
    /*
      make_sum_func_list() calls rollup_make_fields() which needs the slice
      TMP3 in input; indeed it compares *curr_all_fields (i.e. the fields_list
      of TMP3) with the GROUP BY list (to know which Item of the SELECT list
      should be set to NULL) so this GROUP BY had better point to the items in
      TMP3 for the comparison to work:
    */
    uint save_sliceno = current_ref_item_slice;
    set_ref_item_slice(REF_SLICE_TMP3);
    if (make_sum_func_list(*curr_all_fields, *curr_fields_list, true, true))
      DBUG_RETURN(true);
    /*
      Exit the TMP3 slice, to set up sum funcs, as they take input from
      previous table, not from that slice.
    */
    set_ref_item_slice(save_sliceno);
    const bool need_distinct = !(qep_tab && qep_tab[0].quick() &&
                                 qep_tab[0].quick()->is_agg_loose_index_scan());
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) DBUG_RETURN(true);
    if (setup_sum_funcs(thd, sum_funcs) || thd->is_fatal_error)
      DBUG_RETURN(true);
    // And now set it as input for next phases:
    set_ref_item_slice(REF_SLICE_TMP3);
  }

  if (qep_tab && (group_list || (order && !m_windowing_steps /* [1] */))) {
    /*
      [1] above: too early to do query ORDER BY if we have windowing; must
      wait till after window processing.
    */
    ASSERT_BEST_REF_IN_JOIN_ORDER(this);
    DBUG_PRINT("info", ("Sorting for send_result_set_metadata"));
    THD_STAGE_INFO(thd, stage_sorting_result);
    /*
      If we have already done the group, add HAVING to sorted table except
      when rollup is present
    */
    if (having_cond && !group_list && !sort_and_group &&
        rollup.state == ROLLUP::STATE_NONE) {
      if (add_having_as_tmp_table_cond(curr_tmp_table)) DBUG_RETURN(true);
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
    ORDER_with_src order_arg = group_list ? group_list : order;
    if (qep_tab &&
        m_ordered_index_usage !=
            (group_list ? ORDERED_INDEX_GROUP_BY : ORDERED_INDEX_ORDER_BY) &&
        // Windowing will change order, so it's too early to sort here
        !m_windowing_steps) {
      // Sort either first non-const table or the last tmp table
      QEP_TAB *const sort_tab = &qep_tab[curr_tmp_table];
      if (need_tmp_before_win && !materialize_join && !exec_tmp_table->group)
        explain_flags.set(order_arg.src, ESP_USING_TMPTABLE);

      if (add_sorting_to_table(curr_tmp_table, &order_arg)) DBUG_RETURN(true);
      /*
        filesort_limit:	 Return only this many rows from filesort().
        We can use select_limit_cnt only if we have no group_by and 1 table.
        This allows us to use Bounded_queue for queries like:
          "select SQL_CALC_FOUND_ROWS * from t1 order by b desc limit 1;"
        m_select_limit == HA_POS_ERROR (we need a full table scan)
        unit->select_limit_cnt == 1 (we only need one row in the result set)
      */
      if (sort_tab->filesort)
        sort_tab->filesort->limit =
            (has_group_by || (primary_tables > curr_tmp_table + 1))
                ? m_select_limit
                : unit->select_limit_cnt;
    }
    if (!plan_is_const() &&
        !qep_tab[const_tables].table()->unique_result.io_cache &&
        !qep_tab[const_tables].table()->sort_result.io_cache) {
      /*
        If no IO cache exists for the first table then we are using an
        INDEX SCAN and no filesort. Thus we should not remove the sorted
        attribute on the INDEX SCAN.
      */
      skip_sort_order = true;
    }
  }

  if (qep_tab && m_windowing_steps) {
    for (uint wno = 0; wno < m_windows.elements; wno++) {
      tmp_table_param.m_window = m_windows[wno];

      if (!tmp_tables) {
        curr_tmp_table = primary_tables;
        tmp_tables++;
        if (plan_is_const()) first_select = sub_select_op;

        if (ref_items[REF_SLICE_SAVE].is_null()) {
          /*
           Make a copy of the base slice in the save slice.
           This is needed because later steps will overwrite the base slice with
           another slice (1-3 or window slice).
           After this slice has been used, overwrite the base slice again with
           the copy in the save slice.
           */
          if (alloc_ref_item_slice(thd, REF_SLICE_SAVE)) DBUG_RETURN(true);

          copy_ref_item_slice(REF_SLICE_SAVE, REF_SLICE_BASE);
          current_ref_item_slice = REF_SLICE_SAVE;
        }
      } else {
        curr_tmp_table++;
        tmp_tables++;
      }

      ORDER_with_src dummy = NULL;

      if (last_slice_before_windowing == REF_SLICE_BASE) {
        tmp_table_param.hidden_field_count =
            all_fields.elements - fields_list.elements;
      } else {
        DBUG_ASSERT(tmp_tables >= 1 &&
                    last_slice_before_windowing > REF_SLICE_BASE);

        tmp_table_param.hidden_field_count =
            tmp_all_fields[last_slice_before_windowing].elements -
            tmp_fields_list[last_slice_before_windowing].elements;
      }

      /*
        Allocate a slice of ref items that describe the items to be copied
        from the next temporary table.
      */
      const uint widx = REF_SLICE_WIN_1 + wno;
      const int fbidx = widx + m_windows.elements;  // use far area
      m_windows[wno]->set_needs_restore_input_row(
          wno == 0 && qep_tab[primary_tables - 1].type() == JT_EQ_REF);

      if (m_windows[wno]->needs_buffering()) {
        /*
          Create the window frame buffer tmp table.  We create a
          temporary table with same contents as the output tmp table
          in the windowing pipeline (columns defined by
          curr_all_fields), but used for intermediate storage, saving
          the window's frame buffer now that we know the window needs
          buffering.
        */
        Temp_table_param *par =
            new (thd->mem_root) Temp_table_param(tmp_table_param);
        par->m_window = nullptr;  // Only OUT table needs access to Window

        List<Item> tmplist(*curr_all_fields, thd->mem_root);
        TABLE *table =
            create_tmp_table(thd, par, tmplist, nullptr, false, false,
                             select_lex->active_options(), HA_POS_ERROR, "");
        if (table == nullptr) DBUG_RETURN(true);

        if (alloc_ref_item_slice(thd, fbidx)) DBUG_RETURN(true);

        if (change_to_use_tmp_fields(
                thd, ref_items[fbidx], tmp_fields_list[fbidx],
                tmp_all_fields[fbidx], curr_fields_list->elements,
                *curr_all_fields))
          DBUG_RETURN(true);

        m_windows[wno]->set_frame_buffer_param(par);
        m_windows[wno]->set_frame_buffer(table);
      }

      Opt_trace_object trace_this_tbl(trace);
      trace_this_tbl.add("adding_tmp_table_in_plan_at_position", curr_tmp_table)
          .add_alnum("cause", "output_for_window_functions")
          .add("with_buffer", m_windows[wno]->needs_buffering());
      QEP_TAB *tab = &qep_tab[curr_tmp_table];
      if (create_intermediate_table(tab, curr_all_fields, dummy, false))
        DBUG_RETURN(true);

      m_windows[wno]->set_outtable_param(tab->tmp_table_param);

      if (m_windows[wno]->make_special_rows_cache(thd, tab->table()))
        DBUG_RETURN(true);

      ORDER_with_src w_partition(m_windows[wno]->sorting_order(),
                                 ESC_WINDOWING);

      if (w_partition.order != nullptr) {
        Opt_trace_object trace_pre_sort(trace, "adding_sort_to_previous_table");
        if (add_sorting_to_table(curr_tmp_table - 1, &w_partition, true))
          DBUG_RETURN(true);
      }

      if (m_windows[wno]->is_last()) {
        if (order != nullptr &&
            m_ordered_index_usage != ORDERED_INDEX_ORDER_BY) {
          if (add_sorting_to_table(curr_tmp_table, &order)) DBUG_RETURN(true);
        }
        if (!tab->filesort && !tab->table()->s->keys &&
            (!(select_lex->active_options() & OPTION_BUFFER_RESULT) ||
             need_tmp_before_win || wno >= 1)) {
          /*
            Last tmp table of execution; no sort, no duplicate elimination, no
            buffering imposed by user (or it has already been implemented by
            a previous tmp table): hence any row needn't be written to
            tmp table's storage; send it out to query's result instead:
          */
          tab->tmp_table_param->m_window_short_circuit = true;
        }
      }

      if (alloc_ref_item_slice(thd, widx)) DBUG_RETURN(true);

      if (change_to_use_tmp_fields(
              thd, ref_items[widx], tmp_fields_list[widx], tmp_all_fields[widx],
              fields_list.elements,
              (last_slice_before_windowing == REF_SLICE_BASE
                   ? all_fields
                   : tmp_all_fields[last_slice_before_windowing])))
        DBUG_RETURN(true);

      curr_fields_list = &tmp_fields_list[widx];
      curr_all_fields = &tmp_all_fields[widx];
      set_ref_item_slice(widx);
      tab->ref_item_slice = widx;
      setup_tmptable_write_func(tab, &trace_this_tbl);

      if (having_cond != nullptr) {
        tab->having = having_cond;
        having_cond = nullptr;
      }

      last_slice_before_windowing = widx;
    }
  }

  fields = curr_fields_list;
  // Reset before execution
  set_ref_item_slice(REF_SLICE_SAVE);
  if (qep_tab) {
    qep_tab[primary_tables + tmp_tables - 1].next_select =
        get_end_select_func();
  }
  grouped = has_group_by;

  unplug_join_tabs();

  /*
    Tmp tables are a layer between the nested loop and the derived table's
    result, WITH RECURSIVE cannot work with them. This should not happen, as a
    recursive query cannot have clauses which use a tmp table (GROUP BY,
    etc).
  */
  DBUG_ASSERT(!select_lex->is_recursive() || !tmp_tables);
  DBUG_RETURN(false);
}

void JOIN::unplug_join_tabs() {
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);
  for (uint i = 0; i < tables; ++i) best_ref[i]->cleanup();

  best_ref = NULL;
}

/**
  @brief Add Filesort object to the given table to sort if with filesort

  @param idx        JOIN_TAB's position in the qep_tab array. The
                    created Filesort object gets attached to this.

  @param sort_order List of expressions to sort the table by
  @param force_stable_sort
                    If true, use stable sort, that is the sort will
                    keep the reative order of equivalent elements.
                    Needed for windowing semantics.

  @note This function moves tab->select, if any, to filesort->select

  @return false on success, true on OOM
*/

bool JOIN::add_sorting_to_table(uint idx, ORDER_with_src *sort_order,
                                bool force_stable_sort) {
  DBUG_ENTER("add_sorting_to_table");
  ASSERT_BEST_REF_IN_JOIN_ORDER(this);
  DBUG_ASSERT(!select_lex->is_recursive());
  const enum join_type jt = qep_tab[idx].type();
  if (jt == JT_CONST || jt == JT_EQ_REF)
    DBUG_RETURN(false);  // 1 single row: is already sorted

  explain_flags.set(sort_order->src, ESP_USING_FILESORT);
  QEP_TAB *const tab = &qep_tab[idx];
  tab->filesort = new (thd->mem_root)
      Filesort(tab, *sort_order, HA_POS_ERROR, force_stable_sort);
  if (!tab->filesort) DBUG_RETURN(true);
  Opt_trace_object trace_tmp(&thd->opt_trace, "filesort");
  trace_tmp.add("adding_sort_to_table_in_plan_at_position", idx);
  {
    if (tab->ref().key >= 0) {
      TABLE *table = tab->table();
      if (tab->quick()) {
        /*
          We can only use 'Only index' if quick key is same as ref_key
          and in index_merge 'Only index' cannot be used
        */
        if (((uint)tab->ref().key != tab->quick()->index))
          table->set_keyread(false);
      } else {
        /*
          We have a ref on a const;  Change this to a range that filesort
          can use.
          For impossible ranges (like when doing a lookup on NULL on a NOT NULL
          field, quick will contain an empty record set.

          @TODO This should be either indicated as range or filesort
          should start using ref directly, without switching to quick.
        */
        JOIN_TAB *const jtab = best_ref[idx];
        QUICK_SELECT_I *q =
            tab->type() == JT_FT
                ? get_ft_select(thd, table, tab->ref().key)
                : get_quick_select_for_ref(thd, table, &tab->ref(),
                                           jtab->found_records);
        if (!q) DBUG_RETURN(true); /* purecov: inspected */
        tab->set_quick(q);         // We keep it displaid as "ref".
      }
    }
  }
  tab->read_first_record = join_init_read_record;
  DBUG_RETURN(false);
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
                                      value for further use in QUICK_SELECT_DESC

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
  DBUG_ENTER("test_if_cheaper_ordering");
  /*
    Check whether there is an index compatible with the given order
    usage of which is cheaper than usage of the ref_key index (ref_key>=0)
    or a table scan.
    It may be the case if ORDER/GROUP BY is used with LIMIT.
  */
  ha_rows best_select_limit = HA_POS_ERROR;
  JOIN *join = tab ? tab->join() : NULL;
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
    DBUG_ASSERT(refkey_rows_estimate >= 1.0);
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
          (ref_key < 0 && (group || table->force_index))) {
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
          set_if_bigger(rec_per_key, 1.0f);
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
        select_limit =
            (ha_rows)(select_limit < fanout ? 1 : select_limit / fanout);
        /*
          We assume that each of the tested indexes is not correlated
          with ref_key. Thus, to select first N records we have to scan
          N/selectivity(ref_key) index entries.
          selectivity(ref_key) = #scanned_records/#table_records =
          refkey_rows_estimate/table_records.
          In any case we can't select more than #table_records.
          N/(refkey_rows_estimate/table_records) > table_records
          <=> N > refkey_rows_estimate.
         */
        if (select_limit > refkey_rows_estimate)
          select_limit = table_records;
        else
          select_limit = (ha_rows)(select_limit * (double)table_records /
                                   refkey_rows_estimate);
        rec_per_key =
            keyinfo->records_per_key(keyinfo->user_defined_key_parts - 1);
        set_if_bigger(rec_per_key, 1.0f);
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
            min<double>(table->cost_model()->page_read_cost(rec_per_key),
                        table_scan_time.total_cost());

        /*
          Switch to index that gives order if its scan time is smaller than
          read_time of current chosen access method. In addition, if the
          current chosen access method is index scan or table scan, always
          switch to the index that gives order when it is covering or when
          force index or group by is present.
        */
        if (((cur_access_method == JT_ALL ||
              cur_access_method == JT_INDEX_SCAN) &&
             (is_covering || group || table->force_index)) ||
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

  if (best_key < 0 || best_key == ref_key) DBUG_RETURN(false);

  *new_key = best_key;
  *new_key_direction = best_key_direction;
  *new_select_limit = has_limit ? best_select_limit : table_records;
  if (new_used_key_parts != NULL) *new_used_key_parts = best_key_parts;

  DBUG_RETURN(true);
}

/**
  Find a key to apply single table UPDATE/DELETE by a given ORDER

  @param       order           Linked list of ORDER BY arguments
  @param       tab             Table to find a key
  @param       limit           LIMIT clause parameter
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

uint get_index_for_order(ORDER_with_src *order, QEP_TAB *tab, ha_rows limit,
                         bool *need_sort, bool *reverse) {
  if (tab->quick() &&
      tab->quick()->unique_key_range()) {  // Single row select (always
                                           // "ordered"): Ok to use with key
                                           // field UPDATE
    *need_sort = false;
    /*
      Returning of MAX_KEY here prevents updating of used_key_is_modified
      in mysql_update(). Use quick select "as is".
    */
    return MAX_KEY;
  }

  TABLE *const table = tab->table();

  if (!*order) {
    *need_sort = false;
    if (tab->quick())
      return tab->quick()->index;  // index or MAX_KEY, use quick select as is
    else
      return table->file
          ->key_used_on_scan;  // MAX_KEY or index for some engines
  }

  if (!is_simple_order(*order))  // just to cut further expensive checks
  {
    *need_sort = true;
    return MAX_KEY;
  }

  if (tab->quick()) {
    if (tab->quick()->index == MAX_KEY) {
      *need_sort = true;
      return MAX_KEY;
    }

    uint used_key_parts;
    bool skip_quick;
    switch (test_if_order_by_key(order, table, tab->quick()->index,
                                 &used_key_parts, &skip_quick)) {
      case 1:  // desired order
        *need_sort = false;
        return tab->quick()->index;
      case 0:  // unacceptable order
        *need_sort = true;
        return MAX_KEY;
      case -1:  // desired order, but opposite direction
      {
        QUICK_SELECT_I *reverse_quick;
        if (!skip_quick &&
            (reverse_quick = tab->quick()->make_reverse(used_key_parts))) {
          delete tab->quick();
          tab->set_quick(reverse_quick);
          tab->set_type(calc_join_type(reverse_quick->get_type()));
          *need_sort = false;
          return reverse_quick->index;
        } else {
          *need_sort = true;
          return MAX_KEY;
        }
      }
    }
    DBUG_ASSERT(0);
  } else if (limit != HA_POS_ERROR) {  // check if some index scan & LIMIT is
                                       // more efficient than filesort

    /*
      Update quick_condition_rows since single table UPDATE/DELETE procedures
      don't call JOIN::make_join_plan() and leave this variable uninitialized.
    */
    table->quick_condition_rows = table->file->stats.records;

    int key, direction;
    if (test_if_cheaper_ordering(NULL, order, table,
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
  return key_info->table->in_use->optimizer_switch_flag(
             OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
             ? key_info->actual_key_parts
             : key_info->user_defined_key_parts;
}

/**
  Returns key flags depending on
  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS flag.

  @param  key_info  pointer to KEY structure

  @return key flags.
*/

static uint actual_key_flags(KEY *key_info) {
  return key_info->table->in_use->optimizer_switch_flag(
             OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
             ? key_info->actual_flags
             : key_info->flags;
}

join_type calc_join_type(int quick_type) {
  if ((quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION))
    return JT_INDEX_MERGE;
  else
    return JT_RANGE;
}

/**
  @} (end of group Query_Optimizer)
*/

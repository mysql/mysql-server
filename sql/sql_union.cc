/* Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
  Process query expressions that are composed of

  1. UNION of query blocks, and/or

  2. have ORDER BY / LIMIT clauses in more than one level.

  An example of 2) is:

    (SELECT * FROM t1 ORDER BY a LIMIT 10) ORDER BY b LIMIT 5

  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/

#include "sql/sql_union.h"

#include "my_config.h"

#include <string.h>
#include <sys/types.h>

#include "memory_debugging.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"     // DEBUG_SYNC
#include "sql/error_handler.h"  // Strict_error_handler
#include "sql/field.h"
#include "sql/filesort.h"  // filesort_free_buffers
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_subselect.h"
#include "sql/mem_root_array.h"
#include "sql/opt_explain.h"  // explain_no_table
#include "sql/opt_explain_format.h"
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_node_base.h"
#include "sql/query_options.h"
#include "sql/set_var.h"
#include "sql/sql_base.h"  // fill_record
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"   // tmp tables
#include "sql/table_function.h"  // Table_function
#include "sql/thr_malloc.h"
#include "sql/window.h"  // Window
#include "template_utils.h"

bool Query_result_union::prepare(List<Item> &, SELECT_LEX_UNIT *u) {
  unit = u;
  return false;
}

bool Query_result_union::send_data(List<Item> &values) {
  // Skip "offset" number of rows before producing rows
  if (unit->offset_limit_cnt > 0) {
    unit->offset_limit_cnt--;
    return false;
  }
  if (fill_record(thd, table, table->visible_field_ptr(), values, NULL, NULL))
    return true; /* purecov: inspected */

  if (!check_unique_constraint(table)) return false;

  const int error = table->file->ha_write_row(table->record[0]);
  if (!error) {
    m_rows_in_table++;
    return false;
  }
  // create_ondisk_from_heap will generate error if needed
  if (!table->file->is_ignorable_error(error)) {
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, table, tmp_table_param.start_recinfo,
                                &tmp_table_param.recinfo, error, true,
                                &is_duplicate))
      return true; /* purecov: inspected */
    // Table's engine changed, index is not initialized anymore
    if (table->hash_field) table->file->ha_index_init(0, false);
    if (!is_duplicate) m_rows_in_table++;
  }
  return false;
}

bool Query_result_union::send_eof() { return false; }

bool Query_result_union::flush() { return false; }

/**
  Create a temporary table to store the result of Query_result_union.

  @param thd_arg            thread handle
  @param column_types       a list of items used to define columns of the
                            temporary table
  @param is_union_distinct  if set, the temporary table will eliminate
                            duplicates on insert
  @param options            create options
  @param table_alias        name of the temporary table
  @param bit_fields_as_long convert bit fields to ulonglong
  @param create_table If false, a table handler will not be created when
                      creating the result table.

  @details
    Create a temporary table that is used to store the result of a UNION,
    derived table, or a materialized cursor.

  @returns false if table created, true if error
*/

bool Query_result_union::create_result_table(
    THD *thd_arg, List<Item> *column_types, bool is_union_distinct,
    ulonglong options, const char *table_alias, bool bit_fields_as_long,
    bool create_table) {
  DBUG_ASSERT(table == NULL);
  tmp_table_param = Temp_table_param();
  count_field_types(thd_arg->lex->current_select(), &tmp_table_param,
                    *column_types, false, true);
  tmp_table_param.skip_create_table = !create_table;
  tmp_table_param.bit_fields_as_long = bit_fields_as_long;
  if (unit != nullptr) {
    if (unit->is_recursive()) {
      /*
        If the UNIQUE key specified for UNION DISTINCT were a primary key in
        InnoDB, rows would be returned by the scan in an order depending on
        their columns' values, not in insertion order.
      */
      tmp_table_param.can_use_pk_for_unique = false;
    }
    if (unit->mixed_union_operators()) {
      /*
        Generally, UNIQUE key can be promoted to PK, saving the space
        consumption of a hidden PK. However, if the query mixes UNION ALL and
        UNION DISTINCT, the PK will be disabled at some point in execution,
        which InnoDB doesn't support as it uses a clustered PK. Then, no PK:
      */
      tmp_table_param.can_use_pk_for_unique = false;
    }
  }

  if (!(table = create_tmp_table(thd_arg, &tmp_table_param, *column_types, NULL,
                                 is_union_distinct, true, options, HA_POS_ERROR,
                                 (char *)table_alias)))
    return true;
  if (create_table) {
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table->hash_field) table->file->ha_index_init(0, 0);
  }
  return false;
}

/**
  Reset and empty the temporary table that stores the materialized query result.

  @note The cleanup performed here is exactly the same as for the two temp
  tables of JOIN - exec_tmp_table_[1 | 2].
*/

void Query_result_union::cleanup() {
  if (table == NULL) return;
  table->file->extra(HA_EXTRA_RESET_STATE);
  if (table->hash_field) table->file->ha_index_or_rnd_end();
  table->file->ha_delete_all_rows();
  free_io_cache(table);
  filesort_free_buffers(table, 0);
}

/**
  UNION result that is passed directly to the receiving Query_result
  without filling a temporary table.

  Function calls are forwarded to the wrapped Query_result, but some
  functions are expected to be called only once for each query, so
  they are only executed for the first query block in the union (except
  for send_eof(), which is executed only for the last query block).

  This Query_result is used when a UNION is not DISTINCT and doesn't
  have a global ORDER BY clause. @see SELECT_LEX_UNIT::prepare().
*/
class Query_result_union_direct final : public Query_result_union {
 private:
  /// Result object that receives all rows
  Query_result *result;
  /// The last query block of the union
  SELECT_LEX *last_select_lex;

  /// Wrapped result is optimized
  bool optimized;
  /// Wrapped result has sent metadata
  bool result_set_metadata_sent;
  /// Wrapped result has started execution
  bool execution_started;

  /// Accumulated current_found_rows
  ulonglong current_found_rows;

  /// Number of rows offset
  ha_rows offset;
  /// Number of rows limit + offset, @see Query_result_union_direct::send_data()
  ha_rows limit;

 public:
  Query_result_union_direct(THD *thd, Query_result *result,
                            SELECT_LEX *last_select_lex)
      : Query_result_union(thd),
        result(result),
        last_select_lex(last_select_lex),
        optimized(false),
        result_set_metadata_sent(false),
        execution_started(false),
        current_found_rows(0) {}
  bool change_query_result(Query_result *new_result) override;
  uint field_count(List<Item> &) const override {
    // Only called for top-level Query_results, usually Query_result_send
    DBUG_ASSERT(false); /* purecov: inspected */
    return 0;           /* purecov: inspected */
  }
  bool postponed_prepare(List<Item> &types) override;
  bool send_result_set_metadata(List<Item> &list, uint flags) override;
  bool send_data(List<Item> &items) override;
  bool optimize() override {
    if (optimized) return false;
    optimized = true;

    return result->optimize();
  }
  bool start_execution() override {
    if (execution_started) return false;
    execution_started = true;
    return result->start_execution();
  }
  void send_error(uint errcode, const char *err) override {
    result->send_error(errcode, err); /* purecov: inspected */
  }
  bool send_eof() override;
  bool flush() override { return false; }
  bool check_simple_select() const override {
    // Only called for top-level Query_results, usually Query_result_send
    DBUG_ASSERT(false); /* purecov: inspected */
    return false;       /* purecov: inspected */
  }
  void abort_result_set() override {
    result->abort_result_set(); /* purecov: inspected */
  }
  void cleanup() override {}
  void set_thd(THD *) {
    /*
      Only called for top-level Query_results, usually Query_result_send,
      and for the results of subquery engines
      (select_<something>_subselect).
    */
    DBUG_ASSERT(false); /* purecov: inspected */
  }
};

/**
  Replace the current query result with new_result and prepare it.

  @param new_result New query result

  @returns false if success, true if error
*/
bool Query_result_union_direct::change_query_result(Query_result *new_result) {
  result = new_result;
  return result->prepare(unit->types, unit);
}

bool Query_result_union_direct::postponed_prepare(List<Item> &types) {
  if (result == NULL) return false;

  return result->prepare(types, unit);
}

bool Query_result_union_direct::send_result_set_metadata(List<Item> &,
                                                         uint flags) {
  if (result_set_metadata_sent) return false;
  result_set_metadata_sent = true;

  /*
    Set global offset and limit to be used in send_data(). These can
    be variables in prepared statements or stored programs, so they
    must be reevaluated for each execution.
   */
  offset = unit->global_parameters()->get_offset();
  limit = unit->global_parameters()->get_limit();
  if (limit + offset >= limit)
    limit += offset;
  else
    limit = HA_POS_ERROR; /* purecov: inspected */

  return result->send_result_set_metadata(unit->types, flags);
}

bool Query_result_union_direct::send_data(List<Item> &items) {
  if (limit == 0) return false;
  limit--;
  if (offset) {
    offset--;
    return false;
  }

  if (fill_record(thd, table, table->field, items, NULL, NULL))
    return true; /* purecov: inspected */

  return result->send_data(unit->item_list);
}

bool Query_result_union_direct::send_eof() {
  /*
    Accumulate the found_rows count for the current query block into the UNION.
    Number of rows returned from a query block is always non-negative.
  */
  ulonglong offset = thd->lex->current_select()->get_offset();
  current_found_rows +=
      thd->current_found_rows > offset ? thd->current_found_rows - offset : 0;

  if (unit->thd->lex->current_select() == last_select_lex) {
    /*
      If SQL_CALC_FOUND_ROWS is not enabled, adjust the current_found_rows
      according to the global limit and offset defined.
    */
    if (!(unit->first_select()->active_options() & OPTION_FOUND_ROWS)) {
      ha_rows global_limit = unit->global_parameters()->get_limit();
      ha_rows global_offset = unit->global_parameters()->get_offset();

      if (global_limit != HA_POS_ERROR) {
        if (global_offset != HA_POS_ERROR) global_limit += global_offset;

        if (current_found_rows > global_limit)
          current_found_rows = global_limit;
      }
    }
    thd->current_found_rows = current_found_rows;

    // Reset and make ready for re-execution
    // @todo: Dangerous if we have an error midway?
    result_set_metadata_sent = false;
    optimized = false;
    execution_started = false;

    return result->send_eof();
  } else
    return false;
}

/// RAII class to automate saving/restoring of current_select()
class Change_current_select {
 public:
  Change_current_select(THD *thd_arg)
      : thd(thd_arg), saved_select(thd->lex->current_select()) {}
  void restore() { thd->lex->set_current_select(saved_select); }
  ~Change_current_select() { restore(); }

 private:
  THD *thd;
  SELECT_LEX *saved_select;
};

/**
  Prepare the fake_select_lex query block

  @param thd_arg Thread handler

  @returns false if success, true if error
*/

bool SELECT_LEX_UNIT::prepare_fake_select_lex(THD *thd_arg) {
  DBUG_ENTER("SELECT_LEX_UNIT::prepare_fake_select_lex");

  DBUG_ASSERT(thd_arg->lex->current_select() == fake_select_lex);

  // The UNION result table is input table for this query block
  fake_select_lex->table_list.link_in_list(&result_table_list,
                                           &result_table_list.next_local);

  result_table_list.select_lex = fake_select_lex;

  // Set up the result table for name resolution
  fake_select_lex->context.table_list =
      fake_select_lex->context.first_name_resolution_table =
          fake_select_lex->get_table_list();
  if (!fake_select_lex->first_execution) {
    for (ORDER *order = fake_select_lex->order_list.first; order;
         order = order->next)
      order->item = &order->item_ptr;
  }
  for (ORDER *order = fake_select_lex->order_list.first; order;
       order = order->next) {
    (*order->item)
        ->walk(&Item::change_context_processor, Item::WALK_POSTFIX,
               (uchar *)&fake_select_lex->context);
  }
  fake_select_lex->set_query_result(query_result());

  fake_select_lex->fields_list = item_list;

  /*
    We need to add up n_sum_items in order to make the correct
    allocation in setup_ref_array().
    Don't add more sum_items if we have already done SELECT_LEX::prepare
    for this (with a different join object)
  */
  if (fake_select_lex->base_ref_items.is_null())
    fake_select_lex->n_child_sum_items += fake_select_lex->n_sum_items;

  DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
              fake_select_lex->master_unit() == this &&
              !fake_select_lex->group_list.elements &&
              fake_select_lex->where_cond() == NULL &&
              fake_select_lex->having_cond() == NULL);

  if (is_recursive()) {
    /*
      The fake_select_lex's JOIN is going to read result_table_list
      repeatedly, so this table has all the attributes of a recursive
      reference:
    */
    result_table_list.set_recursive_reference();
  }

  if (fake_select_lex->prepare(thd_arg)) DBUG_RETURN(true);

  DBUG_RETURN(false);
}

/**
  Prepares all query blocks of a query expression, including
  fake_select_lex. If a recursive query expression, this also creates the
  materialized temporary table.

  @param thd_arg       Thread handler
  @param sel_result    Result object where the unit's output should go.
  @param added_options These options will be added to the query blocks.
  @param removed_options Options that cannot be used for this query

  @returns false if success, true if error
 */
bool SELECT_LEX_UNIT::prepare(THD *thd_arg, Query_result *sel_result,
                              ulonglong added_options,
                              ulonglong removed_options) {
  DBUG_ENTER("SELECT_LEX_UNIT::prepare");

  DBUG_ASSERT(!is_prepared());
  Change_current_select save_select(thd);

  Query_result *tmp_result;
  bool instantiate_tmp_table = false;

  SELECT_LEX *last_select = first_select();
  while (last_select->next_select()) last_select = last_select->next_select();

  set_query_result(sel_result);

  thd_arg->lex->set_current_select(first_select());

  // Save fake_select_lex in case we don't need it for anything but
  // global parameters.
  if (saved_fake_select_lex == NULL &&  // Don't overwrite on PS second prepare
      fake_select_lex != NULL)
    saved_fake_select_lex = fake_select_lex;

  const bool simple_query_expression = is_simple();

  // Create query result object for use by underlying query blocks
  if (!simple_query_expression) {
    if (is_union() && !union_needs_tmp_table()) {
      if (!(tmp_result = union_result = new (*THR_MALLOC)
                Query_result_union_direct(thd, sel_result, last_select)))
        goto err; /* purecov: inspected */
      if (fake_select_lex != NULL) fake_select_lex = NULL;
      instantiate_tmp_table = false;
    } else {
      if (!(tmp_result = union_result =
                new (*THR_MALLOC) Query_result_union(thd)))
        goto err; /* purecov: inspected */
      instantiate_tmp_table = true;
    }
  } else {
    // Only one query block, and no "fake" object: No extra result needed:
    tmp_result = sel_result;
  }

  if (fake_select_lex != NULL) {
    /*
      There exists a query block that consolidates the UNION result.
      Prepare the active options for this query block. If these options
      contain OPTION_BUFFER_RESULT, the query block will perform a buffering
      operation, which means that an underlying query block does not need to
      buffer its result, and the buffer option for the underlying query blocks
      can be cleared.
      For subqueries in form "a IN (SELECT .. UNION SELECT ..):
      when optimizing the fake_select_lex that reads the results of the union
      from a temporary table, do not mark the temp. table as constant because
      the contents in it may vary from one subquery execution to another, by
      adding OPTION_NO_CONST_TABLES.
    */
    fake_select_lex->make_active_options(
        (added_options & (OPTION_FOUND_ROWS | OPTION_BUFFER_RESULT)) |
            OPTION_NO_CONST_TABLES | SELECT_NO_UNLOCK,
        0);
    added_options &= ~OPTION_BUFFER_RESULT;
  }
  first_select()->context.resolve_in_select_list = true;

  for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select()) {
    // All query blocks get their options in this phase
    sl->set_query_result(tmp_result);
    sl->make_active_options(added_options | SELECT_NO_UNLOCK, removed_options);
    sl->fields_list = sl->item_list;
    /*
      setup_tables_done_option should be set only for very first SELECT,
      because it protect from second setup_tables call for select-like non
      select commands (DELETE/INSERT/...) and they use only very first
      SELECT (for union it can be only INSERT ... SELECT).
    */
    added_options &= ~OPTION_SETUP_TABLES_DONE;

    thd_arg->lex->set_current_select(sl);

    if (sl == first_recursive) {
      // create_result_table() depends on current_select()
      save_select.restore();
      /*
        All next query blocks will read the temporary table, which we must
        thus create now:
      */
      if (derived_table->setup_materialized_derived_tmp_table(thd_arg))
        goto err; /* purecov: inspected */
      thd_arg->lex->set_current_select(sl);
    }

    if (sl->recursive_reference)  // Make tmp table known to query block:
      derived_table->common_table_expr()->substitute_recursive_reference(
          thd_arg, sl);

    if (sl->prepare(thd_arg)) goto err;

    /*
      Use items list of underlaid select for derived tables to preserve
      information about fields lengths and exact types
    */
    if (!is_union())
      types = first_select()->item_list;
    else if (sl == first_select()) {
      types.empty();
      List_iterator_fast<Item> it(sl->item_list);
      Item *item_tmp;
      while ((item_tmp = it++)) {
        /*
          If the outer query has a GROUP BY clause, an outer reference to this
          query block may have been wrapped in a Item_outer_ref, which has not
          been fixed yet. An Item_type_holder must be created based on a fixed
          Item, so use the inner Item instead.
        */
        DBUG_ASSERT(item_tmp->fixed ||
                    (item_tmp->type() == Item::REF_ITEM &&
                     down_cast<Item_ref *>(item_tmp)->ref_type() ==
                         Item_ref::OUTER_REF));
        if (!item_tmp->fixed) item_tmp = item_tmp->real_item();

        auto holder = new Item_type_holder(thd_arg, item_tmp);
        if (!holder) goto err; /* purecov: inspected */
        if (is_recursive()) {
          holder->maybe_null = true;  // Always nullable, per SQL standard.
          /*
            The UNION code relies on join_types() to change some
            transitional types like MYSQL_TYPE_DATETIME2 into other types; in
            case this is the only nonrecursive query block join_types() won't
            be called so we need an explicit call:
          */
          holder->join_types(thd_arg, item_tmp);
        }
        types.push_back(holder);
      }
    } else {
      if (types.elements != sl->item_list.elements) {
        my_error(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, MYF(0));
        goto err;
      }
      if (sl->recursive_reference) {
        /*
          Recursive query blocks don't determine output types of the result.
          The only thing to check could be if the recursive query block has a
          type which can't be cast to the output type of the result.
          But in MySQL, all types can be cast to each other (at least during
          resolution; an error may reported when trying to actually insert, for
          example an INT into a POINT). So no further compatibility check is
          needed here.
        */
      } else {
        List_iterator_fast<Item> it(sl->item_list);
        List_iterator_fast<Item> tp(types);
        Item *type, *item_tmp;
        while ((type = tp++, item_tmp = it++)) {
          if (((Item_type_holder *)type)->join_types(thd_arg, item_tmp))
            goto err;
        }
      }
    }

    if (sl->recursive_reference &&
        (sl->is_grouped() || sl->m_windows.elements > 0)) {
      // Per SQL2011.
      my_error(ER_CTE_RECURSIVE_FORBIDS_AGGREGATION, MYF(0),
               derived_table->alias);
      goto err;
    }
  }

  if (is_recursive()) {
    // This had to wait until all query blocks are prepared:
    if (check_materialized_derived_query_blocks(thd_arg))
      goto err; /* purecov: inspected */
  }

  /*
    If the query is using Query_result_union_direct, we have postponed
    preparation of the underlying Query_result until column types are known.
  */
  if (union_result != NULL && union_result->postponed_prepare(types)) goto err;

  if (!simple_query_expression) {
    /*
      Check that it was possible to aggregate all collations together for UNION.
      We need this in case of UNION DISTINCT, to filter out duplicates using
      the proper collation.

      TODO: consider removing this test in case of UNION ALL.
    */
    List_iterator_fast<Item> tp(types);
    Item *type;

    while ((type = tp++)) {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE) {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }
    ulonglong create_options =
        first_select()->active_options() | TMP_TABLE_ALL_COLUMNS;
    /*
      Force the temporary table to be a MyISAM table if we're going to use
      fulltext functions (MATCH ... AGAINST .. IN BOOLEAN MODE) when reading
      from it (this should be removed when fulltext search is moved
      out of MyISAM).
    */
    if (fake_select_lex && fake_select_lex->ftfunc_list->elements)
      create_options |= TMP_TABLE_FORCE_MYISAM;

    if (union_result->create_result_table(
            thd, &types, union_distinct != nullptr, create_options, "", false,
            instantiate_tmp_table))
      goto err;
    result_table_list = TABLE_LIST();
    result_table_list.db = (char *)"";
    result_table_list.table_name = result_table_list.alias = (char *)"union";
    result_table_list.table = table = union_result->table;
    table->pos_in_table_list = &result_table_list;
    result_table_list.select_lex =
        fake_select_lex ? fake_select_lex : saved_fake_select_lex;
    result_table_list.set_tableno(0);

    result_table_list.set_privileges(SELECT_ACL);

    if (!item_list.elements) {
      Prepared_stmt_arena_holder ps_arena_holder(thd);
      if (table->fill_item_list(&item_list)) goto err; /* purecov: inspected */
    } else {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      table->reset_item_list(&item_list);
    }
    if (fake_select_lex != NULL) {
      thd_arg->lex->set_current_select(fake_select_lex);

      if (prepare_fake_select_lex(thd_arg)) goto err;
    }
  }

  // Query blocks are prepared, update the state
  set_prepared();

  DBUG_RETURN(false);

err:
  (void)cleanup(false);
  DBUG_RETURN(true);
}

/**
  Optimize all query blocks of a query expression, including fake_select_lex

  @param thd    thread handler

  @returns false if optimization successful, true if error
*/

bool SELECT_LEX_UNIT::optimize(THD *thd) {
  DBUG_ENTER("SELECT_LEX_UNIT::optimize");

  DBUG_ASSERT(is_prepared() && !is_optimized());

  Change_current_select save_select(thd);

  for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select()) {
    thd->lex->set_current_select(sl);

    // LIMIT is required for optimization
    if (set_limit(thd, sl)) DBUG_RETURN(true); /* purecov: inspected */

    if (sl->optimize(thd)) DBUG_RETURN(true);

    /*
      Accumulate estimated number of rows.
      1. Implicitly grouped query has one row (with HAVING it has zero or one
         rows).
      2. If GROUP BY clause is optimized away because it was a constant then
         query produces at most one row.
    */
    if (query_result())
      query_result()->estimated_rowcount +=
          sl->is_implicitly_grouped() || sl->join->group_optimized_away
              ? 1
              : sl->join->best_rowcount;
  }
  if (fake_select_lex) {
    thd->lex->set_current_select(fake_select_lex);

    if (set_limit(thd, fake_select_lex))
      DBUG_RETURN(true); /* purecov: inspected */

    /*
      In EXPLAIN command, constant subqueries that do not use any
      tables are executed two times:
       - 1st time is a real evaluation to get the subquery value
       - 2nd time is to produce EXPLAIN output rows.
      1st execution sets certain members (e.g. Query_result) to perform
      subquery execution rather than EXPLAIN line production. In order
      to reset them back, we re-do all of the actions (yes it is ugly).
    */
    DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
                fake_select_lex->master_unit() == this &&
                !fake_select_lex->group_list.elements &&
                fake_select_lex->get_table_list() == &result_table_list &&
                fake_select_lex->where_cond() == NULL &&
                fake_select_lex->having_cond() == NULL);

    if (fake_select_lex->optimize(thd)) DBUG_RETURN(true);
  }
  set_optimized();  // All query blocks optimized, update the state
  DBUG_RETURN(false);
}

/**
  Explain query starting from this unit.

  @param ethd  THD of explaining thread

  @return false if success, true if error
*/

bool SELECT_LEX_UNIT::explain(THD *ethd) {
  DBUG_ENTER("SELECT_LEX_UNIT::explain");

#ifndef DBUG_OFF
  SELECT_LEX *lex_select_save = thd->lex->current_select();
#endif
  Explain_format *fmt = ethd->lex->explain_format;
  const bool other = (thd != ethd);
  bool ret = false;

  DBUG_ASSERT(other || is_optimized() || outer_select()->is_empty_query() ||
              // @todo why is this necessary?
              outer_select()->join == nullptr ||
              outer_select()->join->zero_result_cause);

  if (fmt->begin_context(CTX_UNION)) DBUG_RETURN(true);

  for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select()) {
    if (fmt->begin_context(CTX_QUERY_SPEC)) DBUG_RETURN(true);
    if (explain_query_specification(ethd, sl, CTX_JOIN) ||
        fmt->end_context(CTX_QUERY_SPEC))
      DBUG_RETURN(true);
  }

  if (fake_select_lex != NULL) {
    // Don't save result as it's needed only for consequent exec.
    ret = explain_query_specification(ethd, fake_select_lex, CTX_UNION_RESULT);
  }
  if (!other) DBUG_ASSERT(thd->lex->current_select() == lex_select_save);

  if (ret) DBUG_RETURN(true);
  fmt->end_context(CTX_UNION);

  DBUG_RETURN(false);
}

/**
   Helper class for SELECT_LEX_UNIT::execute(). Manages executions of
   non-recursive and recursive query blocks (if any).

   There are two possible flows of data rows for recursive CTEs:

   1) Assuming QB1 UNION ALL QBR2 UNION ALL QBR3, where QBRi are recursive, we
   have a single tmp table (the derived table's result):

   QB1 appends rows to tmp table.
   Label eval_recursive_members:
   QBR2 reads tmp table and appends new rows to it; it also reads its own
   new rows, etc (loop back) until no more rows.
   QBR3 same.
   If rows have been inserted since we passed the label, go to the label.

   2) Assuming QB1 UNION DISTINCT QBR2 UNION DISTINCT QBR3, where QBRi are
   recursive, we have two tmp tables (the union-distinct's result UR and, at
   the external layer, the derived table's result DR; UR has a unique index);
   FAKE is the fake_select_lex:

   QB1 appends rows to UR.
   FAKE reads from UR and appends to DR.
   Label eval_recursive_members:
   QBR2 reads DR and appends new rows to UR; thus it does not read its own
   new rows as they are not in DR yet.
   QBR3 same.
   FAKE reads from UR and appends to DR.
   If rows have been inserted into DR since we passed the label, go to the
   label.

   In both flows, sub_select() is used to read the recursive reference with a
   table scan. It reads until there are no more rows, which could be simply
   implemented by reading until the storage engine reports EOF, but is
   not. The reason is that storage engines (MEMORY, InnoDB) have behaviour at
   EOF which isn't compatible with the requirement to catch up with new rows:
   1) In both engines, when they report EOF, the scan stays blocked at EOF
   forever even if rows are later inserted. In detail, this happens because
   heap_scan() unconditionally increments info->current_record, and because
   InnoDB has a supremum record.
   2) Specifically for the MEMORY engine: the UNION DISTINCT table of a
   recursive CTE receives interlaced writes (which can hit a duplicate key)
   and reads. A read cursor is corrupted by a write if there is a duplicate key
   error. Scenario:
      - write 'A'
      - write 'A': allocates a record, hits a duplicate key error, leaves
      the allocated place as "deleted record".
      - init scan
      - read: finds 'A' at #0
      - read: finds deleted record at #1, properly skips over it, moves to EOF
      - even if we save the read position at this point, it's "after #1"
      - close scan
      - write 'B': takes the place of deleted record, i.e. writes at #1
      - write 'C': writes at #2
      - init scan, reposition at saved position
      - read: still after #1, so misses 'B'.
     In this scenario, the table is formed of real records followed by
     deleted records and then EOF.
   3) To avoid those problems, sub_select() stops reading when it has read the
   count of real records in the table, thus engines never hit EOF or a deleted
   record.
*/
class Recursive_executor {
 private:
  SELECT_LEX_UNIT *const unit;
  THD *const thd;
  Strict_error_handler strict_handler;
  enum_check_fields save_check_for_truncated_fields;
  sql_mode_t save_sql_mode;
  enum { DISABLED_TRACE = 1, POP_HANDLER = 2, EXEC_RECURSIVE = 4 };
  uint8 flags;  ///< bitmap made of the above enum bits
  /**
    If recursive: count of rows in the temporary table when we started the
    current iteration of the for-loop which executes query blocks.
  */
  ha_rows row_count;
  TABLE *table;          ///< Table for result of union
  handler *cached_file;  ///< 'handler' of 'table'
  /// Space to store a row position (InnoDB uses 6 bytes, MEMORY uses 16)
  uchar row_ref[16];

 public:
  Recursive_executor(SELECT_LEX_UNIT *unit_arg, THD *thd_arg)
      : unit(unit_arg),
        thd(thd_arg),
        strict_handler(
            Strict_error_handler::ENABLE_SET_SELECT_STRICT_ERROR_HANDLER),
        flags(0),
        row_count(0),
        table(nullptr),
        cached_file(nullptr) {
    TRASH(row_ref, sizeof(row_ref));
  }

  bool initialize(TABLE *table_arg) {
    if (!unit->is_recursive()) return false;

    /*
      For RECURSIVE, beginners will forget that:
      - the CTE's column types are defined by the non-recursive member
      - which implies that recursive member's selected expressions are cast to
      the non-recursive member's type.
      That will cause silent truncation and possibly an infinite recursion due
      to a condition like: 'LENGTH(growing_col) < const', or,
      'growing_col < const',
      which is always satisfied due to truncation.

      This situation is similar to
      create table t select "x" as a;
      insert into t select concat("x",a) from t;
      which sends ER_DATA_TOO_LONG in strict mode.

      So we should inform the user.

      If we only raised warnings: it will not interrupt an infinite recursion,
      a MAX_RECURSION hint (if we featured one) may interrupt; but then the
      warnings won't be seen, as the interruption will raise an error. So
      warnings are useless.
      Instead, we send a truncation error: it is visible, indicates the
      source of the problem, and is consistent with the INSERT case above.

      Usually, truncation in SELECT triggers an error only in
      strict mode; but if we don't send an error we get a runaway query;
      and as WITH RECURSIVE is a new feature we don't have to carry the
      permissiveness of the past, so we send an error even if in non-strict
      mode.

      For a non-recursive UNION, truncation shouldn't happen as all UNION
      members participated in type calculation.
    */
    if (thd->is_strict_mode()) {
      flags |= POP_HANDLER;
      save_check_for_truncated_fields = thd->check_for_truncated_fields;
      thd->check_for_truncated_fields = CHECK_FIELD_WARN;
      thd->push_internal_handler(&strict_handler);
    }

    for (SELECT_LEX *sl = unit->first_recursive; sl; sl = sl->next_select()) {
      TABLE_LIST *tl = sl->recursive_reference;
      DBUG_ASSERT(tl && tl->table &&
                  // returns rows in insertion order:
                  tl->table->s->primary_key == MAX_KEY);
      if (open_tmp_table(tl->table)) return true; /* purecov: inspected */
    }
    unit->got_all_recursive_rows = false;
    table = table_arg;
    return false;
  }

  /// @returns Query block to execute first, in current phase
  SELECT_LEX *first_select() const {
    return (flags & EXEC_RECURSIVE) ? unit->first_recursive
                                    : unit->first_select();
  }

  /// @returns Query block to execute last, in current phase
  SELECT_LEX *last_select() const {
    return (flags & EXEC_RECURSIVE) ? nullptr : unit->first_recursive;
  }

  /// @returns true if more iterations are needed
  bool more_iterations() {
    if (!unit->is_recursive()) return false;

    ha_rows new_row_count = *unit->query_result()->row_count();
    if (row_count == new_row_count) {
      // nothing new
      if (unit->got_all_recursive_rows)
        return false;  // The final iteration is done.
      unit->got_all_recursive_rows = true;
      /*
        Do a final iteration, just to get table free-ing/unlocking. But skip
        non-recursive query blocks as they have already done that.
      */
      flags |= EXEC_RECURSIVE;
      return true;
    }

#ifdef ENABLED_DEBUG_SYNC
    if (unit->first_select()->next_select()->join->recursive_iteration_count ==
        4) {
      DEBUG_SYNC(thd, "in_WITH_RECURSIVE");
    }
#endif

    row_count = new_row_count;
    Opt_trace_context &trace = thd->opt_trace;
    /*
      If recursive query blocks have been executed at least once, and repeated
      executions should not be traced, disable tracing, unless it already is
      disabled.
    */
    if ((flags & (EXEC_RECURSIVE | DISABLED_TRACE)) == EXEC_RECURSIVE &&
        !trace.feature_enabled(Opt_trace_context::REPEATED_SUBSELECT)) {
      flags |= DISABLED_TRACE;
      trace.disable_I_S_for_this_and_children();
    }

    flags |= EXEC_RECURSIVE;

    return true;
  }

  /**
    fake_select_lex is going to read rows which appeared since the previous
    pass. So it needs to re-establish the scan where it had left.
  */
  bool prepare_for_scan() {
    if (cached_file == nullptr) return false;
    int error;
    if (cached_file == table->file) {
      DBUG_ASSERT(!cached_file->inited);
      error = cached_file->ha_rnd_init(false);
      DBUG_ASSERT(!error);
      error = cached_file->ha_rnd_pos(table->record[0], row_ref);
    } else {
      // Since last pass of reads, MEMORY changed to InnoDB:
      QEP_TAB *qep_tab = table->reginfo.qep_tab;
      error = reposition_innodb_cursor(table, qep_tab->m_fetched_rows);
    }
    DBUG_ASSERT(!error);
    return error;
  }

  /**
    After fake_select_lex has done a pass of reading 'table', control will
    soon go to recursive query blocks which may write to 'table', thus we save
    the read-cursor's position (necessary to re-establish the scan at next
    pass), then close the cursor (necessary to allow writes).
    A tidy approach like this is necessary: with a single 'handler', an open
    read-cursor cannot survive writes (example: in MEMORY, read-cursor
    position is 'info->current_ptr' (see heap_scan()) and heap_write()
    changes it).
  */
  bool save_scan_position() {
    if (!unit->is_recursive()) return false;
    if (!table->file->inited) {
      // Scan is not initialized if seed SELECT returned empty result
      cached_file = nullptr;
      return false;
    }
    cached_file = table->file;
    cached_file->position(table->record[0]);
    DBUG_ASSERT(sizeof(row_ref) >= cached_file->ref_length);
    memcpy(row_ref, cached_file->ref, cached_file->ref_length);
    int error = cached_file->ha_rnd_end();
    DBUG_ASSERT(!error);
    return error;
  }

  ~Recursive_executor() {
    if (unit->is_recursive()) {
      if (flags & DISABLED_TRACE) thd->opt_trace.restore_I_S();
      if (flags & POP_HANDLER) {
        thd->pop_internal_handler();
        thd->check_for_truncated_fields = save_check_for_truncated_fields;
      }
    }
  }
};

/**
  Execute a query expression that may be a UNION and/or have an ordered result.

  @param thd          thread handle

  @returns false if success, true if error
*/

bool SELECT_LEX_UNIT::execute(THD *thd) {
  DBUG_ENTER("SELECT_LEX_UNIT::exec");
  DBUG_ASSERT(!is_simple() && is_optimized());

  if (is_executed() && !uncacheable) DBUG_RETURN(false);

  /*
    Even if we return "true" the statement might continue
    (e.g. ER_SUBQUERY_1_ROW in stmt with IGNORE), so we want to restore
    current_select():
  */
  Change_current_select save_select(thd);

  if (is_executed()) {
    for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select()) {
      if (sl->join->is_executed()) {
        thd->lex->set_current_select(sl);
        sl->join->reset();
      }
      if (fake_select_lex != nullptr) {
        thd->lex->set_current_select(fake_select_lex);
        fake_select_lex->join->reset();
      }
    }
  }

  // Set "executed" state, even though execution may end with an error
  set_executed();

  if (item) {
    item->reset_value_registration();

    if (item->assigned()) {
      item->assigned(false);  // Prepare for re-execution of this unit
      item->reset();
      if (table->is_created()) {
        table->file->ha_delete_all_rows();
        table->file->info(HA_STATUS_VARIABLE);
      }
    }
    // re-enable indexes for next subquery execution
    if (union_distinct && table->file->ha_enable_indexes(HA_KEY_SWITCH_ALL))
      DBUG_RETURN(true); /* purecov: inspected */
  }

  Recursive_executor recursive_executor(this, thd);
  if (recursive_executor.initialize(table))
    DBUG_RETURN(true); /* purecov: inspected */

  bool status = false;  // Execution error status

  do {
    for (auto sl = recursive_executor.first_select();
         sl != recursive_executor.last_select(); sl = sl->next_select()) {
      thd->lex->set_current_select(sl);

      // Set limit and offset for each execution:
      if (set_limit(thd, sl)) DBUG_RETURN(true); /* purecov: inspected */

      // Execute this query block
      sl->join->exec();
      status = sl->join->error != 0;

      if (sl == union_distinct && sl->next_select()) {
        // This is UNION DISTINCT, so there should be a fake_select_lex
        DBUG_ASSERT(fake_select_lex != NULL);
        if (table->file->ha_disable_indexes(HA_KEY_SWITCH_ALL))
          DBUG_RETURN(true); /* purecov: inspected */
        table->no_keyread = 1;
      }

      if (status) DBUG_RETURN(true);

      if (union_result->flush()) DBUG_RETURN(true); /* purecov: inspected */
    }

    if (fake_select_lex != NULL) {
      thd->lex->set_current_select(fake_select_lex);
      if (table->hash_field)  // Prepare for access method of JOIN::exec
        table->file->ha_index_or_rnd_end();
      if (set_limit(thd, fake_select_lex))
        DBUG_RETURN(true); /* purecov: inspected */
      JOIN *join = fake_select_lex->join;
      if (recursive_executor.prepare_for_scan())
        DBUG_RETURN(true); /* purecov: inspected */
      join->exec();
      status = join->error != 0;
      if (status) DBUG_RETURN(true);
      if (recursive_executor.save_scan_position())
        DBUG_RETURN(true);    /* purecov: inspected */
      if (table->hash_field)  // Prepare for duplicate elimination
        table->file->ha_index_init(0, false);
    }

  } while (recursive_executor.more_iterations());

  if (fake_select_lex) {
    fake_select_lex->table_list.empty();
    int error = table->file->info(HA_STATUS_VARIABLE);
    if (error) {
      table->file->print_error(error, MYF(0)); /* purecov: inspected */
      DBUG_RETURN(true);                       /* purecov: inspected */
    }
    thd->current_found_rows = (ulonglong)table->file->stats.records;
  }

  DBUG_RETURN(status);
}

/**
  Cleanup this query expression object after preparation or one round
  of execution. After the cleanup, the object can be reused for a
  new round of execution, but a new optimization will be needed before
  the execution.

  @return false if previous execution was successful, and true otherwise
*/

bool SELECT_LEX_UNIT::cleanup(bool full) {
  DBUG_ENTER("SELECT_LEX_UNIT::cleanup");

  DBUG_ASSERT(thd == current_thd);

  if (cleaned >= (full ? UC_CLEAN : UC_PART_CLEAN)) DBUG_RETURN(false);

  cleaned = (full ? UC_CLEAN : UC_PART_CLEAN);

  bool error = false;
  for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select())
    error |= sl->cleanup(full);

  if (fake_select_lex) {
    /*
      Normally done at end of evaluation, but not if there was an
      error:
    */
    fake_select_lex->table_list.empty();
    fake_select_lex->recursive_reference = nullptr;
    error |= fake_select_lex->cleanup(full);
  }

  // fake_select_lex's table depends on Temp_table_param inside union_result
  if (full && union_result) {
    union_result->cleanup();
    destroy(union_result);
    union_result = NULL;  // Safety
    if (table) free_tmp_table(thd, table);
    table = NULL;  // Safety
  }

  /*
    explain_marker is (mostly) a property determined at prepare time and must
    thus be preserved for the next execution, if this is a prepared statement.
  */

  DBUG_RETURN(error);
}

#ifndef DBUG_OFF
void SELECT_LEX_UNIT::assert_not_fully_clean() {
  DBUG_ASSERT(cleaned < UC_CLEAN);
  SELECT_LEX *sl = first_select();
  for (;;) {
    if (!sl) {
      sl = fake_select_lex;
      if (!sl) break;
    }
    for (SELECT_LEX_UNIT *lex_unit = sl->first_inner_unit(); lex_unit;
         lex_unit = lex_unit->next_unit())
      lex_unit->assert_not_fully_clean();
    if (sl == fake_select_lex)
      break;
    else
      sl = sl->next_select();
  }
}
#endif

void SELECT_LEX_UNIT::reinit_exec_mechanism() {
  prepared = optimized = executed = false;
#ifndef DBUG_OFF
  if (is_union()) {
    List_iterator_fast<Item> it(item_list);
    Item *field;
    while ((field = it++)) {
      /*
        we can't cleanup here, because it broke link to temporary table field,
        but have to drop fixed flag to allow next fix_field of this field
        during re-executing
      */
      field->fixed = 0;
    }
  }
#endif
}

/**
  Change the query result object used to return the final result of
  the unit, replacing occurences of old_result with new_result.

  @param new_result New query result object
  @param old_result Old query result object

  @retval false Success
  @retval true  Error
*/

bool SELECT_LEX_UNIT::change_query_result(
    Query_result_interceptor *new_result,
    Query_result_interceptor *old_result) {
  for (SELECT_LEX *sl = first_select(); sl; sl = sl->next_select()) {
    if (sl->query_result() && sl->change_query_result(new_result, old_result))
      return true; /* purecov: inspected */
  }
  set_query_result(new_result);
  return false;
}

/**
  Get column type information for this query expression.

  For a single query block the column types are taken from the list
  of selected items of this block.

  For a union this function assumes that SELECT_LEX_UNIT::prepare()
  has been called and returns the type holders that were created for unioned
  column types of all query blocks.

  @note
    The implementation of this function should be in sync with
    SELECT_LEX_UNIT::prepare()

  @returns List of items as specified in function description
*/

List<Item> *SELECT_LEX_UNIT::get_unit_column_types() {
  DBUG_ASSERT(is_prepared());

  return is_union() ? &types : &first_select()->item_list;
}

/**
  Get field list for this query expression.

  For a UNION of query blocks, return the field list generated during prepare.
  For a single query block, return the field list after all possible
  intermediate query processing steps are done (optimization is complete).

  @returns List containing fields of the query expression.
*/

List<Item> *SELECT_LEX_UNIT::get_field_list() {
  DBUG_ASSERT(is_optimized());

  return is_union() ? &types : first_select()->join->fields;
}

const Query_result *SELECT_LEX_UNIT::recursive_result(
    SELECT_LEX *reader) const {
  DBUG_ASSERT(reader->master_unit() == this && reader->is_recursive());
  if (reader == fake_select_lex)
    return union_result;
  else
    return m_query_result;
}

bool SELECT_LEX_UNIT::mixed_union_operators() const {
  return union_distinct && union_distinct->next_select();
}

/**
   Closes (and, if last reference, drops) temporary tables created to
   materialize derived tables, schema tables and CTEs.

   @param thd  Thread handler
   @param list List of tables to search in
*/
static void destroy_materialized(THD *thd, TABLE_LIST *list) {
  for (auto tl = list; tl; tl = tl->next_local) {
    if (tl->merge_underlying_list) {
      // Find a materialized view inside another view.
      destroy_materialized(thd, tl->merge_underlying_list);
    } else if (tl->is_table_function()) {
      tl->table_function->cleanup();
    }
    if (tl->table == nullptr) continue;  // Not materialized
    if (tl->is_view_or_derived()) {
      tl->reset_name_temporary();
      if (tl->common_table_expr()) tl->common_table_expr()->tmp_tables.clear();
    } else if (!tl->is_recursive_reference() && !tl->schema_table &&
               !tl->is_table_function())
      continue;
    free_tmp_table(thd, tl->table);
    tl->table = nullptr;
  }
}

/**
  Cleanup after preparation or one round of execution.

  @return false if previous execution was successful, and true otherwise
*/

bool SELECT_LEX::cleanup(bool full) {
  DBUG_ENTER("SELECT_LEX::cleanup()");

  bool error = false;
  if (join) {
    if (full) {
      DBUG_ASSERT(join->select_lex == this);
      error = join->destroy();
      destroy(join);
      join = NULL;
    } else
      join->cleanup();
  }

  THD *const thd = master_unit()->thd;

  if (full) destroy_materialized(thd, get_table_list());

  for (SELECT_LEX_UNIT *lex_unit = first_inner_unit(); lex_unit;
       lex_unit = lex_unit->next_unit()) {
    error |= lex_unit->cleanup(full);
  }
  inner_refs_list.empty();

  if (full && m_windows.elements > 0) {
    List_iterator<Window> li(m_windows);
    Window *w;
    while ((w = li++)) w->cleanup(thd);
  }

  DBUG_RETURN(error);
}

void SELECT_LEX::cleanup_all_joins() {
  if (join) join->cleanup();

  for (SELECT_LEX_UNIT *unit = first_inner_unit(); unit;
       unit = unit->next_unit()) {
    for (SELECT_LEX *sl = unit->first_select(); sl; sl = sl->next_select())
      sl->cleanup_all_joins();
  }
}

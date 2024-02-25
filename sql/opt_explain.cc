/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
  @file sql/opt_explain.cc
  "EXPLAIN <command>" implementation.
*/

#include "sql/opt_explain.h"

#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "ft_global.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_double2ulonglong.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/derror.h"      // ER_THD
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_subselect.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/key.h"
#include "sql/mysqld.h"              // stage_explaining
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/opt_costmodel.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"  // Opt_trace_*
#include "sql/parse_tree_node_base.h"
#include "sql/protocol.h"
#include "sql/query_term.h"
#include "sql/range_optimizer/group_index_skip_scan_plan.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // is_explainable_query
#include "sql/sql_partition.h"  // for make_used_partitions_str()
#include "sql/sql_select.h"
#include "sql/table.h"
#include "sql/table_function.h"  // Table_function
#include "sql/visible_fields.h"
#include "sql_string.h"
#include "template_utils.h"

class Opt_trace_context;

using std::function;
using std::string;
using std::vector;

typedef qep_row::extra extra;

static bool mysql_explain_query_expression(THD *explain_thd,
                                           const THD *query_thd,
                                           Query_expression *unit);

const char *join_type_str[] = {
    "UNKNOWN", "system", "const",    "eq_ref",      "ref",        "ALL",
    "range",   "index",  "fulltext", "ref_or_null", "index_merge"};

static const enum_query_type cond_print_flags =
    enum_query_type(QT_ORDINARY | QT_SHOW_SELECT_NUMBER);

/// First string: for regular EXPLAIN; second: for EXPLAIN CONNECTION
static const char *plan_not_ready[] = {"Not optimized, outer query is empty",
                                       "Plan isn't ready yet"};

static bool ExplainIterator(THD *ethd, const THD *query_thd,
                            Query_expression *unit);

namespace {

/**
  A base for all Explain_* classes

  Explain_* classes collect and output EXPLAIN data.

  This class hierarchy is a successor of the old select_describe() function
  of 5.5.
*/

class Explain {
 protected:
  THD *const explain_thd;        ///< cached THD which runs the EXPLAIN command
  const THD *query_thd;          ///< THD which runs the query to be explained
  const CHARSET_INFO *const cs;  ///< cached pointer to system_charset_info
  /**
     Cached Query_block of the explained query. Used for all explained stmts,
     including single-table UPDATE (provides way to access ORDER BY of
     UPDATE).
  */
  Query_block *const query_block;

  Explain_format *const fmt;          ///< shortcut for thd->lex->explain_format
  enum_parsing_context context_type;  ///< associated value for struct. explain

  bool order_list;  ///< if query block has ORDER BY

  const bool explain_other;  ///< if we explain other thread than us

 protected:
  class Lazy_condition : public Lazy {
    Item *const condition;

   public:
    Lazy_condition(Item *condition_arg) : condition(condition_arg) {}
    bool eval(String *ret) override {
      ret->length(0);
      if (condition) condition->print(current_thd, ret, cond_print_flags);
      return false;
    }
  };

  explicit Explain(enum_parsing_context context_type_arg, THD *explain_thd_arg,
                   const THD *query_thd_arg, Query_block *query_block_arg)
      : explain_thd(explain_thd_arg),
        query_thd(query_thd_arg),
        cs(system_charset_info),
        query_block(query_block_arg),
        fmt(explain_thd->lex->explain_format),
        context_type(context_type_arg),
        order_list(false),
        explain_other(explain_thd != query_thd) {
    if (explain_other) query_thd->query_plan.assert_plan_is_locked_if_other();
  }

 public:
  virtual ~Explain() = default;

  bool send();

  /**
     Tells if it is allowed to print the WHERE / GROUP BY / etc
     clauses.
  */
  bool can_print_clauses() const {
    /*
      Certain implementations of Item::print() modify the item, so cannot be
      called by another thread which does not own the item. Moreover, the
      owning thread may be modifying the item at this moment (example:
      Item_in_subselect::finalize_materialization_transform() is done
      at first execution of the subquery, which happens after the parent query
      has a plan, and affects how the parent query would be printed).
    */
    return !explain_other;
  }

 protected:
  /**
    Explain everything but subqueries
  */
  virtual bool shallow_explain();
  /**
    Explain the rest of things after the @c shallow_explain() call
  */
  bool explain_subqueries();
  bool mark_subqueries(Item *item, qep_row *destination);
  bool prepare_columns();

  /**
    Push a part of the "extra" column into formatter

    Traditional formatter outputs traditional_extra_tags[tag] as is.
    Hierarchical formatter outputs a property with the json_extra_tags[tag] name
    and a boolean value of true.

    @param      tag     type of the "extra" part

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag) {
    extra *e = new (explain_thd->mem_root) extra(tag);
    return e == nullptr || fmt->entry()->col_extra.push_back(e);
  }

  /**
    Push a part of the "extra" column into formatter

    @param      tag     type of the "extra" part
    @param      arg     for traditional formatter: rest of the part text,
                        for hierarchical format: string value of the property

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag, const String &arg) {
    if (arg.is_empty()) return push_extra(tag);
    extra *e =
        new (explain_thd->mem_root) extra(tag, arg.dup(explain_thd->mem_root));
    return !e || !e->data || fmt->entry()->col_extra.push_back(e);
  }

  /**
    Push a part of the "extra" column into formatter

    @param      tag     type of the "extra" part
    @param      arg     for traditional formatter: rest of the part text,
                        for hierarchical format: string value of the property

    NOTE: arg must be a long-living string constant.

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag, const char *arg) {
    extra *e = new (explain_thd->mem_root) extra(tag, arg);
    return !e || fmt->entry()->col_extra.push_back(e);
  }

  /*
    Rest of the functions are overloadable functions, those calculate and fill
    "col_*" fields with Items for further sending as EXPLAIN columns.

    "explain_*" functions return false on success and true on error (usually
    OOM).
  */
  virtual bool explain_id();
  virtual bool explain_select_type();
  virtual bool explain_table_name() { return false; }
  virtual bool explain_partitions() { return false; }
  virtual bool explain_join_type() { return false; }
  virtual bool explain_possible_keys() { return false; }
  /** fill col_key and and col_key_len fields together */
  virtual bool explain_key_and_len() { return false; }
  virtual bool explain_ref() { return false; }
  /** fill col_rows and col_filtered fields together */
  virtual bool explain_rows_and_filtered() { return false; }
  virtual bool explain_extra() { return false; }
  virtual bool explain_modify_flags() { return false; }

 protected:
  /**
     Returns true if the WHERE, ORDER BY, GROUP BY, etc clauses can safely be
     traversed: it means that we can iterate through them (no element is
     added/removed/replaced); the internal details of an element can change
     though (in particular if that element is an Item_subselect).

     By default, if we are explaining another connection, this is not safe.
  */
  virtual bool can_walk_clauses() { return !explain_other; }
  virtual enum_parsing_context get_subquery_context(
      Query_expression *unit) const;

 private:
  /**
    Returns true if EXPLAIN should not produce any information about subqueries.
   */
  virtual bool skip_subqueries() const { return false; }
};

enum_parsing_context Explain::get_subquery_context(
    Query_expression *unit) const {
  return unit->get_explain_marker(query_thd);
}

/**
  Explain_no_table class outputs a trivial EXPLAIN row with "extra" column

  This class is intended for simple cases to produce EXPLAIN output
  with "No tables used", "No matching records" etc.
  Optionally it can output number of estimated rows in the "row"
  column.

  @note This class also produces EXPLAIN rows for inner units (if any).
*/

class Explain_no_table : public Explain {
 private:
  const char *message;  ///< cached "message" argument
  const ha_rows rows;   ///< HA_POS_ERROR or cached "rows" argument

 public:
  Explain_no_table(THD *explain_thd_arg, const THD *query_thd_arg,
                   Query_block *query_block_arg, const char *message_arg,
                   enum_parsing_context context_type_arg = CTX_JOIN,
                   ha_rows rows_arg = HA_POS_ERROR)
      : Explain(context_type_arg, explain_thd_arg, query_thd_arg,
                query_block_arg),
        message(message_arg),
        rows(rows_arg) {
    if (can_walk_clauses())
      order_list = (query_block_arg->order_list.elements != 0);
  }

 protected:
  bool shallow_explain() override;

  bool explain_rows_and_filtered() override;
  bool explain_extra() override;
  bool explain_modify_flags() override;

 private:
  enum_parsing_context get_subquery_context(
      Query_expression *unit) const override;
};

/**
  Explain_union_result class outputs EXPLAIN row for UNION
*/

class Explain_setop_result : public Explain {
 public:
  Explain_setop_result(THD *explain_thd_arg, const THD *query_thd_arg,
                       Query_block *query_block_arg, Query_term *qt,
                       enum_parsing_context ctx)
      : Explain(ctx, explain_thd_arg, query_thd_arg, query_block_arg),
        m_query_term(down_cast<Query_term_set_op *>(qt)) {
    assert(m_query_term->term_type() != QT_QUERY_BLOCK);
    // Use optimized values from block's join
    order_list = !query_block_arg->join->order.empty();
    // A plan exists so the reads above are safe:
    assert(query_block_arg->join->get_plan_state() != JOIN::NO_PLAN);
  }

 protected:
  bool explain_id() override;
  bool explain_table_name() override;
  bool explain_join_type() override;
  bool explain_extra() override;
  /* purecov: begin deadcode */
  bool can_walk_clauses() override {
    assert(0);    // UNION result can't have conditions
    return true;  // Because we know that we have a plan
  }
  /* purecov: end */
  Query_term_set_op *m_query_term;
};

/**
  Common base class for Explain_join and Explain_table
*/

class Explain_table_base : public Explain {
 protected:
  /**
     The QEP_TAB which we are currently explaining. It is NULL for the
     inserted table in INSERT/REPLACE SELECT, and single-table UPDATE/DELETE.
     @note that you should never read quick() or condition() even for SELECT,
     they may change under your feet without holding the mutex;
     read quick and condition in this class instead.
  */
  QEP_TAB *tab{nullptr};

  const TABLE *table{nullptr};
  join_type type{JT_UNKNOWN};
  AccessPath *range_scan_path{nullptr};
  Item *condition{nullptr};
  bool dynamic_range{false};
  Table_ref *table_ref{nullptr};
  bool skip_records_in_range{false};
  bool reversed_access{false};

  Key_map usable_keys;

  Explain_table_base(enum_parsing_context context_type_arg,
                     THD *const explain_thd_arg, const THD *query_thd_arg,
                     Query_block *query_block_arg = nullptr,
                     TABLE *const table_arg = nullptr)
      : Explain(context_type_arg, explain_thd_arg, query_thd_arg,
                query_block_arg),
        table(table_arg) {}

  bool explain_partitions() override;
  bool explain_possible_keys() override;

  bool explain_key_parts(int key, uint key_parts);
  bool explain_key_and_len_quick(AccessPath *range_scan);
  bool explain_key_and_len_index(int key);
  bool explain_key_and_len_index(int key, uint key_length, uint key_parts);
  bool explain_extra_common(int range_scan_type, uint keyno);
  bool explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                     bool need_sort_arg);
};

/**
  Explain_join class produces EXPLAIN output for JOINs
*/

class Explain_join : public Explain_table_base {
 private:
  bool need_tmp_table;  ///< add "Using temporary" to "extra" if true
  bool need_order;      ///< add "Using filesort"" to "extra" if true
  const bool distinct;  ///< add "Distinct" string to "extra" column if true

  JOIN *join;           ///< current JOIN
  int range_scan_type;  ///< current range scan type, really an AccessPath::Type

 public:
  Explain_join(THD *explain_thd_arg, const THD *query_thd_arg,
               Query_block *query_block_arg, bool need_tmp_table_arg,
               bool need_order_arg, bool distinct_arg)
      : Explain_table_base(CTX_JOIN, explain_thd_arg, query_thd_arg,
                           query_block_arg),
        need_tmp_table(need_tmp_table_arg),
        need_order(need_order_arg),
        distinct(distinct_arg),
        join(query_block_arg->join) {
    assert(join->get_plan_state() == JOIN::PLAN_READY);
    order_list = !join->order.empty();
  }

 private:
  // Next 4 functions begin and end context for GROUP BY, ORDER BY and DISTINC
  bool begin_sort_context(Explain_sort_clause clause, enum_parsing_context ctx);
  bool end_sort_context(Explain_sort_clause clause, enum_parsing_context ctx);
  bool begin_simple_sort_context(Explain_sort_clause clause,
                                 enum_parsing_context ctx);
  bool end_simple_sort_context(Explain_sort_clause clause,
                               enum_parsing_context ctx);
  bool explain_qep_tab(size_t tab_num);

 protected:
  bool shallow_explain() override;

  bool explain_table_name() override;
  bool explain_join_type() override;
  bool explain_key_and_len() override;
  bool explain_ref() override;
  bool explain_rows_and_filtered() override;
  bool explain_extra() override;
  bool explain_select_type() override;
  bool explain_id() override;
  bool explain_modify_flags() override;
  bool can_walk_clauses() override {
    return true;  // Because we know that we have a plan
  }
};

/**
  Explain_table class produce EXPLAIN output for queries without top-level JOIN

  This class is a simplified version of the Explain_join class. It works in the
  context of queries which implementation lacks top-level JOIN object (EXPLAIN
  single-table UPDATE and DELETE).
*/

class Explain_table : public Explain_table_base {
 private:
  const uint key;                   ///< cached "key" number argument
  const ha_rows limit;              ///< HA_POS_ERROR or cached "limit" argument
  const bool need_tmp_table;        ///< cached need_tmp_table argument
  const bool need_sort;             ///< cached need_sort argument
  const enum_mod_type mod_type;     ///< Table modification type
  const bool used_key_is_modified;  ///< UPDATE command updates used key
  const char *message;              ///< cached "message" argument

 public:
  Explain_table(THD *const explain_thd_arg, const THD *query_thd_arg,
                Query_block *query_block_arg, TABLE *const table_arg,
                enum join_type type_arg, AccessPath *range_scan_arg,
                Item *condition_arg, uint key_arg, ha_rows limit_arg,
                bool need_tmp_table_arg, bool need_sort_arg,
                enum_mod_type mod_type_arg, bool used_key_is_modified_arg,
                const char *msg)
      : Explain_table_base(CTX_JOIN, explain_thd_arg, query_thd_arg,
                           query_block_arg, table_arg),
        key(key_arg),
        limit(limit_arg),
        need_tmp_table(need_tmp_table_arg),
        need_sort(need_sort_arg),
        mod_type(mod_type_arg),
        used_key_is_modified(used_key_is_modified_arg),
        message(msg) {
    type = type_arg;
    range_scan_path = range_scan_arg;
    condition = condition_arg;
    usable_keys = table->possible_quick_keys;
    if (can_walk_clauses())
      order_list = (query_block_arg->order_list.elements != 0);
  }

  bool explain_modify_flags() override;

 private:
  virtual bool explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                             bool need_sort_arg);
  bool shallow_explain() override;

  bool explain_ref() override;
  bool explain_table_name() override;
  bool explain_join_type() override;
  bool explain_key_and_len() override;
  bool explain_rows_and_filtered() override;
  bool explain_extra() override;

  bool can_walk_clauses() override {
    return true;  // Because we know that we have a plan
  }
};

/**
  This class outputs an empty plan for queries that use a secondary engine. It
  is only used with the hypergraph optimizer, and only when the traditional
  format is specified. The traditional format is not supported by the hypergraph
  optimizer, so only an empty plan is shown, with extra information showing a
  secondary engine is used.
 */
class Explain_secondary_engine final : public Explain {
 public:
  Explain_secondary_engine(THD *explain_thd_arg, const THD *query_thd_arg,
                           Query_block *query_block_arg)
      : Explain(CTX_JOIN, explain_thd_arg, query_thd_arg, query_block_arg) {}

 protected:
  bool explain_select_type() override {
    fmt->entry()->col_select_type.set(enum_explain_type::EXPLAIN_NONE);
    return false;
  }

  bool explain_extra() override {
    StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;
    bool error = false;
    error |= buffer.append(STRING_WITH_LEN("Using secondary engine "));
    error |= buffer.append(
        ha_resolve_storage_engine_name(SecondaryEngineHandlerton(query_thd)));
    error |= buffer.append(
        STRING_WITH_LEN(". Use EXPLAIN FORMAT=TREE to show the plan."));
    if (error) return error;
    return fmt->entry()->col_message.set(buffer);
  }

 private:
  bool skip_subqueries() const override { return true; }
};

}  // namespace

/* Explain class functions ****************************************************/

bool Explain::shallow_explain() {
  return prepare_columns() || fmt->flush_entry();
}

/**
  Qualify subqueries with WHERE/HAVING/ORDER BY/GROUP BY clause type marker

  @param item           Item tree to find subqueries
  @param destination    For WHERE clauses

  @note WHERE clause belongs to TABLE or QEP_TAB. The @c destination parameter
        provides a pointer to QEP data for such a table to associate a future
        subquery EXPLAIN output with table QEP provided.

  @retval false         OK
  @retval true          Error
*/

bool Explain::mark_subqueries(Item *item, qep_row *destination) {
  if (skip_subqueries() || item == nullptr || !fmt->is_hierarchical())
    return false;

  item->compile(&Item::explain_subquery_checker,
                reinterpret_cast<uchar **>(&destination),
                &Item::explain_subquery_propagator, nullptr);
  return false;
}

static bool explain_ref_key(Explain_format *fmt, uint key_parts,
                            store_key *key_copy[]) {
  if (key_parts == 0) return false;

  for (uint part_no = 0; part_no < key_parts; part_no++) {
    const store_key *const s_key = key_copy[part_no];
    if (s_key == nullptr) {
      // Const keys don't need to be copied
      if (fmt->entry()->col_ref.push_back(STORE_KEY_CONST_NAME))
        return true; /* purecov: inspected */
    } else if (fmt->entry()->col_ref.push_back(s_key->name()))
      return true; /* purecov: inspected */
  }
  return false;
}

enum_parsing_context Explain_no_table::get_subquery_context(
    Query_expression *unit) const {
  const enum_parsing_context context = Explain::get_subquery_context(unit);
  if (context == CTX_OPTIMIZED_AWAY_SUBQUERY) return context;
  if (context == CTX_DERIVED)
    return context;
  else if (message != plan_not_ready[explain_other])
    /*
      When zero result is given all subqueries are considered as optimized
      away.
    */
    return CTX_OPTIMIZED_AWAY_SUBQUERY;
  return context;
}

/**
  Traverses SQL clauses of this query specification to identify children
  subqueries, marks each of them with the clause they belong to.
  Then goes though all children subqueries and produces their EXPLAIN
  output, attached to the proper clause's context.

  @retval       false   Ok
  @retval       true    Error (OOM)
*/
bool Explain::explain_subqueries() {
  if (skip_subqueries()) return false;

  /*
    Subqueries in empty queries are neither optimized nor executed. They are
    therefore not to be included in the explain output.
  */
  if (query_block->is_empty_query()) return false;

  for (Query_expression *unit = query_block->first_inner_query_expression();
       unit; unit = unit->next_query_expression()) {
    Query_block *sl = unit->first_query_block();
    enum_parsing_context context = get_subquery_context(unit);
    if (context == CTX_NONE) context = CTX_OPTIMIZED_AWAY_SUBQUERY;

    uint derived_clone_id = 0;
    bool is_derived_clone = false;
    if (context == CTX_DERIVED) {
      Table_ref *tl = unit->derived_table;
      derived_clone_id = tl->query_block_id_for_explain();
      assert(derived_clone_id);
      is_derived_clone = derived_clone_id != tl->query_block_id();
      if (is_derived_clone && !fmt->is_hierarchical()) {
        // Don't show underlying tables of derived table clone
        continue;
      }
    }

    if (fmt->begin_context(context, unit)) return true;

    if (is_derived_clone) fmt->entry()->derived_clone_id = derived_clone_id;

    if (mysql_explain_query_expression(explain_thd, query_thd, unit))
      return true;

    /*
      This must be after mysql_explain_query_expression() so that
      JOIN::optimize() has run and had a chance to choose materialization.
    */
    if (fmt->is_hierarchical() &&
        (context == CTX_WHERE || context == CTX_HAVING ||
         context == CTX_SELECT_LIST || context == CTX_GROUP_BY_SQ ||
         context == CTX_ORDER_BY_SQ) &&
        (!explain_other ||
         (sl->join && sl->join->get_plan_state() != JOIN::NO_PLAN)) &&
        // Check below requires complete plan
        unit->item &&
        (unit->item->engine_type() == Item_subselect::HASH_SJ_ENGINE)) {
      fmt->entry()->is_materialized_from_subquery = true;
      fmt->entry()->col_table_name.set_const("<materialized_subquery>");
      fmt->entry()->using_temporary = true;

      fmt->entry()->col_join_type.set_const(
          join_type_str[unit->item->get_join_type()]);
      fmt->entry()->col_key.set_const("<auto_key>");

      char buff_key_len[24];
      fmt->entry()->col_key_len.set(
          buff_key_len,
          longlong10_to_str(unit->item->get_table()->key_info[0].key_length,
                            buff_key_len, 10) -
              buff_key_len);

      const Index_lookup &ref = unit->item->index_lookup();
      if (explain_ref_key(fmt, ref.key_parts, ref.key_copy)) return true;

      fmt->entry()->col_rows.set(1);
      /*
       The value to look up depends on the outer value, so the materialized
       subquery is dependent and not cacheable:
      */
      fmt->entry()->is_dependent = true;
      fmt->entry()->is_cacheable = false;
    }

    if (fmt->end_context(context)) return true;
  }
  return false;
}

/**
  Pre-calculate table property values for further EXPLAIN output
*/
bool Explain::prepare_columns() {
  return explain_id() || explain_select_type() || explain_table_name() ||
         explain_partitions() || explain_join_type() ||
         explain_possible_keys() || explain_key_and_len() || explain_ref() ||
         explain_modify_flags() || explain_rows_and_filtered() ||
         explain_extra();
}

/**
  Explain class main function

  This function:
    a) allocates a Query_result_send object (if no one pre-allocated available),
    b) calculates and sends whole EXPLAIN data.

  @return false if success, true if error
*/

bool Explain::send() {
  DBUG_TRACE;

  if (fmt->begin_context(context_type, nullptr)) return true;

  /* Don't log this into the slow query log */
  explain_thd->server_status &=
      ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);

  bool ret = shallow_explain() || explain_subqueries();

  if (!ret) ret = fmt->end_context(context_type);

  return ret;
}

bool Explain::explain_id() {
  fmt->entry()->col_id.set(query_block->select_number);
  return false;
}

bool Explain::explain_select_type() {
  // ignore top-level Query_blockes
  // Elaborate only when plan is ready
  if (query_block->master_query_expression()->outer_query_block() &&
      query_block->join &&
      query_block->join->get_plan_state() != JOIN::NO_PLAN) {
    fmt->entry()->is_dependent = query_block->is_dependent();
    if (query_block->type() != enum_explain_type::EXPLAIN_DERIVED)
      fmt->entry()->is_cacheable = query_block->is_cacheable();
  }
  fmt->entry()->col_select_type.set(query_block->type());
  return false;
}

/* Explain_no_table class functions *******************************************/

bool Explain_no_table::shallow_explain() {
  return (fmt->begin_context(CTX_MESSAGE) || Explain::shallow_explain() ||
          (can_walk_clauses() &&
           mark_subqueries(query_block->where_cond(), fmt->entry())) ||
          fmt->end_context(CTX_MESSAGE));
}

bool Explain_no_table::explain_rows_and_filtered() {
  /* Don't print estimated # of rows in table for INSERT/REPLACE. */
  if (rows == HA_POS_ERROR || fmt->entry()->mod_type == MT_INSERT ||
      fmt->entry()->mod_type == MT_REPLACE)
    return false;
  fmt->entry()->col_rows.set(rows);
  return false;
}

bool Explain_no_table::explain_extra() {
  return fmt->entry()->col_message.set(message);
}

bool Explain_no_table::explain_modify_flags() {
  switch (query_thd->query_plan.get_command()) {
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_UPDATE:
      fmt->entry()->mod_type = MT_UPDATE;
      break;
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_DELETE:
      fmt->entry()->mod_type = MT_DELETE;
      break;
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_INSERT:
      fmt->entry()->mod_type = MT_INSERT;
      break;
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_REPLACE:
      fmt->entry()->mod_type = MT_REPLACE;
      break;
    default:;
  }
  return false;
}

/* Explain_union_result class functions
 * ****************************************/

bool Explain_setop_result::explain_id() { return Explain::explain_id(); }

bool Explain_setop_result::explain_table_name() {
  // Get the last of UNION's selects
  Query_block *last_query_block =
      m_query_term->m_children.back()->query_block();
  ;
  // # characters needed to print select_number of last select
  int last_length = (int)log10((double)last_query_block->select_number) + 1;

  char table_name_buffer[NAME_LEN];
  const char *op_type;
  if (context_type == CTX_UNION_RESULT) {
    op_type = "<union";
  } else if (context_type == CTX_INTERSECT_RESULT) {
    op_type = "<intersect";
  } else if (context_type == CTX_EXCEPT_RESULT) {
    op_type = "<except";
  } else {
    if (order_list) {
      if (query_block->select_limit != nullptr) {
        op_type = "<ordered/limited";
      } else {
        op_type = "<ordered";
      }
    } else if (query_block->select_limit != nullptr) {
      op_type = "<limited";
    } else {
      op_type = "<ordered";
    }
  }
  const size_t op_type_len = strlen(op_type);
  size_t lastop = 0;
  size_t len = op_type_len;
  memcpy(table_name_buffer, op_type, op_type_len);
  /*
    - len + lastop: current position in table_name_buffer
    - 6 + last_length: the number of characters needed to print
      '...,'<last_query_block->select_number>'>\0'
  */
  bool overflow = false;
  for (auto qt : m_query_term->m_children) {
    if (len + lastop + op_type_len + last_length >= NAME_CHAR_LEN) {
      overflow = true;
      break;
    }
    len += lastop;
    lastop = snprintf(table_name_buffer + len, NAME_CHAR_LEN - len, "%u,",
                      qt->query_block()->select_number);
  }

  if (overflow || len + lastop >= NAME_CHAR_LEN) {
    memcpy(table_name_buffer + len, STRING_WITH_LEN("...,"));
    len += 4;
    lastop = snprintf(table_name_buffer + len, NAME_CHAR_LEN - len, "%u,",
                      last_query_block->select_number);
  }
  len += lastop;
  table_name_buffer[len - 1] = '>';  // change ',' to '>'

  return fmt->entry()->col_table_name.set(table_name_buffer, len);
}

bool Explain_setop_result::explain_join_type() {
  fmt->entry()->col_join_type.set_const(join_type_str[JT_ALL]);
  return false;
}

bool Explain_setop_result::explain_extra() {
  if (!fmt->is_hierarchical()) {
    /*
     Currently we always use temporary table for UNION result
    */
    if (push_extra(ET_USING_TEMPORARY)) return true;
  }
  /*
    here we assume that the query will return at least two rows, so we
    show "filesort" in EXPLAIN. Of course, sometimes we'll be wrong
    and no filesort will be actually done, but executing all selects in
    the UNION to provide precise EXPLAIN information will hardly be
    appreciated :)
  */
  if (order_list) {
    return push_extra(ET_USING_FILESORT);
  }
  return Explain::explain_extra();
}

/* Explain_table_base class functions *****************************************/

bool Explain_table_base::explain_partitions() {
  if (table->part_info)
    return make_used_partitions_str(table->part_info,
                                    &fmt->entry()->col_partitions);
  return false;
}

bool Explain_table_base::explain_possible_keys() {
  if (usable_keys.is_clear_all()) return false;

  if ((table->file->ha_table_flags() & HA_NO_INDEX_ACCESS) != 0) return false;

  for (uint j = 0; j < table->s->keys; j++) {
    if (usable_keys.is_set(j) &&
        fmt->entry()->col_possible_keys.push_back(table->key_info[j].name))
      return true;
  }
  return false;
}

bool Explain_table_base::explain_key_parts(int key, uint key_parts) {
  KEY_PART_INFO *kp = table->key_info[key].key_part;
  for (uint i = 0; i < key_parts; i++, kp++)
    if (fmt->entry()->col_key_parts.push_back(
            get_field_name_or_expression(explain_thd, kp->field)))
      return true;
  return false;
}

bool Explain_table_base::explain_key_and_len_quick(AccessPath *path) {
  bool ret = false;
  StringBuffer<512> str_key(cs);
  StringBuffer<512> str_key_len(cs);

  if (used_index(path) != MAX_KEY)
    ret = explain_key_parts(used_index(range_scan_path),
                            get_used_key_parts(path));
  add_keys_and_lengths(path, &str_key, &str_key_len);
  return (ret || fmt->entry()->col_key.set(str_key) ||
          fmt->entry()->col_key_len.set(str_key_len));
}

bool Explain_table_base::explain_key_and_len_index(int key) {
  assert(key != MAX_KEY);
  return explain_key_and_len_index(key, table->key_info[key].key_length,
                                   table->key_info[key].user_defined_key_parts);
}

bool Explain_table_base::explain_key_and_len_index(int key, uint key_length,
                                                   uint key_parts) {
  assert(key != MAX_KEY);

  char buff_key_len[24];
  const KEY *key_info = table->key_info + key;
  const size_t length =
      longlong10_to_str(key_length, buff_key_len, 10) - buff_key_len;
  const bool ret = explain_key_parts(key, key_parts);
  return (ret || fmt->entry()->col_key.set(key_info->name) ||
          fmt->entry()->col_key_len.set(buff_key_len, length));
}

bool Explain_table_base::explain_extra_common(int range_scan_type, uint keyno) {
  if (keyno != MAX_KEY && keyno == table->file->pushed_idx_cond_keyno &&
      table->file->pushed_idx_cond) {
    StringBuffer<160> buff(cs);
    if (fmt->is_hierarchical() && can_print_clauses()) {
      table->file->pushed_idx_cond->print(explain_thd, &buff, cond_print_flags);
    }
    if (push_extra(ET_USING_INDEX_CONDITION, buff))
      return true; /* purecov: inspected */
  }

  const TABLE *pushed_root = table->file->member_of_pushed_join();
  if (pushed_root && query_block->join &&
      query_block->join->get_plan_state() == JOIN::PLAN_READY) {
    char buf[128];
    size_t len;
    int pushed_id = 0;
    for (QEP_TAB *prev = query_block->join->qep_tab; prev <= tab; prev++) {
      if (prev->table() == nullptr) continue;

      const TABLE *prev_root = prev->table()->file->member_of_pushed_join();
      if (prev_root == prev->table()) {
        pushed_id++;
        if (prev_root == pushed_root) break;
      }
    }
    if (pushed_root == table) {
      uint pushed_count = table->file->number_of_pushed_joins();
      len = snprintf(buf, sizeof(buf) - 1, "Parent of %d pushed join@%d",
                     pushed_count, pushed_id);
    } else {
      len = snprintf(buf, sizeof(buf) - 1, "Child of '%s' in pushed join@%d",
                     table->file->parent_of_pushed_join()->alias, pushed_id);
    }

    {
      StringBuffer<128> buff(cs);
      buff.append(buf, len);
      if (push_extra(ET_PUSHED_JOIN, buff)) return true;
    }
  }

  switch (range_scan_type) {
    case AccessPath::ROWID_UNION:
    case AccessPath::ROWID_INTERSECTION:
    case AccessPath::INDEX_MERGE: {
      StringBuffer<32> buff(cs);
      add_info_string(range_scan_path, &buff);
      if (fmt->is_hierarchical()) {
        /*
          We are replacing existing col_key value with a quickselect info,
          but not the reverse:
        */
        assert(fmt->entry()->col_key.length);
        if (fmt->entry()->col_key.set(buff))  // keep col_key_len intact
          return true;
      } else {
        if (push_extra(ET_USING, buff)) return true;
      }
    } break;
    default:;
  }

  if (table_ref && table_ref->table_function) {
    StringBuffer<64> str(cs);
    str.append(table_ref->table_function->func_name());

    if (push_extra(ET_TABLE_FUNCTION, str) || push_extra(ET_USING_TEMPORARY))
      return true;
  }
  if (dynamic_range) {
    StringBuffer<64> str(STRING_WITH_LEN("index map: 0x"), cs);
    /* 4 bits per 1 hex digit + terminating '\0' */
    char buf[MAX_KEY / 4 + 1];
    str.append(tab->keys().print(buf));
    if (push_extra(ET_RANGE_CHECKED_FOR_EACH_RECORD, str)) return true;
  } else if (condition) {
    if (fmt->is_hierarchical() && can_print_clauses()) {
      Lazy_condition *c = new (explain_thd->mem_root) Lazy_condition(condition);
      if (c == nullptr) return true;
      fmt->entry()->col_attached_condition.set(c);
    } else if (push_extra(ET_USING_WHERE))
      return true;
  }

  {
    const Item *pushed_cond = table->file->pushed_cond;
    if (pushed_cond) {
      StringBuffer<64> buff(cs);
      if (can_print_clauses())
        pushed_cond->print(explain_thd, &buff, cond_print_flags);
      if (push_extra(ET_USING_PUSHED_CONDITION, buff)) return true;
    }
    if (((range_scan_type >= 0 && is_reverse_sorted_range(range_scan_path)) ||
         reversed_access) &&
        push_extra(ET_BACKWARD_SCAN))
      return true;
  }
  if (table->reginfo.not_exists_optimize && push_extra(ET_NOT_EXISTS))
    return true;

  if (range_scan_type == AccessPath::INDEX_RANGE_SCAN) {
    uint mrr_flags = range_scan_path->index_range_scan().mrr_flags;

    /*
      During normal execution of a query, multi_range_read_init() is
      called to initialize MRR. If HA_MRR_SORTED is set at this point,
      multi_range_read_init() for any native MRR implementation will
      revert to default MRR if not HA_MRR_SUPPORT_SORTED.
      Calling multi_range_read_init() can potentially be costly, so it
      is not done when executing an EXPLAIN. We therefore simulate
      its effect here:
    */
    if (mrr_flags & HA_MRR_SORTED && !(mrr_flags & HA_MRR_SUPPORT_SORTED))
      mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;

    if (!(mrr_flags & HA_MRR_USE_DEFAULT_IMPL) && push_extra(ET_USING_MRR))
      return true;
  }

  if (type == JT_FT &&
      (table->file->ha_table_flags() & HA_CAN_FULLTEXT_HINTS)) {
    /*
      Print info about FT hints.
    */
    StringBuffer<64> buff(cs);
    Ft_hints *ft_hints = tab->ft_func()->get_hints();
    bool not_first = false;
    if (ft_hints->get_flags() & FT_SORTED) {
      buff.append("sorted");
      not_first = true;
    } else if (ft_hints->get_flags() & FT_NO_RANKING) {
      buff.append("no_ranking");
      not_first = true;
    }
    if (ft_hints->get_op_type() != FT_OP_UNDEFINED &&
        ft_hints->get_op_type() != FT_OP_NO) {
      char buf[64];
      size_t len = 0;

      if (not_first) buff.append(", ");
      switch (ft_hints->get_op_type()) {
        case FT_OP_GT:
          len = snprintf(buf, sizeof(buf) - 1, "rank > %.0g",
                         ft_hints->get_op_value());
          break;
        case FT_OP_GE:
          len = snprintf(buf, sizeof(buf) - 1, "rank >= %.0g",
                         ft_hints->get_op_value());
          break;
        default:
          assert(0);
      }

      buff.append(buf, len, cs);
      not_first = true;
    }

    if (ft_hints->get_limit() != HA_POS_ERROR) {
      char buf[64];
      size_t len = 0;

      if (not_first) buff.append(", ");

      len =
          snprintf(buf, sizeof(buf) - 1, "limit = %llu", ft_hints->get_limit());
      buff.append(buf, len, cs);
      not_first = true;
    }
    if (not_first) push_extra(ET_FT_HINTS, buff);
  }

  /*
    EXPLAIN FORMAT=JSON FOR CONNECTION will mention clearly that index dive has
    been skipped.
  */
  if (explain_thd->lex->sql_command == SQLCOM_EXPLAIN_OTHER &&
      fmt->is_hierarchical() && skip_records_in_range)
    push_extra(ET_SKIP_RECORDS_IN_RANGE);

  return false;
}

bool Explain_table_base::explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                                       bool need_sort_arg) {
  /*
    For hierarchical EXPLAIN we output "Using temporary" and
    "Using filesort" with related ORDER BY, GROUP BY or DISTINCT
  */
  if (fmt->is_hierarchical()) return false;

  if (need_tmp_table_arg && push_extra(ET_USING_TEMPORARY)) return true;
  if (need_sort_arg && push_extra(ET_USING_FILESORT)) return true;
  return false;
}

bool Explain_join::explain_modify_flags() {
  THD::Query_plan const *query_plan = &query_thd->query_plan;
  /*
    Because we are PLAN_READY, the following data structures are not changing
    and thus are safe to read.
  */
  switch (query_plan->get_command()) {
    case SQLCOM_UPDATE:
    case SQLCOM_UPDATE_MULTI:
      if (table->pos_in_table_list->is_updated() &&
          table->s->table_category != TABLE_CATEGORY_TEMPORARY)
        fmt->entry()->mod_type = MT_UPDATE;
      break;
    case SQLCOM_DELETE:
    case SQLCOM_DELETE_MULTI:
      if (table->pos_in_table_list->is_deleted() &&
          table->s->table_category != TABLE_CATEGORY_TEMPORARY)
        fmt->entry()->mod_type = MT_DELETE;
      break;
    case SQLCOM_INSERT_SELECT:
      if (table == query_plan->get_lex()->insert_table_leaf->table)
        fmt->entry()->mod_type = MT_INSERT;
      break;
    case SQLCOM_REPLACE_SELECT:
      if (table == query_plan->get_lex()->insert_table_leaf->table)
        fmt->entry()->mod_type = MT_REPLACE;
      break;
    default:;
  };
  return false;
}

/* Explain_join class functions ***********************************************/

bool Explain_join::begin_sort_context(Explain_sort_clause clause,
                                      enum_parsing_context ctx) {
  const Explain_format_flags *flags = &join->explain_flags;
  return (flags->get(clause, ESP_EXISTS) &&
          !flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->begin_context(ctx, nullptr, flags));
}

bool Explain_join::end_sort_context(Explain_sort_clause clause,
                                    enum_parsing_context ctx) {
  const Explain_format_flags *flags = &join->explain_flags;
  return (flags->get(clause, ESP_EXISTS) &&
          !flags->get(clause, ESP_IS_SIMPLE) && fmt->end_context(ctx));
}

bool Explain_join::begin_simple_sort_context(Explain_sort_clause clause,
                                             enum_parsing_context ctx) {
  const Explain_format_flags *flags = &join->explain_flags;
  return (flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->begin_context(ctx, nullptr, flags));
}

bool Explain_join::end_simple_sort_context(Explain_sort_clause clause,
                                           enum_parsing_context ctx) {
  const Explain_format_flags *flags = &join->explain_flags;
  return (flags->get(clause, ESP_IS_SIMPLE) && fmt->end_context(ctx));
}

bool Explain_join::shallow_explain() {
  qep_row *join_entry = fmt->entry();

  join_entry->col_read_cost.set(join->best_read);

  if (query_block->is_recursive()) {
    /*
      This will add the "recursive" word to:
      - the block of the JOIN, in JSON format
      - the first table of the JOIN, in TRADITIONAL format.
    */
    if (push_extra(ET_RECURSIVE)) return true; /* purecov: inspected */
  }

  LEX const *query_lex = join->thd->query_plan.get_lex();
  if (query_lex->insert_table_leaf &&
      query_lex->insert_table_leaf->query_block == join->query_block) {
    table = query_lex->insert_table_leaf->table;
    /*
      The target table for INSERT/REPLACE doesn't actually belong to join,
      thus tab is set to NULL. But in order to print it we add it to the
      list of plan rows. Explain printing code (traditional/json) will deal with
      it.
    */
    tab = nullptr;
    if (fmt->begin_context(CTX_QEP_TAB) || prepare_columns() ||
        fmt->flush_entry() || fmt->end_context(CTX_QEP_TAB))
      return true; /* purecov: inspected */
  }

  if (begin_sort_context(ESC_ORDER_BY, CTX_ORDER_BY))
    return true; /* purecov: inspected */
  if (begin_sort_context(ESC_DISTINCT, CTX_DISTINCT))
    return true; /* purecov: inspected */

  qep_row *order_by_distinct = fmt->entry();
  qep_row *windowing = nullptr;

  if (join->m_windowing_steps) {
    if (begin_sort_context(ESC_WINDOWING, CTX_WINDOW))
      return true; /* purecov: inspected */

    windowing = fmt->entry();
    if (!fmt->is_hierarchical()) {
      /*
        TRADITIONAL prints nothing for window functions, except the use of a
        temporary table and a filesort.
      */
      push_warning(explain_thd, Sql_condition::SL_NOTE, ER_WINDOW_EXPLAIN_JSON,
                   ER_THD(explain_thd, ER_WINDOW_EXPLAIN_JSON));
    }
    windowing->m_windows = &query_block->m_windows;
    if (join->windowing_cost > 0)
      windowing->col_read_cost.set(join->windowing_cost);
  }

  if (begin_sort_context(ESC_GROUP_BY, CTX_GROUP_BY))
    return true; /* purecov: inspected */

  qep_row *order_by_distinct_or_grouping = fmt->entry();

  if (join->sort_cost > 0.0) {
    /*
      This sort is for GROUP BY, ORDER BY, DISTINCT so we attach its cost to
      them, by checking which is in use. When there is no windowing, we ascribe
      this cost always to the GROUP BY, if there is one, since ORDER
      BY/DISTINCT sorts in those cases are elided, else to ORDER BY, or
      DISTINCT.  With windowing, both GROUP BY and ORDER BY/DISTINCT may carry
      sorting costs.
    */
    if (join->m_windowing_steps) {
      int atrs = 0;  // attribute sorting costs to pre-window and/or post-window
      if (order_by_distinct_or_grouping != windowing &&
          join->explain_flags.get(ESC_GROUP_BY, ESP_USING_FILESORT)) {
        // We have a group by: assign it cost iff is used sorting
        order_by_distinct_or_grouping->col_read_cost.set(join->sort_cost);
        atrs++;
      }
      if (order_by_distinct != join_entry &&
          (join->explain_flags.get(ESC_ORDER_BY, ESP_USING_FILESORT) ||
           join->explain_flags.get(ESC_DISTINCT, ESP_USING_FILESORT))) {
        order_by_distinct->col_read_cost.set(join->sort_cost);
        atrs++;
      }

      if (atrs == 2) {
        /*
          We do sorting twice because of intervening windowing sorts, so
          increase total correspondingly. It has already been added to
          best_read once in the optimizer.
        */
        join_entry->col_read_cost.set(join->best_read + join->sort_cost);
      }
    } else {
      /*
        Due to begin_sort_context() calls above, fmt->entry() returns another
        context than stored in join_entry.
      */
      assert(order_by_distinct_or_grouping != join_entry ||
             !fmt->is_hierarchical());
      order_by_distinct_or_grouping->col_read_cost.set(join->sort_cost);
    }
  }

  if (begin_sort_context(ESC_BUFFER_RESULT, CTX_BUFFER_RESULT))
    return true; /* purecov: inspected */

  for (size_t t = 0, cnt = fmt->is_hierarchical() ? join->primary_tables
                                                  : join->tables;
       t < cnt; t++) {
    if (explain_qep_tab(t)) return true;
  }

  if (end_sort_context(ESC_BUFFER_RESULT, CTX_BUFFER_RESULT)) return true;
  if (end_sort_context(ESC_GROUP_BY, CTX_GROUP_BY)) return true;
  if (join->m_windowing_steps) {
    if (end_sort_context(ESC_WINDOWING, CTX_WINDOW))
      return true; /* purecov: inspected */
  }
  if (end_sort_context(ESC_DISTINCT, CTX_DISTINCT)) return true;
  if (end_sort_context(ESC_ORDER_BY, CTX_ORDER_BY)) return true;

  return false;
}

bool Explain_join::explain_qep_tab(size_t tabnum) {
  tab = join->qep_tab + tabnum;
  type = tab->type();
  range_scan_path = tab->range_scan();
  condition = tab->condition_optim();
  dynamic_range = tab->dynamic_range();
  skip_records_in_range = tab->skip_records_in_range();
  reversed_access = tab->reversed_access();
  table_ref = tab->table_ref;
  if (!tab->position()) return false;
  table = tab->table();
  usable_keys = tab->keys();
  usable_keys.merge(table->possible_quick_keys);
  range_scan_type = -1;

  if (tab->type() == JT_RANGE || tab->type() == JT_INDEX_MERGE) {
    assert(range_scan_path);
    range_scan_type = range_scan_path->type;
  }

  if (tab->starts_weedout()) fmt->begin_context(CTX_DUPLICATES_WEEDOUT);

  const bool first_non_const = tabnum == join->const_tables;

  if (first_non_const) {
    if (begin_simple_sort_context(ESC_ORDER_BY, CTX_SIMPLE_ORDER_BY))
      return true;
    if (begin_simple_sort_context(ESC_DISTINCT, CTX_SIMPLE_DISTINCT))
      return true;
    if (begin_simple_sort_context(ESC_GROUP_BY, CTX_SIMPLE_GROUP_BY))
      return true;
  }

  Semijoin_mat_exec *const sjm = tab->sj_mat_exec();
  const enum_parsing_context c = sjm ? CTX_MATERIALIZATION : CTX_QEP_TAB;

  if (fmt->begin_context(c) || prepare_columns()) return true;

  fmt->entry()->query_block_id = table->pos_in_table_list->query_block_id();

  if (sjm) {
    if (sjm->is_scan) {
      fmt->entry()->col_rows.cleanup();  // TODO: set(something reasonable)
    } else {
      fmt->entry()->col_rows.set(1);
    }
  }

  if (fmt->flush_entry() ||
      (can_walk_clauses() && mark_subqueries(condition, fmt->entry())))
    return true;

  if (sjm && fmt->is_hierarchical()) {
    for (size_t sjt = sjm->inner_table_index, end = sjt + sjm->table_count;
         sjt < end; sjt++) {
      if (explain_qep_tab(sjt)) return true;
    }
  }

  if (fmt->end_context(c)) return true;

  if (first_non_const) {
    if (end_simple_sort_context(ESC_GROUP_BY, CTX_SIMPLE_GROUP_BY)) return true;
    if (end_simple_sort_context(ESC_DISTINCT, CTX_SIMPLE_DISTINCT)) return true;
    if (end_simple_sort_context(ESC_ORDER_BY, CTX_SIMPLE_ORDER_BY)) return true;
  }

  if (tab->finishes_weedout() && fmt->end_context(CTX_DUPLICATES_WEEDOUT))
    return true;

  return false;
}

/**
  Generates either usual table name or <derived#N>, and passes it to
  any given function for showing to the user.
  @param tr   Table reference
  @param fmt  EXPLAIN's format
  @param func Function receiving the name
  @returns true if error.
*/
static bool store_table_name(
    Table_ref *tr, Explain_format *fmt,
    std::function<bool(const char *name, size_t len)> func) {
  char namebuf[NAME_LEN];
  size_t len = sizeof(namebuf);
  if (tr->query_block_id() && tr->is_view_or_derived() &&
      !fmt->is_hierarchical()) {
    /* Derived table name generation */
    len = snprintf(namebuf, len - 1, "<derived%u>",
                   tr->query_block_id_for_explain());
    return func(namebuf, len);
  } else {
    return func(tr->alias, strlen(tr->alias));
  }
}

bool Explain_join::explain_table_name() {
  return store_table_name(table->pos_in_table_list, fmt,
                          [&](const char *name, size_t len) {
                            return fmt->entry()->col_table_name.set(name, len);
                          });
}

bool Explain_join::explain_select_type() {
  if (tab && sj_is_materialize_strategy(tab->get_sj_strategy()))
    fmt->entry()->col_select_type.set(enum_explain_type::EXPLAIN_MATERIALIZED);
  else
    return Explain::explain_select_type();
  return false;
}

bool Explain_join::explain_id() {
  if (tab && sj_is_materialize_strategy(tab->get_sj_strategy()))
    fmt->entry()->col_id.set(tab->sjm_query_block_id());
  else
    return Explain::explain_id();
  return false;
}

bool Explain_join::explain_join_type() {
  const join_type j_t = type == JT_UNKNOWN ? JT_ALL : type;
  const char *str = join_type_str[j_t];
  if ((j_t == JT_EQ_REF || j_t == JT_REF || j_t == JT_REF_OR_NULL) &&
      join->query_expression()->item) {
    /*
      For backward-compatibility, we have special presentation of "index
      lookup used for in(subquery)": we do not show "ref/etc", but
      "index_subquery/unique_subquery".
    */
    if (join->query_expression()->item->engine_type() ==
        Item_subselect::INDEXSUBQUERY_ENGINE)
      str = (j_t == JT_EQ_REF) ? "unique_subquery" : "index_subquery";
  }

  fmt->entry()->col_join_type.set_const(str);
  return false;
}

bool Explain_join::explain_key_and_len() {
  if (!tab) return false;
  if (tab->ref().key_parts)
    return explain_key_and_len_index(tab->ref().key, tab->ref().key_length,
                                     tab->ref().key_parts);
  else if (type == JT_INDEX_SCAN || type == JT_FT)
    return explain_key_and_len_index(tab->index());
  else if (type == JT_RANGE || type == JT_INDEX_MERGE ||
           ((type == JT_REF || type == JT_REF_OR_NULL) && range_scan_path))
    return explain_key_and_len_quick(range_scan_path);
  return false;
}

bool Explain_join::explain_ref() {
  if (!tab) return false;
  return explain_ref_key(fmt, tab->ref().key_parts, tab->ref().key_copy);
}

bool Explain_join::explain_rows_and_filtered() {
  if (!tab || tab->table_ref->schema_table) return false;

  POSITION *const pos = tab->position();

  if (explain_thd->lex->sql_command == SQLCOM_EXPLAIN_OTHER &&
      skip_records_in_range) {
    // Skipping col_rows, col_filtered, col_prefix_rows will set them to NULL.
    fmt->entry()->col_cond_cost.set(0);
    fmt->entry()->col_read_cost.set(0.0);
    fmt->entry()->col_prefix_cost.set(0);
    fmt->entry()->col_data_size_query.set("0");
  } else {
    fmt->entry()->col_rows.set(static_cast<ulonglong>(pos->rows_fetched));
    fmt->entry()->col_filtered.set(
        pos->rows_fetched
            ? static_cast<float>(100.0 * tab->position()->filter_effect)
            : 0.0f);

    // Print cost-related info
    double prefix_rows = pos->prefix_rowcount;
    ulonglong prefix_rows_ull =
        static_cast<ulonglong>(std::min(prefix_rows, ULLONG_MAX_DOUBLE));
    fmt->entry()->col_prefix_rows.set(prefix_rows_ull);
    double const cond_cost = join->cost_model()->row_evaluate_cost(prefix_rows);
    fmt->entry()->col_cond_cost.set(cond_cost < 0 ? 0 : cond_cost);
    fmt->entry()->col_read_cost.set(pos->read_cost < 0.0 ? 0.0
                                                         : pos->read_cost);
    fmt->entry()->col_prefix_cost.set(pos->prefix_cost);
    // Calculate amount of data from this table per query
    char data_size_str[32];
    double data_size = prefix_rows * tab->table()->s->rec_buff_length;
    human_readable_num_bytes(data_size_str, sizeof(data_size_str), data_size);
    fmt->entry()->col_data_size_query.set(data_size_str);
  }

  return false;
}

bool Explain_join::explain_extra() {
  if (!tab) return false;
  if (tab->type() == JT_SYSTEM && tab->position()->rows_fetched == 0.0) {
    if (push_extra(ET_CONST_ROW_NOT_FOUND))
      return true; /* purecov: inspected */
  } else if (tab->type() == JT_CONST && tab->position()->rows_fetched == 0.0) {
    if (push_extra(ET_UNIQUE_ROW_NOT_FOUND))
      return true; /* purecov: inspected */
  } else if (tab->type() == JT_CONST && tab->position()->rows_fetched == 1.0 &&
             tab->table()->has_null_row()) {
    if (push_extra(ET_IMPOSSIBLE_ON_CONDITION))
      return true; /* purecov: inspected */
  } else {
    uint keyno = MAX_KEY;
    if (tab->ref().key_parts)
      keyno = tab->ref().key;
    else if (tab->type() == JT_RANGE || tab->type() == JT_INDEX_MERGE)
      keyno = used_index(range_scan_path);

    if (explain_extra_common(range_scan_type, keyno)) return true;

    if (((tab->type() == JT_INDEX_SCAN || tab->type() == JT_CONST) &&
         table->covering_keys.is_set(tab->index())) ||
        (range_scan_type == AccessPath::ROWID_INTERSECTION &&
         range_scan_path->rowid_intersection().is_covering) ||
        /*
          Notice that table->key_read can change on the fly (grep
          for set_keyread); so EXPLAIN CONNECTION reads a changing variable,
          fortunately it's a bool and not a pointer and the consequences
          cannot be severe (at worst, wrong EXPLAIN).
        */
        table->key_read || tab->keyread_optim()) {
      if (range_scan_type == AccessPath::GROUP_INDEX_SKIP_SCAN) {
        StringBuffer<64> buff(cs);
        if (range_scan_path->group_index_skip_scan().param->is_index_scan)
          buff.append(STRING_WITH_LEN("scanning"));
        if (push_extra(ET_USING_INDEX_FOR_GROUP_BY, buff)) return true;
      } else if (range_scan_type == AccessPath::INDEX_SKIP_SCAN) {
        if (push_extra(ET_USING_INDEX_FOR_SKIP_SCAN)) return true;
      } else {
        if (push_extra(ET_USING_INDEX)) return true;
      }
    }

    if (explain_tmptable_and_filesort(need_tmp_table, need_order)) return true;
    need_tmp_table = need_order = false;

    if (distinct && tab->not_used_in_distinct && push_extra(ET_DISTINCT))
      return true;

    if (tab->do_loosescan() && push_extra(ET_LOOSESCAN)) return true;

    if (tab->starts_weedout()) {
      if (!fmt->is_hierarchical() && push_extra(ET_START_TEMPORARY))
        return true;
    }
    if (tab->finishes_weedout()) {
      if (!fmt->is_hierarchical() && push_extra(ET_END_TEMPORARY)) return true;
    } else if (tab->do_firstmatch()) {
      if (tab->firstmatch_return == PRE_FIRST_PLAN_IDX) {
        if (push_extra(ET_FIRST_MATCH)) return true;
      } else {
        StringBuffer<64> buff(cs);
        if (store_table_name(join->qep_tab[tab->firstmatch_return].table_ref,
                             fmt,
                             [&](const char *name, size_t len) {
                               return buff.append(name, len);
                             }) ||
            push_extra(ET_FIRST_MATCH, buff))
          return true;
      }
    }

    if (tab->lateral_derived_tables_depend_on_me) {
      StringBuffer<64> buff(cs);
      bool first = true;
      for (int table_idx :
           BitsSetIn(tab->lateral_derived_tables_depend_on_me)) {
        QEP_TAB *tab2 = &join->qep_tab[table_idx];
        if (!first) buff.append(",");
        first = false;
        if (store_table_name(tab2->table_ref, fmt,
                             [&](const char *name, size_t len) {
                               return buff.append(name, len);
                             }))
          return true;
      }
      if (push_extra(ET_REMATERIALIZE, buff)) return true;
    }

    if (tab->has_guarded_conds() && push_extra(ET_FULL_SCAN_ON_NULL_KEY))
      return true;

    if (tab->op_type == QEP_TAB::OT_BNL || tab->op_type == QEP_TAB::OT_BKA) {
      StringBuffer<64> buff(cs);
      if (tab->op_type == QEP_TAB::OT_BNL) {
        // BNL does not exist in the iterator executor, but is nearly
        // always rewritten to hash join, so use that in traditional EXPLAIN.
        buff.append("hash join");
      } else if (tab->op_type == QEP_TAB::OT_BKA)
        buff.append("Batched Key Access");
      else
        assert(0); /* purecov: inspected */
      if (push_extra(ET_USING_JOIN_BUFFER, buff)) return true;
    }
  }
  if (fmt->is_hierarchical() && (!bitmap_is_clear_all(table->read_set) ||
                                 !bitmap_is_clear_all(table->write_set))) {
    Field **fld;
    for (fld = table->field; *fld; fld++) {
      if (!bitmap_is_set(table->read_set, (*fld)->field_index()) &&
          !bitmap_is_set(table->write_set, (*fld)->field_index()))
        continue;

      const char *field_description =
          get_field_name_or_expression(explain_thd, *fld);
      fmt->entry()->col_used_columns.push_back(field_description);
      if (table->is_binary_diff_enabled(*fld))
        fmt->entry()->col_partial_update_columns.push_back(field_description);
    }
  }

  if (table->s->is_secondary_engine() &&
      push_extra(ET_USING_SECONDARY_ENGINE, table->file->table_type()))
    return true;

  return false;
}

/* Explain_table class functions **********************************************/

bool Explain_table::explain_modify_flags() {
  fmt->entry()->mod_type = mod_type;
  return false;
}

bool Explain_table::explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                                  bool need_sort_arg) {
  if (fmt->is_hierarchical()) {
    /*
      For hierarchical EXPLAIN we output "using_temporary_table" and
      "using_filesort" with related ORDER BY, GROUP BY or DISTINCT
      (excluding the single-table UPDATE command that updates used key --
      in this case we output "using_temporary_table: for update"
      at the "table" node)
    */
    if (need_tmp_table_arg) {
      assert(used_key_is_modified || order_list);
      if (used_key_is_modified && push_extra(ET_USING_TEMPORARY, "for update"))
        return true;
    }
  } else {
    if (need_tmp_table_arg && push_extra(ET_USING_TEMPORARY)) return true;

    if (need_sort_arg && push_extra(ET_USING_FILESORT)) return true;
  }

  return false;
}

bool Explain_table::shallow_explain() {
  Explain_format_flags flags;
  if (order_list) {
    flags.set(ESC_ORDER_BY, ESP_EXISTS);
    if (need_sort) flags.set(ESC_ORDER_BY, ESP_USING_FILESORT);
    if (!used_key_is_modified && need_tmp_table)
      flags.set(ESC_ORDER_BY, ESP_USING_TMPTABLE);
  }

  if (order_list && fmt->begin_context(CTX_ORDER_BY, nullptr, &flags))
    return true;

  if (fmt->begin_context(CTX_QEP_TAB)) return true;

  if (Explain::shallow_explain() ||
      (can_walk_clauses() &&
       mark_subqueries(query_block->where_cond(), fmt->entry())))
    return true;

  if (fmt->end_context(CTX_QEP_TAB)) return true;

  if (order_list && fmt->end_context(CTX_ORDER_BY)) return true;

  return false;
}

bool Explain_table::explain_table_name() {
  return fmt->entry()->col_table_name.set(table->alias);
}

bool Explain_table::explain_join_type() {
  join_type jt;
  if (range_scan_path)
    jt = calc_join_type(range_scan_path);
  else if (key != MAX_KEY)
    jt = JT_INDEX_SCAN;
  else
    jt = JT_ALL;

  fmt->entry()->col_join_type.set_const(join_type_str[jt]);
  return false;
}

bool Explain_table::explain_ref() {
  if (range_scan_path) {
    int key_parts = get_used_key_parts(range_scan_path);
    while (key_parts--) {
      fmt->entry()->col_ref.push_back("const");
    }
  }
  return false;
}

bool Explain_table::explain_key_and_len() {
  if (range_scan_path)
    return explain_key_and_len_quick(range_scan_path);
  else if (key != MAX_KEY)
    return explain_key_and_len_index(key);
  return false;
}

bool Explain_table::explain_rows_and_filtered() {
  /* Don't print estimated # of rows in table for INSERT/REPLACE. */
  if (fmt->entry()->mod_type == MT_INSERT ||
      fmt->entry()->mod_type == MT_REPLACE)
    return false;

  ha_rows examined_rows =
      query_thd->query_plan.get_modification_plan()->examined_rows;
  fmt->entry()->col_rows.set(static_cast<long long>(examined_rows));

  fmt->entry()->col_filtered.set(100.0);

  return false;
}

bool Explain_table::explain_extra() {
  if (message) return fmt->entry()->col_message.set(message);

  for (Field **fld = table->field; *fld != nullptr; ++fld)
    if (table->is_binary_diff_enabled(*fld))
      fmt->entry()->col_partial_update_columns.push_back((*fld)->field_name);

  uint keyno;
  int range_scan_type;
  if (range_scan_path) {
    keyno = used_index(range_scan_path);
    range_scan_type = range_scan_path->type;
  } else {
    keyno = key;
    range_scan_type = -1;
  }

  return (explain_extra_common(range_scan_type, keyno) ||
          explain_tmptable_and_filesort(need_tmp_table, need_sort));
}

/**
  Send a message as an "extra" column value

  This function forms the 1st row of the QEP output with a simple text message.
  This is useful to explain such trivial cases as "No tables used" etc.

  @note Also this function explains the rest of QEP (subqueries or joined
        tables if any).

  @param explain_thd thread handle for the connection doing explain
  @param query_thd   thread handle for the connection being explained
  @param query_block  query_block to explain
  @param message     text message for the "extra" column.
  @param ctx         current query context, CTX_JOIN in most cases.

  @return false if success, true if error
*/

static bool explain_no_table(THD *explain_thd, const THD *query_thd,
                             Query_block *query_block, const char *message,
                             enum_parsing_context ctx) {
  DBUG_TRACE;
  const bool ret = Explain_no_table(explain_thd, query_thd, query_block,
                                    message, ctx, HA_POS_ERROR)
                       .send();
  return ret;
}

/**
  Check that we are allowed to explain all views in list.
  Because this function is called only when we have a complete plan, we know
  that:
  - views contained in merge-able views have been merged and brought up in
  the top list of tables, so we only need to scan this list
  - table_list is not changing while we are reading it.
  If we don't have a complete plan, EXPLAIN output does not contain table
  names, so we don't need to check views.

  @param table_list table to start with, usually lex->query_tables

  @returns
    true   Caller can't EXPLAIN query due to lack of rights on a view in the
           query
    false  Caller can EXPLAIN query
*/

static bool check_acl_for_explain(const Table_ref *table_list) {
  for (const Table_ref *tbl = table_list; tbl; tbl = tbl->next_global) {
    if (tbl->is_view() && tbl->view_no_explain) {
      my_error(ER_VIEW_NO_EXPLAIN, MYF(0));
      return true;
    }
  }
  return false;
}

/**
  EXPLAIN handling for single-table INSERT VALUES, UPDATE, and DELETE queries

  Send to the client a QEP data set for single-table
  EXPLAIN INSERT VALUES/UPDATE/DELETE queries.
  As far as single-table INSERT VALUES/UPDATE/DELETE are implemented without
  the regular JOIN tree, we can't reuse explain_query_expression() directly,
  thus we deal with this single table in a special way and then call
  explain_query_expression() for subqueries (if any).

  @param explain_thd    thread handle for the connection doing explain
  @param query_thd      thread handle for the connection being explained
  @param plan           table modification plan
  @param select         Query's select lex

  @return false if success, true if error
*/

bool explain_single_table_modification(THD *explain_thd, const THD *query_thd,
                                       const Modification_plan *plan,
                                       Query_block *select) {
  DBUG_TRACE;
  Query_result_send result;
  const bool other = (query_thd != explain_thd);
  bool ret;

  if (explain_thd->lex->explain_format->is_iterator_based()) {
    // These kinds of queries don't have a JOIN with an iterator tree.
    return ExplainIterator(explain_thd, query_thd, nullptr);
  }

  if (query_thd->lex->using_hypergraph_optimizer) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "EXPLAIN with TRADITIONAL format");
    return true;
  }

  /**
    Prepare the self-allocated result object

    For queries with top-level JOIN the caller provides pre-allocated
    Query_result_send object. Then that JOIN object prepares the
    Query_result_send object calling result->prepare() in
    Query_block::prepare(), result->optimize() in JOIN::optimize() and
    result->start_execution() in JOIN::exec(). However without the presence of
    the top-level JOIN we have to prepare/initialize Query_result_send object
    manually.
  */
  mem_root_deque<Item *> dummy(explain_thd->mem_root);
  if (result.prepare(explain_thd, dummy, explain_thd->lex->unit))
    return true; /* purecov: inspected */

  explain_thd->lex->explain_format->send_headers(&result);

  /*
    Optimize currently non-optimized subqueries when needed, but
    - do not optimize subqueries for other connections, and
    - there is no need to optimize subqueries that will not be explained
      because they are attached to a query block that do not return any rows.
  */
  if (!other && !select->is_empty_query()) {
    for (Query_expression *unit = select->first_inner_query_expression(); unit;
         unit = unit->next_query_expression()) {
      // Derived tables and const subqueries are already optimized
      if (!unit->is_optimized() &&
          unit->optimize(explain_thd, /*materialize_destination=*/nullptr,
                         /*create_iterators=*/false,
                         /*finalize_access_paths=*/true))
        return true; /* purecov: inspected */
    }
  }

  if (!plan || plan->zero_result) {
    ret = Explain_no_table(explain_thd, query_thd, select,
                           plan ? plan->message : plan_not_ready[other],
                           CTX_JOIN, HA_POS_ERROR)
              .send();
  } else {
    // Check access rights for views
    if (other &&
        check_acl_for_explain(query_thd->query_plan.get_lex()->query_tables))
      ret = true;
    else
      ret = Explain_table(explain_thd, query_thd, select, plan->table,
                          plan->type, plan->range_scan, plan->condition,
                          plan->key, plan->limit, plan->need_tmp_table,
                          plan->need_sort, plan->mod_type,
                          plan->used_key_is_modified, plan->message)
                .send() ||
            explain_thd->is_error();
  }
  if (ret)
    result.abort_result_set(explain_thd);
  else {
    if (!other) {
      StringBuffer<1024> str;
      query_thd->lex->unit->print(
          explain_thd, &str,
          enum_query_type(QT_TO_SYSTEM_CHARSET | QT_SHOW_SELECT_NUMBER |
                          QT_NO_DATA_EXPANSION));
      str.append('\0');
      push_warning(explain_thd, Sql_condition::SL_NOTE, ER_YES, str.ptr());
    }

    result.send_eof(explain_thd);
  }
  return ret;
}

/**
  Explain query_block's join.

  @param explain_thd thread handle for the connection doing explain
  @param query_thd   thread handle for the connection being explained
  @param query_term  explain join attached to given term's query_block
  @param ctx         current explain context
*/

bool explain_query_specification(THD *explain_thd, const THD *query_thd,
                                 Query_term *query_term,
                                 enum_parsing_context ctx) {
  Query_block *query_block = query_term->query_block();
  Opt_trace_context *const trace = &explain_thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "join_explain");
  trace_exec.add_select_number(query_block->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  JOIN *join = query_block->join;
  const bool other = (query_thd != explain_thd);

  if (!join || join->get_plan_state() == JOIN::NO_PLAN)
    return explain_no_table(explain_thd, query_thd, query_block,
                            plan_not_ready[other], ctx);

  THD::Query_plan const *query_plan = &join->thd->query_plan;

  // Check access rights for views
  if (other && check_acl_for_explain(query_plan->get_lex()->query_tables))
    return true;

  THD_STAGE_INFO(explain_thd, stage_explaining);

  bool ret;

  switch (join->get_plan_state()) {
    case JOIN::ZERO_RESULT: {
      ret = explain_no_table(explain_thd, query_thd, query_block,
                             join->zero_result_cause, ctx);
      break;
    }
    case JOIN::NO_TABLES: {
      if (query_plan->get_lex()->insert_table_leaf &&
          query_plan->get_lex()->insert_table_leaf->query_block ==
              query_block) {
        // INSERT/REPLACE SELECT ... FROM dual
        ret = Explain_table(
                  explain_thd, query_thd, query_block,
                  query_plan->get_lex()->insert_table_leaf->table, JT_UNKNOWN,
                  /*quick=*/nullptr, /*condition=*/nullptr, MAX_KEY,
                  HA_POS_ERROR, false, false,
                  (query_plan->get_lex()->sql_command == SQLCOM_INSERT_SELECT
                       ? MT_INSERT
                       : MT_REPLACE),
                  false, nullptr)
                  .send() ||
              explain_thd->is_error();
      } else
        ret = explain_no_table(explain_thd, query_thd, query_block,
                               "No tables used", CTX_JOIN);

      break;
    }
    case JOIN::PLAN_READY: {
      /*
        (1) If this connection is explaining its own query
        (2) and it hasn't already prepared the JOIN's result,
        then we need to prepare it (for example, to materialize I_S tables).
      */
      if (!other && !join->is_executed() && join->prepare_result())
        return true; /* purecov: inspected */

      const Explain_format_flags *flags = &join->explain_flags;
      const bool need_tmp_table = flags->any(ESP_USING_TMPTABLE);
      const bool need_order = flags->any(ESP_USING_FILESORT);
      const bool distinct = flags->get(ESC_DISTINCT, ESP_EXISTS);

      if (query_term->term_type() == QT_QUERY_BLOCK)
        ret = Explain_join(explain_thd, query_thd, query_block, need_tmp_table,
                           need_order, distinct)
                  .send();
      else {
        ret = Explain_setop_result(explain_thd, query_thd, query_block,
                                   query_term, ctx)
                  .send();
      }
      break;
    }
    default:
      assert(0); /* purecov: inspected */
      ret = true;
  }
  assert(ret || !explain_thd->is_error());
  ret |= explain_thd->is_error();
  return ret;
}

static bool ExplainIterator(THD *ethd, const THD *query_thd,
                            Query_expression *unit) {
  Query_result_send result;
  {
    mem_root_deque<Item *> field_list(ethd->mem_root);
    Item *item = new Item_empty_string("EXPLAIN", 78, system_charset_info);
    field_list.push_back(item);
    if (result.send_result_set_metadata(
            ethd, field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
      return true;
    }
  }

  {
    std::string explain = PrintQueryPlan(ethd, query_thd, unit);
    if (explain.empty()) {
      my_error(ER_INTERNAL_ERROR, MYF(0), "Failed to print query plan");
      return true;
    }
    mem_root_deque<Item *> field_list(ethd->mem_root);
    Item *item =
        new Item_string(explain.data(), explain.size(), system_charset_info);
    field_list.push_back(item);

    if (query_thd->killed) {
      ethd->raise_warning(ER_QUERY_INTERRUPTED);
    }

    if (result.send_data(ethd, field_list)) {
      return true;
    }
  }
  return result.send_eof(ethd);
}

/**
  A query result handler that outputs nothing. It is used during EXPLAIN
  ANALYZE, to ignore the output of the query when it's being run.
 */
class Query_result_null : public Query_result_interceptor {
 public:
  Query_result_null() : Query_result_interceptor() {}
  uint field_count(const mem_root_deque<Item *> &) const override { return 0; }
  bool send_result_set_metadata(THD *, const mem_root_deque<Item *> &,
                                uint) override {
    return false;
  }
  bool send_data(THD *thd, const mem_root_deque<Item *> &items) override {
    // Evaluate all the items, to make sure that any subqueries in SELECT lists
    // are evaluated. We don't get their timings added to any parents, but at
    // least we will have real row counts and times printed out.
    for (Item *item : VisibleFields(items)) {
      item->val_str(&m_str);
      if (thd->is_error()) return true;
    }
    return false;
  }
  bool send_eof(THD *) override { return false; }

 private:
  String m_str;
};

/**
  This code which prints the extended description is not robust
  against malformed queries, so skip calling this function if we have an error
  or if explaining other thread (see Explain::can_print_clauses()).
*/
void print_query_for_explain(const THD *query_thd, Query_expression *unit,
                             String *str) {
  if (unit == nullptr) return;

  /* Only certain statements can be explained.  */
  if (query_thd->query_plan.get_command() == SQLCOM_SELECT ||
      query_thd->query_plan.get_command() == SQLCOM_INSERT_SELECT ||
      query_thd->query_plan.get_command() == SQLCOM_REPLACE_SELECT ||
      query_thd->query_plan.get_command() == SQLCOM_DELETE ||
      query_thd->query_plan.get_command() == SQLCOM_DELETE_MULTI ||
      query_thd->query_plan.get_command() == SQLCOM_UPDATE ||
      query_thd->query_plan.get_command() == SQLCOM_UPDATE_MULTI)  // (2)
  {
    /*
      The warnings system requires input in utf8, see mysqld_show_warnings().
    */

    enum_query_type eqt =
        enum_query_type(QT_TO_SYSTEM_CHARSET | QT_SHOW_SELECT_NUMBER);

    /**
      For DML statements use QT_NO_DATA_EXPANSION to avoid over-simplification.
    */
    if (query_thd->query_plan.get_command() != SQLCOM_SELECT)
      eqt = enum_query_type(eqt | QT_NO_DATA_EXPANSION);

    unit->print(query_thd, str, eqt);
  }
}
/**
  EXPLAIN handling for SELECT, INSERT/REPLACE SELECT, and multi-table
  UPDATE/DELETE queries

  Send to the client a QEP data set for any DML statement that has a QEP
  represented completely by JOIN object(s).

  This function uses a specific Query_result object for sending explain
  output to the client.

  When explaining own query, the existing Query_result object (found
  in outermost Query_expression or Query_block) is used. However, if the
  Query_result is unsuitable for explanation (need_explain_interceptor()
  returns true), wrap the Query_result inside an Query_result_explain object.

  When explaining other query, create a Query_result_send object and prepare it
  as if it was a regular SELECT query.

  @note see explain_single_table_modification() for single-table
        UPDATE/DELETE EXPLAIN handling.

  @note explain_query() calls abort_result_set() itself in the case of
        failure (OOM etc.) since it may use an internally created
        Query_result object that has to be deleted before exiting the function.

  @param explain_thd thread handle for the connection doing explain
  @param query_thd   thread handle for the connection being explained
  @param unit    query tree to explain

  @return false if success, true if error
*/

bool explain_query(THD *explain_thd, const THD *query_thd,
                   Query_expression *unit) {
  DBUG_TRACE;

  const bool other = (explain_thd != query_thd);
  const bool secondary_engine = SecondaryEngineHandlerton(query_thd) != nullptr;

  LEX *lex = explain_thd->lex;
  if (lex->explain_format->is_iterator_based()) {
    if (lex->is_explain_analyze) {
      if (secondary_engine) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "EXPLAIN ANALYZE with secondary engine");
        return true;
      }
      if (unit->root_iterator() == nullptr) {
        // TODO(sgunders): Remove when the iterator executor supports
        // all queries.
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), "EXPLAIN ANALYZE on this query");
        unit->set_executed();
        return true;
      }

      // Run the query, but with the result suppressed.
      Query_result_null null_result;
      unit->set_query_result(&null_result);
      explain_thd->running_explain_analyze = true;
      unit->execute(explain_thd);
      explain_thd->running_explain_analyze = false;
      unit->set_executed();
      if (query_thd->is_error()) return true;
    }
    if (secondary_engine)
      push_warning(explain_thd, Sql_condition::SL_NOTE, ER_YES,
                   "Query is executed in secondary engine; the actual"
                   " query plan may diverge from the printed one");
    return ExplainIterator(explain_thd, query_thd, unit);
  }

  // Non-iterator-based formats are not supported with EXPLAIN ANALYZE.
  if (lex->is_explain_analyze)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             (lex->explain_format->is_hierarchical()
                  ? "EXPLAIN ANALYZE with JSON format"
                  : "EXPLAIN ANALYZE with TRADITIONAL format"));

  // Non-iterator-based formats are not supported with the hypergraph
  // optimizer. But we still want to be able to use EXPLAIN with no format
  // specified (implicitly the traditional format) to show if the query is
  // offloaded to a secondary engine, so we return a fake plan with that
  // information.
  const bool fake_explain_for_secondary_engine =
      query_thd->lex->using_hypergraph_optimizer && secondary_engine &&
      !lex->explain_format->is_hierarchical();

  if (query_thd->lex->using_hypergraph_optimizer &&
      !fake_explain_for_secondary_engine) {
    // With hypergraph, JSON is iterator-based. So it must be TRADITIONAL.
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "EXPLAIN with TRADITIONAL format");
    return true;
  }

  Query_result *explain_result = nullptr;

  if (!other)
    explain_result = unit->query_result()
                         ? unit->query_result()
                         : unit->first_query_block()->query_result();

  Query_result_explain explain_wrapper(unit, explain_result);

  if (other) {
    if (!((explain_result = new (explain_thd->mem_root) Query_result_send())))
      return true; /* purecov: inspected */
    mem_root_deque<Item *> dummy(explain_thd->mem_root);
    if (explain_result->prepare(explain_thd, dummy, explain_thd->lex->unit))
      return true; /* purecov: inspected */
  } else {
    assert(unit->is_optimized());
    if (explain_result->need_explain_interceptor())
      explain_result = &explain_wrapper;
  }

  explain_thd->lex->explain_format->send_headers(explain_result);

  // Reset OFFSET/LIMIT for EXPLAIN output
  explain_thd->lex->unit->offset_limit_cnt = 0;
  explain_thd->lex->unit->select_limit_cnt = 0;

  const bool res =
      fake_explain_for_secondary_engine
          ? Explain_secondary_engine(explain_thd, query_thd,
                                     unit->first_query_block())
                .send()
          : mysql_explain_query_expression(explain_thd, query_thd, unit);

  // Skip this if applicable. See print_query_for_explain() comments.
  if (!res && !other) {
    StringBuffer<1024> str;
    print_query_for_explain(query_thd, unit, &str);
    str.append('\0');
    push_warning(explain_thd, Sql_condition::SL_NOTE, ER_YES, str.ptr());
  }

  if (res)
    explain_result->abort_result_set(explain_thd);
  else
    explain_result->send_eof(explain_thd);

  if (other) destroy(explain_result);

  return res;
}

/**
  Explain UNION or subqueries of the unit

  If the unit is a UNION, explain it as a UNION. Otherwise explain nested
  subselects.

  @param explain_thd    thread handle for the connection doing explain
  @param query_thd      thread handle for the connection being explained
  @param unit           unit object, might not belong to ethd

  @return false if success, true if error
*/

bool mysql_explain_query_expression(THD *explain_thd, const THD *query_thd,
                                    Query_expression *unit) {
  DBUG_TRACE;
  bool res = false;
  if (unit->is_simple())
    res = explain_query_specification(explain_thd, query_thd,
                                      unit->query_term(), CTX_JOIN);
  else
    res = unit->explain(explain_thd, query_thd);
  assert(res || !explain_thd->is_error());
  res |= explain_thd->is_error();
  return res;
}

/**
  Callback function used by Sql_cmd_explain_other_thread::execute() to find thd
  based on the thread id.

  @note It acquires LOCK_thd_data mutex and LOCK_query_plan mutex,
  when it finds matching thd.
  It is the responsibility of the caller to release LOCK_thd_data.
  We release LOCK_query_plan in the DTOR.
*/
class Find_thd_query_lock : public Find_THD_Impl {
 public:
  explicit Find_thd_query_lock(my_thread_id value)
      : m_id(value), m_thd(nullptr) {}
  ~Find_thd_query_lock() override {
    if (m_thd) m_thd->unlock_query_plan();
  }
  bool operator()(THD *thd) override {
    if (thd->thread_id() == m_id) {
      thd->lock_query_plan();
      m_thd = thd;
      return true;
    }
    return false;
  }

 private:
  const my_thread_id m_id;  ///< The thread id we are looking for.
  THD *m_thd;               ///< THD we found, having this ID.
};

/**
   Entry point for EXPLAIN CONNECTION: locates the connection by its ID, takes
   proper locks, explains its current statement, releases locks.
   @param  thd THD executing this function (== the explainer)
*/
bool Sql_cmd_explain_other_thread::execute(THD *thd) {
  bool res = false;
  bool send_ok = false;
  const char *user;
  const std::string &db_name = thd->db().str ? thd->db().str : "";
  THD::Query_plan *qp;
  DEBUG_SYNC(thd, "before_explain_other");
  /*
    Check for a super user, if:
    1) connected user don't have enough rights, or
    2) has switched to another user
    then it's not super user.
  */
  if (!(thd->m_main_security_ctx.check_access(GLOBAL_ACLS & ~GRANT_ACL,
                                              db_name)) ||    // (1)
      (0 != strcmp(thd->m_main_security_ctx.priv_user().str,  // (2)
                   thd->security_context()->priv_user().str) ||
       0 != my_strcasecmp(system_charset_info,
                          thd->m_main_security_ctx.priv_host().str,
                          thd->security_context()->priv_host().str))) {
    // Can see only connections of this user
    user = thd->security_context()->priv_user().str;
  } else {
    // Can see all connections
    user = nullptr;
  }

  // Pick thread
  Find_thd_query_lock find_thd_query_lock(m_thread_id);
  THD_ptr query_thd_ptr;
  if (!thd->killed) {
    query_thd_ptr =
        Global_THD_manager::get_instance()->find_thd(&find_thd_query_lock);
  }

  if (!query_thd_ptr) {
    my_error(ER_NO_SUCH_THREAD, MYF(0), m_thread_id);
    goto err;
  }

  qp = &query_thd_ptr->query_plan;

  if (query_thd_ptr->get_protocol()->connection_alive() &&
      !query_thd_ptr->system_thread && qp->get_command() != SQLCOM_END) {
    /*
      Don't explain:
      1) Prepared statements
      2) EXPLAIN to avoid clash in EXPLAIN code
      3) statements of stored routine
      4) Resolver has not finished (then data structures are changing too much
        and are not safely readable).
        m_sql_cmd is set during parsing and cleared in LEX::reset(), without
        mutex. If we are here, the explained connection has set its qp to
        something else than SQLCOM_END with set_query_plan(), so is in a phase
        after parsing and before LEX::reset(). Thus we can read m_sql_cmd.
        m_sql_cmd::m_prepared is set at end of resolution and cleared at end
        of execution (before setting qp to SQLCOM_END), without mutex.
        So if we see it false while it just changed to true, we'll bail out
        which is ok; if we see it true while it just changed to false, we can
        indeed explain as the plan is still valid and will remain so as we
        hold the mutex.
    */
    if (!qp->is_ps_query() &&  // (1)
        is_explainable_query(qp->get_command()) &&
        !qp->get_lex()->is_explain() &&      // (2)
        qp->get_lex()->sphead == nullptr &&  // (3)
        (!qp->get_lex()->m_sql_cmd ||
         qp->get_lex()->m_sql_cmd->is_prepared()))  // (4)
    {
      Security_context *tmp_sctx = query_thd_ptr->security_context();
      assert(tmp_sctx->user().str);
      if (user && strcmp(tmp_sctx->user().str, user)) {
        my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
                 thd->security_context()->priv_user().str,
                 thd->security_context()->priv_host().str,
                 (thd->password ? ER_THD(thd, ER_YES) : ER_THD(thd, ER_NO)));
        goto err;
      }
    } else {
      /*
        Note that we send "not supported" for a supported stmt (e.g. SELECT)
        which is in-parsing or in-preparation, which is a bit confusing, but
        ok as the user is unlikely to try EXPLAIN in these short phases.
      */
      my_error(ER_EXPLAIN_NOT_SUPPORTED, MYF(0));
      goto err;
    }
  } else {
    send_ok = true;
    goto err;
  }
  DEBUG_SYNC(thd, "explain_other_got_thd");

  if (qp->is_single_table_plan())
    res = explain_single_table_modification(
        thd, query_thd_ptr.get(), qp->get_modification_plan(),
        qp->get_lex()->unit->first_query_block());
  else
    res = explain_query(thd, query_thd_ptr.get(), qp->get_lex()->unit);

err:
  DEBUG_SYNC(thd, "after_explain_other");
  if (!res && send_ok) my_ok(thd, 0);

  return false;  // Always return "success".
}

void Modification_plan::register_in_thd() {
  thd->lock_query_plan();
  assert(thd->query_plan.get_modification_plan() == nullptr);
  thd->query_plan.set_modification_plan(this);
  thd->unlock_query_plan();
}

/**
  Modification_plan's constructor, to represent that we will use an access
  method on the table.

  @details
  Create single table modification plan. The plan is registered in the
  given thd unless the modification is done in a sub-statement
  (function/trigger).

  @param thd_arg        owning thread
  @param mt             modification type - MT_INSERT/MT_UPDATE/etc
  @param table_arg      Table to modify
  @param type_arg       Access type (JT_*) for this table
  @param range_scan_arg Range index scan used, if any
  @param condition_arg  Condition applied, if any
  @param key_arg        MAX_KEY or and index number of the key that was chosen
                        to access table data.
  @param limit_arg      HA_POS_ERROR or LIMIT value.
  @param need_tmp_table_arg true if it requires temporary table --
                        "Using temporary"
                        string in the "extra" column.
  @param need_sort_arg  true if it requires filesort() -- "Using filesort"
                        string in the "extra" column.
  @param used_key_is_modified_arg UPDATE updates used key column
  @param rows           How many rows we plan to modify in the table.
*/

Modification_plan::Modification_plan(
    THD *thd_arg, enum_mod_type mt, TABLE *table_arg, enum join_type type_arg,
    AccessPath *range_scan_arg, Item *condition_arg, uint key_arg,
    ha_rows limit_arg, bool need_tmp_table_arg, bool need_sort_arg,
    bool used_key_is_modified_arg, ha_rows rows)
    : thd(thd_arg),
      mod_type(mt),
      table(table_arg),
      type(type_arg),
      range_scan(range_scan_arg),
      condition(condition_arg),
      key(key_arg),
      limit(limit_arg),
      need_tmp_table(need_tmp_table_arg),
      need_sort(need_sort_arg),
      used_key_is_modified(used_key_is_modified_arg),
      message(nullptr),
      zero_result(false),
      examined_rows(rows) {
  assert(current_thd == thd);
  if (!thd->in_sub_stmt) register_in_thd();
}

/**
  Modification_plan's constructor, to convey a message in the "extra" column
  of EXPLAIN. This is for the case where this message is the main information
  (there is no access path to the table).

  @details
  Create minimal single table modification plan. The plan is registered in the
  given thd unless the modification is done in a sub-statement
  (function/trigger).

  @param thd_arg    Owning thread
  @param mt         Modification type - MT_INSERT/MT_UPDATE/etc
  @param table_arg  Table to modify
  @param message_arg Message
  @param zero_result_arg If we shortcut execution
  @param rows       How many rows we plan to modify in the table.
*/

Modification_plan::Modification_plan(THD *thd_arg, enum_mod_type mt,
                                     TABLE *table_arg, const char *message_arg,
                                     bool zero_result_arg, ha_rows rows)
    : thd(thd_arg),
      mod_type(mt),
      table(table_arg),
      key(MAX_KEY),
      limit(HA_POS_ERROR),
      need_tmp_table(false),
      need_sort(false),
      used_key_is_modified(false),
      message(message_arg),
      zero_result(zero_result_arg),
      examined_rows(rows) {
  assert(current_thd == thd);
  if (!thd->in_sub_stmt) register_in_thd();
}

Modification_plan::~Modification_plan() {
  if (!thd->in_sub_stmt) {
    thd->lock_query_plan();
    assert(current_thd == thd &&
           thd->query_plan.get_modification_plan() == this);
    thd->query_plan.set_modification_plan(nullptr);
    thd->unlock_query_plan();
  }
}

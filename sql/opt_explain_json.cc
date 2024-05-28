/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#include "sql/opt_explain_json.h"

#include <assert.h>
#include <sys/types.h>  // uint

#include <climits>
#include <cstddef>  // size_t
#include <cstdio>

#include "my_alloc.h"  // operator new
#include "my_compiler.h"

#include "mysql/strings/m_ctype.h"
#include "sql-common/json_dom.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/enum_query_type.h"
#include "sql/item.h"
#include "sql/item_sum.h"
#include "sql/key_spec.h"
#include "sql/mysqld.h"
#include "sql/opt_trace.h"          // Opt_trace_object
#include "sql/opt_trace_context.h"  // Opt_trace_context
#include "sql/protocol.h"           // Protocol
#include "sql/query_result.h"       // Query_result
#include "sql/sql_class.h"          // THD
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/window.h"
#include "sql_string.h"
#include "string_with_len.h"

class Query_expression;

/**
  Property names, former parts of traditional "extra" column

  This array must be in sync with Extra_tag enum.
*/
static const char *json_extra_tags[ET_total] = {
    nullptr,                               // ET_none
    "using_temporary_table",               // ET_USING_TEMPORARY
    "using_filesort",                      // ET_USING_FILESORT
    "index_condition",                     // ET_USING_INDEX_CONDITION
    nullptr,                               // ET_USING
    "range_checked_for_each_record",       // ET_RANGE_CHECKED_FOR_EACH_RECORD
    "pushed_condition",                    // ET_USING_PUSHED_CONDITION
    "using_where",                         // ET_USING_WHERE
    "not_exists",                          // ET_NOT_EXISTS
    "using_MRR",                           // ET_USING_MRR
    "using_index",                         // ET_USING_INDEX
    "full_scan_on_NULL_key",               // ET_FULL_SCAN_ON_NULL_KEY
    "using_index_for_group_by",            // ET_USING_INDEX_FOR_GROUP_BY
    "using_index_for_skip_scan",           // ET_USING_INDEX_FOR_SKIP_SCAN
    "distinct",                            // ET_DISTINCT
    "loosescan",                           // ET_LOOSESCAN
    nullptr,                               // ET_START_TEMPORARY
    nullptr,                               // ET_END_TEMPORARY
    "first_match",                         // ET_FIRST_MATCH
    nullptr,                               // ET_MATERIALIZE
    nullptr,                               // ET_START_MATERIALIZE
    nullptr,                               // ET_END_MATERIALIZE
    nullptr,                               // ET_SCAN
    "using_join_buffer",                   // ET_USING_JOIN_BUFFER
    "const_row_not_found",                 // ET_CONST_ROW_NOT_FOUND
    "unique_row_not_found",                // ET_UNIQUE_ROW_NOT_FOUND
    "impossible_on_condition",             // ET_IMPOSSIBLE_ON_CONDITION
    "pushed_join",                         // ET_PUSHED_JOIN
    "ft_hints",                            // ET_FT_HINTS
    "backward_index_scan",                 // ET_BACKWARD_SCAN
    "recursive",                           // ET_RECURSIVE
    "table_function",                      // ET_TABLE_FUNCTION
    "skip_records_in_range_due_to_force",  // ET_SKIP_RECORDS_IN_RANGE
    "using_secondary_engine",              // ET_USING_SECONDARY_ENGINE
    "rematerialize"                        // ET_REMATERIALIZE
};

// JSON key names
static const char K_ACCESS_TYPE[] = "access_type";
static const char K_ATTACHED_CONDITION[] = "attached_condition";
static const char K_ATTACHED_SUBQUERIES[] = "attached_subqueries";
static const char K_BUFFER_RESULT[] = "buffer_result";
static const char K_CACHEABLE[] = "cacheable";
static const char K_DEPENDENT[] = "dependent";
static const char K_DUPLICATES_REMOVAL[] = "duplicates_removal";
static const char K_FILTERED[] = "filtered";
static const char K_FRAME_BUFFER[] = "frame_buffer";
static const char K_FUNCTIONS[] = "functions";

static const char K_GROUPING_OPERATION[] = "grouping_operation";
static const char K_GROUP_BY_SUBQUERIES[] = "group_by_subqueries";
static const char K_HAVING_SUBQUERIES[] = "having_subqueries";
static const char K_INSERT_VALUES_SUBQUERIES[] = "insert_values_subqueries";
static const char K_INSERT_UPDATE_SUBQUERIES[] = "insert_update_subqueries";
static const char K_KEY[] = "key";
static const char K_KEY_LENGTH[] = "key_length";
static const char K_MATERIALIZED_FROM_SUBQUERY[] = "materialized_from_subquery";
static const char K_MESSAGE[] = "message";
static const char K_NAME[] = "name";
static const char K_NESTED_LOOP[] = "nested_loop";
static const char K_OPTIMIZED_AWAY_SUBQUERIES[] = "optimized_away_subqueries";
static const char K_OPTIMIZED_FRAME_EVALUATION[] = "optimized_frame_evaluation";
static const char K_ORDERING_OPERATION[] = "ordering_operation";
static const char K_ORDER_BY_SUBQUERIES[] = "order_by_subqueries";
static const char K_PARTITIONS[] = "partitions";
static const char K_POSSIBLE_KEYS[] = "possible_keys";
static const char K_QUERY_BLOCK[] = "query_block";
static const char K_QUERY_SPECIFICATIONS[] = "query_specifications";
static const char K_REF[] = "ref";
static const char K_SELECT_ID[] = "select_id";
static const char K_SELECT_LIST_SUBQUERIES[] = "select_list_subqueries";
static const char K_SHARING_TMP_TABLE[] = "sharing_temporary_table_with";
static const char K_TABLE[] = "table";
static const char K_TABLE_NAME[] = "table_name";
static const char K_UNION_RESULT[] = "union_result";
static const char K_INTERSECT_RESULT[] = "intersect_result";
static const char K_EXCEPT_RESULT[] = "except_result";
static const char K_UNARY_RESULT[] = "unary_result";
static const char K_UPDATE_VALUE_SUBQUERIES[] = "update_value_subqueries";
static const char K_USED_KEY_PARTS[] = "used_key_parts";
static const char K_USING_FILESORT[] = "using_filesort";
static const char K_FILESORT_KEY[] = "filesort_key";

static const char K_USING_TMP_TABLE[] = "using_temporary_table";

static const char K_WINDOW_DEF_POS[] = "definition_position";
static const char K_WINDOW_LAST_EXECUTED[] = "last_executed_window";
static const char K_WINDOWS[] = "windows";
static const char K_WINDOWING[] = "windowing";

static const char K_ROWS[] = "rows_examined_per_scan";
static const char K_PREFIX_ROWS[] = "rows_produced_per_join";

static const char K_COST_INFO[] = "cost_info";
static const char K_READ_TIME[] = "read_cost";
static const char K_PREFIX_COST[] = "prefix_cost";
static const char K_COND_COST[] = "eval_cost";
static const char K_SORT_COST[] = "sort_cost";
static const char K_QUERY_COST[] = "query_cost";
static const char K_DATA_SIZE_QUERY[] = "data_read_per_join";
static const char K_USED_COLUMNS[] = "used_columns";

static const char *mod_type_name[] = {"", "insert", "update", "delete",
                                      "replace"};

/*
  see commentary at the beginning of opt_trace.cc
*/
namespace opt_explain_json_namespace {

class joinable_ctx;
class sort_ctx;
class subquery_ctx;
class setop_result_ctx;
class window_ctx;

/**
  @note Keep in sync with the @c list_names array.
*/
enum subquery_list_enum {
  SQ_SELECT_LIST,     ///< SELECT list subqueries
  SQ_UPDATE_VALUE,    ///< UPDATE ... SET field=(subquery)
  SQ_INSERT_VALUES,   ///< subqueries in VALUES of INSERT ... VALUES
  SQ_INSERT_UPDATE,   ///< subqueries in UPDATE of
                      ///< INSERT ... ON DUPLICATE KEY UPDATE
  SQ_HAVING,          ///< HAVING clause subqueries
  SQ_OPTIMIZED_AWAY,  ///< "optimized_away_subqueries"
  //--------------
  SQ_toplevel,  ///< SQ array size for unit_ctx
  //--------------
  SQ_ORDER_BY,  ///< ORDER BY clause subqueries
  SQ_GROUP_BY,  ///< GROUP BY clause subqueries
  //--------------
  SQ_total
};

/**
  @note Keep in sync with @c subquery_list_enum.
*/
static const char *list_names[SQ_total] = {
    K_SELECT_LIST_SUBQUERIES,
    K_UPDATE_VALUE_SUBQUERIES,
    K_INSERT_VALUES_SUBQUERIES,
    K_INSERT_UPDATE_SUBQUERIES,
    K_HAVING_SUBQUERIES,
    K_OPTIMIZED_AWAY_SUBQUERIES,
    "",
    K_ORDER_BY_SUBQUERIES,
    K_GROUP_BY_SUBQUERIES,
};

/**
  Base class for all intermediate tree nodes
*/

class context : public Explain_context {
 protected:
  const char *name;

 public:
  context *parent;  ///< link to parent node or NULL

  context(enum_parsing_context type_arg, const char *name_arg,
          context *parent_arg)
      : Explain_context(type_arg), name(name_arg), parent(parent_arg) {}

  virtual ~context() = default;

  /**
    Pass the node with its child nodes to a JSON formatter

    @param json         Formatter

    @retval false       Ok
    @retval true        Error

    @note The @c join_ctx class overloads this function.
  */
  virtual bool format(Opt_trace_context *json) {
    Opt_trace_object obj(json, name);
    return format_body(json, &obj);
  }

  bool is_query_block() const { return name == K_QUERY_BLOCK; }

 private:
  /**
    Format JSON object body

    @param json         Formatter
    @param obj          Object of this body

    @retval false       Ok
    @retval true        Error
  */
  virtual bool format_body(Opt_trace_context *json, Opt_trace_object *obj) = 0;

 public:
  /**
    Analogue of the "id" column in the traditional EXPLAIN output

    @param hide         if true, ban the output of K_SELECT_ID JSON property
                        in the underlying table_with_where_and_derived_ctx
                        objects

    @returns "Select number" that is associated with this node
  */
  virtual size_t id(bool hide = false) = 0;

  virtual bool cacheable() {
    assert(0);
    return true;
  }
  virtual bool dependent() {
    assert(0);
    return false;
  }

  virtual class qep_row *entry() {
    assert(0);
    return nullptr;
  }
  virtual enum_mod_type get_mod_type() { return MT_NONE; }

  /**
    Associate a child node with this node

    This function is to be overloaded by subquery_ctx.
  */
  virtual void set_child(context *) {}

  /// associate CTX_UNION_RESULT node with CTX_UNION node
  virtual void set_setop_result(setop_result_ctx *) { assert(0); }

  /**
    Append a subquery node to the specified list of the unit node

    @param subquery_type    Describes the Item tree where the subquery exists
    @param ctx              Subquery node

    @retval false           Ok
    @retval true            Error
  */
  virtual bool add_subquery(subquery_list_enum subquery_type [[maybe_unused]],
                            subquery_ctx *ctx [[maybe_unused]]) {
    assert(0);
    return true;
  }
  /**
    Format nested loop join subtree (if any) to JSON formatter

    @param json                 Formatter

    @retval false               Ok
    @retval true                Error
  */
  virtual bool format_nested_loop(Opt_trace_context *json [[maybe_unused]]) {
    assert(0);
    return true;
  }

  /**
    Add a CTX_QEP_TAB node to a CTX_JOIN node

    @param ctx          CTX_QEP_TAB node

    @retval false           Ok
    @retval true            Error
  */
  virtual bool add_join_tab(joinable_ctx *ctx [[maybe_unused]]) {
    assert(0);
    return true;
  }

  /**
    Set nested ORDER BY/GROUP BY/DISTINCT node to @c ctx
  */
  virtual void set_sort(sort_ctx *ctx [[maybe_unused]]) { assert(0); }

  /**
    Set nested WINDOW node to @c ctx
  */
  virtual void set_window(window_ctx *ctx [[maybe_unused]]) { assert(0); }

  /**
    Add a query specification node to the CTX_UNION node

    @param ctx              query specification node

    @retval false           Ok
    @retval true            Error
  */
  virtual bool add_query_spec(context *ctx [[maybe_unused]]) { return false; }

  /**
    Try to associate a derived subquery node with this or underlying node

    @param subquery     Derived subquery node

    @retval true        Success
    @retval false       Can't associate: this node or its child nodes are not
                        derived from the subquery
  */
  virtual bool find_and_set_derived(context *subquery [[maybe_unused]]) {
    assert(0);
    return false;
  }

  /**
    Associate WHERE subqueries of given context and unit with this object

    @param ctx          Context of WHERE subquery
    @param subquery     For CTX_QEP_TAB: match given unit with a previously
                        collected by the register_where_subquery function.
    @returns
      -1   subquery wasn't found
       0   subqusery were added
       1   error occurred
  */
  virtual int add_where_subquery(subquery_ctx *ctx [[maybe_unused]],
                                 Query_expression *subquery [[maybe_unused]]) {
    assert(0);
    return false;
  }

  /// Helper function to format output for derived subquery if any
  virtual bool format_derived(Opt_trace_context *) { return false; }

  /// Helper function to format output for associated WHERE subqueries if any
  virtual bool format_where(Opt_trace_context *) { return false; }

  /// Helper function to format output for HAVING, ORDER/GROUP BY subqueries
  virtual bool format_query_expression(Opt_trace_context *) { return false; }
};

/**
  Node class to wrap a subquery node tree

  Implements CTX_WHERE, CTX_HAVING,  CTX_QUALIFY, CTX_ORDER_BY_SQ,
  CTX_GROUP_BY_SQ and CTX_OPTIMIZED_AWAY_SUBQUERY context nodes.
  This class hosts underlying join_ctx or uion_ctx.

  Is used for a subquery, a derived table.
*/

class subquery_ctx : virtual public context, public qep_row {
  /*
    TODO: After the conversion from multiple inheritace to templates
    convert "context" to "unit_ctx" (common base of uion_ctx & join_ctx).
  */
  context *subquery;  ///< hosted subquery tree: CTX_JOIN, CTX_UNION,
                      ///< CTX_INTERSECT, CTX_EXCEPT or CTX_UNARY

 public:
  subquery_ctx(enum_parsing_context type_arg, const char *name_arg,
               context *parent_arg)
      : context(type_arg, name_arg, parent_arg), subquery(nullptr) {}

  qep_row *entry() override { return this; }

  /*
    Materialized subquery statuses of dependency on the outer query and
    cacheability may differ from the source subquery, for example, if
    we "push down" the outer look up value for SJ.
    Thus, for materialized subqueries return direct is_cacheable and
    is_dependent values instead of source subquery statuses:
  */
  bool cacheable() override {
    return is_materialized_from_subquery ? is_cacheable : subquery->cacheable();
  }
  bool dependent() override {
    return is_materialized_from_subquery ? is_dependent : subquery->dependent();
  }

  bool format(Opt_trace_context *json) override {
    if (name)
      return context::format(json);
    else {
      /*
        Subquery is always a homogeneous array element,
        create anonymous  wrapper object:
      */
      Opt_trace_object anonymous_wrapper(json);
      return format_body(json, &anonymous_wrapper);
    }
  }

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    if (type == CTX_DERIVED) {
      if (derived_clone_id) {
        Opt_trace_object(json, K_SHARING_TMP_TABLE)
            .add(K_SELECT_ID, derived_clone_id);
        // Don't show underlying tables of derived table clone
        return false;
      }
      obj->add(K_USING_TMP_TABLE, true);
      obj->add(K_DEPENDENT, dependent());
      obj->add(K_CACHEABLE, cacheable());
      return subquery->format(json);
    } else if (using_temporary) {
      if (!is_materialized_from_subquery) {
        obj->add(K_USING_TMP_TABLE, true);
        obj->add(K_DEPENDENT, dependent());
        obj->add(K_CACHEABLE, cacheable());
      }

      {
        Opt_trace_object tmp_table(json, K_TABLE);

        if (!col_table_name.is_empty())
          tmp_table.add_utf8(K_TABLE_NAME, col_table_name.str);
        if (!col_join_type.is_empty())
          tmp_table.add_alnum(K_ACCESS_TYPE, col_join_type.str);
        if (!col_key.is_empty()) tmp_table.add_utf8(K_KEY, col_key.str);
        if (!col_key_len.is_empty())
          tmp_table.add_alnum(K_KEY_LENGTH, col_key_len.str);
        if (!col_rows.is_empty()) tmp_table.add(K_ROWS, col_rows.value);

        if (is_materialized_from_subquery) {
          Opt_trace_object materialized(json, K_MATERIALIZED_FROM_SUBQUERY);
          materialized.add(K_USING_TMP_TABLE, true);
          materialized.add(K_DEPENDENT, dependent());
          materialized.add(K_CACHEABLE, cacheable());
          return format_query_block(json);
        }
      }
      return format_query_block(json);
    } else {
      obj->add(K_DEPENDENT, dependent());
      obj->add(K_CACHEABLE, cacheable());
      return subquery->format(json);
    }
  }

  bool format_query_block(Opt_trace_context *json) {
    if (subquery->is_query_block()) return subquery->format(json);

    const Opt_trace_object query_block(json, K_QUERY_BLOCK);
    return subquery->format(json);
  }

 public:
  void set_child(context *child) override {
    assert(subquery == nullptr);
    assert(child->type == CTX_JOIN || child->type == CTX_UNION ||
           child->type == CTX_INTERSECT || child->type == CTX_EXCEPT ||
           child->type == CTX_UNARY);
    subquery = child;
  }

  size_t id(bool hide) override { return subquery->id(hide); }
};

/**
  Helper function to pass a subquery list to a JSON formatter

  @param json         output formatter
  @param subqueries   subquery list to output
  @param name         name for the output section

  @retval false       Ok
  @retval true        Error
*/
static bool format_list(Opt_trace_context *json, List<subquery_ctx> &subqueries,
                        const char *name) {
  if (!subqueries.is_empty()) {
    const Opt_trace_array subs(json, name);

    List_iterator<subquery_ctx> it(subqueries);
    subquery_ctx *t;
    while ((t = it++)) {
      // Homogeneous array: additional anonymous wrapper object is not needed
      if (t->format(json)) return true;
    }
  }
  return false;
}

/**
  Helper base class to host HAVING, ORDER BY and GROUP BY subquery nodes
*/
class unit_ctx : virtual public context {
  List<subquery_ctx> subquery_lists[SQ_total];

 public:
  unit_ctx(enum_parsing_context type_arg, const char *name_arg,
           context *parent_arg)
      : context(type_arg, name_arg, parent_arg) {}

  /**
    Helper function to distinguish subquery-less nodes

    @retval true        Node hosts no subqueries
    @retval false       Node hosts some subqueries
  */
  bool has_no_subqueries() const {
    for (size_t i = 0; i < SQ_total; i++) {
      if (!subquery_lists[i].is_empty()) return false;
    }
    return true;
  }

  bool format_query_expression(Opt_trace_context *json) override {
    for (size_t i = 0; i < SQ_total; i++) {
      if (format_list(json, subquery_lists[i], list_names[i])) return true;
    }
    return false;
  }

  bool add_subquery(subquery_list_enum subquery_type,
                    subquery_ctx *ctx) override {
    return subquery_lists[subquery_type].push_back(ctx);
  }
};

class table_base_ctx : virtual public context, virtual public qep_row {
 protected:
  bool is_hidden_id;  ///< if true, don't output K_SELECT_ID property

 public:
  table_base_ctx(enum_parsing_context type_arg, const char *name_arg,
                 context *parent_arg)
      : context(type_arg, name_arg, parent_arg), is_hidden_id(false) {}

  qep_row *entry() override { return this; }

 protected:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override;

 public:
  size_t id(bool) override { return col_id.is_empty() ? 0 : col_id.value; }

  bool cacheable() override { return is_cacheable; }
  bool dependent() override { return is_dependent; }
};

static void add_string_array(Opt_trace_context *json, const char *list_name,
                             List<const char> &strings) {
  if (!strings.is_empty()) {
    Opt_trace_array extra(json, list_name);

    List_iterator<const char> it(strings);
    const char *s;
    while ((s = it++)) extra.add_utf8(s);
  }
}

static void print_cost(char *buf, uint buf_len, double cost) {
  if (cost < 100000000000000.0)
    snprintf(buf, buf_len, "%.2f", cost);
  else
    snprintf(buf, buf_len, "%.14g", cost);
}

static void print_filtered(char *buf, uint buf_len, double filtered) {
  snprintf(buf, buf_len, "%.2f", filtered);
}

bool table_base_ctx::format_body(Opt_trace_context *json,
                                 Opt_trace_object *obj) {
  StringBuffer<64> buff;

  if (mod_type != MT_NONE) obj->add(mod_type_name[mod_type], true);

  if (!col_id.is_empty() && !is_hidden_id) obj->add(K_SELECT_ID, col_id.value);

  if (!col_table_name.is_empty())
    obj->add_utf8(K_TABLE_NAME, col_table_name.str);

  add_string_array(json, K_PARTITIONS, col_partitions);

  if (!col_join_type.is_empty())
    obj->add_alnum(K_ACCESS_TYPE, col_join_type.str);

  add_string_array(json, K_POSSIBLE_KEYS, col_possible_keys);

  if (!col_key.is_empty()) obj->add_utf8(K_KEY, col_key.str);

  if (!col_key_parts.is_empty())
    add_string_array(json, K_USED_KEY_PARTS, col_key_parts);

  if (!col_key_len.is_empty()) obj->add_alnum(K_KEY_LENGTH, col_key_len.str);

  add_string_array(json, K_REF, col_ref);

  if (!col_rows.is_empty()) obj->add(K_ROWS, col_rows.value);
  if (!col_prefix_rows.is_empty())
    obj->add(K_PREFIX_ROWS, col_prefix_rows.value);

  if (!col_filtered.is_empty()) {
    char buf[32];  // 32 is enough for digits of a double
    print_filtered(buf, sizeof(buf), col_filtered.value);
    obj->add_utf8(K_FILTERED, buf);
  }

  format_extra(obj);

  if (!col_read_cost.is_empty()) {
    Opt_trace_object cost_info(json, K_COST_INFO);
    char buf[32];  // 32 is enough for digits of a double

    print_cost(buf, sizeof(buf), col_read_cost.value);
    cost_info.add_utf8(K_READ_TIME, buf);

    if (!col_cond_cost.is_empty()) {
      print_cost(buf, sizeof(buf), col_cond_cost.value);
      cost_info.add_utf8(K_COND_COST, buf);
    }
    if (!col_prefix_cost.is_empty()) {
      print_cost(buf, sizeof(buf), col_prefix_cost.value);
      cost_info.add_utf8(K_PREFIX_COST, buf);
    }
    if (!col_data_size_query.is_empty())
      cost_info.add_utf8(K_DATA_SIZE_QUERY, col_data_size_query.str);
  }

  if (!col_used_columns.is_empty())
    add_string_array(json, K_USED_COLUMNS, col_used_columns);

  if (!col_partial_update_columns.is_empty())
    add_string_array(json, "partial_update_columns",
                     col_partial_update_columns);

  if (!col_message.is_empty() && type != CTX_MESSAGE) {
    assert(col_extra.is_empty());
    obj->add_alnum(K_MESSAGE, col_message.str);
  }

  {  // Keep together for better output readability
    if (!col_attached_condition.is_empty())
      obj->add_utf8(K_ATTACHED_CONDITION, col_attached_condition.str);
    if (format_where(json)) return true;
  }

  return format_derived(json) || format_query_expression(json);
}

/**
  Node class for the CTX_UNION_RESULT
*/
class setop_result_ctx : public table_base_ctx, public unit_ctx {
  List<context> *query_specs;  ///< query specification nodes (inner selects)
  List<subquery_ctx> order_by_subqueries;
  List<subquery_ctx> optimized_away_subqueries;
  joinable_ctx *message;

 public:
  explicit setop_result_ctx(context *parent_arg, enum_parsing_context ctx,
                            const char *str)
      : context(ctx, str, parent_arg),
        table_base_ctx(ctx, str, parent_arg),
        unit_ctx(ctx, str, parent_arg),
        message(nullptr) {}

  // Remove warnings: 'inherits ... from ... via dominance'
  size_t id(bool hide) override { return table_base_ctx::id(hide); }
  bool cacheable() override { return table_base_ctx::cacheable(); }
  bool dependent() override { return table_base_ctx::dependent(); }
  qep_row *entry() override { return table_base_ctx::entry(); }
  bool format_query_expression(Opt_trace_context *json) override {
    return table_base_ctx::format_query_expression(json);
  }

  void push_down_query_specs(List<context> *specs) { query_specs = specs; }

  bool add_subquery(subquery_list_enum subquery_type,
                    subquery_ctx *ctx) override {
    switch (subquery_type) {
      case SQ_ORDER_BY:
        return order_by_subqueries.push_back(ctx);
      case SQ_OPTIMIZED_AWAY:
        return optimized_away_subqueries.push_back(ctx);
      default:
        assert(!"Unknown query type!");
        return false;  // ignore in production
    }
  }

  bool add_join_tab(joinable_ctx *ctx) override {
    assert(!message);
    message = ctx;
    return false;
  }

  bool format(Opt_trace_context *json) override;

  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override;
};

/**
  Common part of CTX_QEP_TAB and CTX_MESSAGE nodes

  This class implements functionality for WHERE and derived subqueries that
  are associated with the table node.
*/
class table_with_where_and_derived : public table_base_ctx {
 public:
  List<subquery_ctx> where_subqueries;  ///< associated WHERE clause subqueries

  table_with_where_and_derived(enum_parsing_context type_arg,
                               const char *name_arg, context *parent_arg)
      : context(type_arg, name_arg, parent_arg),
        table_base_ctx(type_arg, name_arg, parent_arg) {}

  size_t id(bool hide) override {
    if (hide) is_hidden_id = true;
    return table_base_ctx::id(hide);
  }

  bool format_where(Opt_trace_context *json) override {
    return format_list(json, where_subqueries, K_ATTACHED_SUBQUERIES);
  }

  bool format_derived(Opt_trace_context *json) override {
    if (derived_from.elements == 0) return false;
    if (derived_from.elements == 1) return derived_from.head()->format(json);
    const Opt_trace_array loops(json, K_NESTED_LOOP);

    List_iterator<context> it(derived_from);
    context *c;
    while ((c = it++)) {
      const Opt_trace_object anonymous_wrapper(json);
      if (c->format(json)) return true;
    }
    return false;
  }
};

/**
  Base for CTX_QEP_TAB, CTX_DUPLICATES_WEEDOUT and CTX_MATERIALIZATION nodes

  This class implements a base to explain individual JOIN_TABs as well
  as JOIN_TAB groups like in semi-join materialization.
*/
class joinable_ctx : virtual public context {
 public:
  joinable_ctx(enum_parsing_context type_arg, const char *name_arg,
               context *parent_arg)
      : context(type_arg, name_arg, parent_arg) {}
};

/**
  Node class for CTX_MESSAGE

  This class is designed to represent fake tables with some messages in the
  "extra" column ("Impossible where" etc).
  We do EXPLAIN of these fake tables to replace explanation of:
    1) usual actual JOIN_TABs of the whole JOIN or
    2) a modifying TABLE of single-table UPDATE/DELETE/etc.
  So, message_ctx always represent a single half-empty fake table in a
  "query_block" node with optional subqueries.
*/
class message_ctx : public joinable_ctx, public table_with_where_and_derived {
 public:
  explicit message_ctx(context *parent_arg)
      : context(CTX_MESSAGE, K_TABLE, parent_arg),
        joinable_ctx(CTX_MESSAGE, K_TABLE, parent_arg),
        table_with_where_and_derived(CTX_MESSAGE, K_TABLE, parent_arg) {}

  // Remove warnings: 'inherits ... from ... via dominance'
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    return table_base_ctx::format_body(json, obj);
  }
  size_t id(bool hide) override {
    return table_with_where_and_derived::id(hide);
  }
  bool cacheable() override { return table_base_ctx::cacheable(); }
  bool dependent() override { return table_base_ctx::dependent(); }
  qep_row *entry() override { return table_base_ctx::entry(); }
  bool format_derived(Opt_trace_context *json) override {
    return table_with_where_and_derived::format_derived(json);
  }
  bool format_where(Opt_trace_context *json) override {
    return table_with_where_and_derived::format_where(json);
  }

  bool find_and_set_derived(context *subquery) override {
    /*
      message_ctx is designed to represent a single fake JOIN_TAB in the JOIN,
      so if the JOIN have a derived table, then this message_ctx represent this
      derived table.
      Unconditionally add subquery:
    */
    derived_from.push_back(subquery);
    return true;
  }

  int add_where_subquery(subquery_ctx *ctx, Query_expression *) override {
    return where_subqueries.push_back(ctx);
  }
};

/**
  Node class for the CTX_QEP_TAB context
*/
class join_tab_ctx : public joinable_ctx, public table_with_where_and_derived {
  /**
    Subquery units that are associated with this JOIN_TAB's condition

    This list is used to match with the @c subquery parameter of
    the @c add_where_subquery function.
  */
  List<Query_expression> where_subquery_units;

 public:
  join_tab_ctx(enum_parsing_context type_arg, context *parent_arg)
      : context(type_arg, K_TABLE, parent_arg),
        joinable_ctx(type_arg, K_TABLE, parent_arg),
        table_with_where_and_derived(type_arg, K_TABLE, parent_arg) {}

  // Remove warnings: 'inherits ... from ... via dominance'
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    return table_base_ctx::format_body(json, obj);
  }
  size_t id(bool hide) override {
    return table_with_where_and_derived::id(hide);
  }
  bool cacheable() override { return table_base_ctx::cacheable(); }
  bool dependent() override { return table_base_ctx::dependent(); }
  qep_row *entry() override { return table_base_ctx::entry(); }
  bool format_derived(Opt_trace_context *json) override {
    return table_with_where_and_derived::format_derived(json);
  }
  bool format_where(Opt_trace_context *json) override {
    return table_with_where_and_derived::format_where(json);
  }

  void register_where_subquery(Query_expression *subquery) override {
    List_iterator<Query_expression> it(where_subquery_units);
    Query_expression *u;
    while ((u = it++)) {
      /*
        The server may transform (x = (SELECT FROM DUAL)) to
        (x <=> (SELECT FROM DUAL) AND x = (SELECT FROM DUAL)),
        so ignore duplicates:
      */
      if (u == subquery) return;
    }
    where_subquery_units.push_back(subquery);
  }

  int add_where_subquery(subquery_ctx *ctx,
                         Query_expression *subquery) override {
    List_iterator<Query_expression> it(where_subquery_units);
    Query_expression *u;
    while ((u = it++)) {
      if (u == subquery) return where_subqueries.push_back(ctx);
    }
    return -1;
  }

  bool find_and_set_derived(context *subquery) override {
    if (query_block_id == subquery->id()) {
      derived_from.push_back(subquery);
      return true;
    }
    return false;
  }
  enum_mod_type get_mod_type() override { return entry()->mod_type; }
};

/**
  Base class for CTX_ORDER_BY, CTX_GROUP_BY and node class for CTX_DISTINCT

  This class represents context for simple ORDER BY/GROUP BY/DISTINCT clauses
  (the clause is effective for the single JOIN_TAB).
*/

class simple_sort_ctx : public joinable_ctx {
 protected:
  /** Single JOIN_TAB that we sort. */
  joinable_ctx *join_tab;

 private:
  /** True if the clause creates intermediate table. */
  const bool using_tmptable;
  /** True if the clause uses filesort. */
  const bool using_filesort;

 public:
  simple_sort_ctx(enum_parsing_context type_arg, const char *name_arg,
                  context *parent_arg, const Explain_format_flags *flags,
                  Explain_sort_clause clause)
      : context(type_arg, name_arg, parent_arg),
        joinable_ctx(type_arg, name_arg, parent_arg),
        join_tab(nullptr),
        using_tmptable(flags->get(clause, ESP_USING_TMPTABLE)),
        using_filesort(flags->get(clause, ESP_USING_FILESORT)) {}

  bool add_join_tab(joinable_ctx *ctx) override {
    join_tab = ctx;
    return false;
  }

  int add_where_subquery(subquery_ctx *ctx,
                         Query_expression *subquery) override {
    return join_tab->add_where_subquery(ctx, subquery);
  }

  bool find_and_set_derived(context *subquery) override {
    return join_tab->find_and_set_derived(subquery);
  }

  size_t id(bool hide) override { return join_tab->id(hide); }
  bool cacheable() override { return join_tab->cacheable(); }
  bool dependent() override { return join_tab->dependent(); }

 protected:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    if (using_tmptable) obj->add(K_USING_TMP_TABLE, true);
    obj->add(K_USING_FILESORT, using_filesort);
    return join_tab->format(json);
  }
};

/**
  Node class for "simple" CTX_ORDER_BY and CTX_GROUP_BY

  This class represents context for simple ORDER BY or GROUP BY clauses
  (the clause is effective for the single JOIN_TAB).
*/

class simple_sort_with_subqueries_ctx : public simple_sort_ctx {
  /** Type of this clause subqueries. */
  const subquery_list_enum subquery_type;
  List<subquery_ctx> subqueries;

 public:
  simple_sort_with_subqueries_ctx(enum_parsing_context type_arg,
                                  const char *name_arg, context *parent_arg,
                                  subquery_list_enum subquery_type_arg,
                                  const Explain_format_flags *flags,
                                  Explain_sort_clause clause)
      : context(type_arg, name_arg, parent_arg),
        simple_sort_ctx(type_arg, name_arg, parent_arg, flags, clause),
        subquery_type(subquery_type_arg) {}

  bool add_subquery(subquery_list_enum subquery_type_arg,
                    subquery_ctx *ctx) override {
    if (subquery_type != subquery_type_arg)
      return simple_sort_ctx::add_subquery(subquery_type_arg, ctx);
    return subqueries.push_back(ctx);
  }

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    return (simple_sort_ctx::format_body(json, obj) ||
            (format_list(json, subqueries, list_names[subquery_type])));
  }
};

/**
  Node class for the CTX_JOIN context
*/

class join_ctx : public unit_ctx, virtual public qep_row {
 protected:
  List<joinable_ctx> join_tabs;  ///< hosted JOIN_TAB nodes
  sort_ctx *sort;
  window_ctx *window;

 public:
  join_ctx(enum_parsing_context type_arg, const char *name_arg,
           context *parent_arg)
      : context(type_arg, name_arg, parent_arg),
        unit_ctx(type_arg, name_arg, parent_arg),
        sort(nullptr),
        window(nullptr) {}

  bool add_join_tab(joinable_ctx *ctx) override {
    return join_tabs.push_back(ctx);
  }

  void set_sort(sort_ctx *ctx) override {
    assert(!sort);
    sort = ctx;
  }

  void set_window(window_ctx *ctx) override {
    assert(!sort);
    window = ctx;
  }

  qep_row *entry() override { return this; }

  /**
    Associate a CTX_DERIVED node with its CTX_QEP_TAB node

    @param subquery     derived subquery tree
  */
  bool find_and_set_derived(context *subquery) override;

  bool add_subquery(subquery_list_enum subquery_type,
                    subquery_ctx *ctx) override;

 protected:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override;
  bool format_body_inner(Opt_trace_context *json, Opt_trace_object *obj);
  virtual const char *get_cost_tag() { return K_QUERY_COST; }

 public:
  bool format_nested_loop(Opt_trace_context *json) override;
  size_t id(bool hide) override;
  bool cacheable() override;
  bool dependent() override;
  int add_where_subquery(subquery_ctx *ctx,
                         Query_expression *subquery) override;
};

/**
  Node class for CTX_SIMPLE_ORDER_BY, CTX_SIMPLE_GROUP_BY and
CTX_SIMPLE_DISTINCT

  CTX_JOIN context (see join_ctx class) may contain nested loop join node *or*
  ORDER BY/GROUP BY/DISTINCT node that is represented by this class:

    join: { nested_loop: [ ... ] }
  or
    join: { order_by|group_by|distinct : { ... } }

  CTX_ORDER_BY may contain nested loop join tree *or* GROUP BY/DISTINCT node:

    order_by: { nested_loop|group_by|distinct: ... }

  CTX_DISTINCT context structure:

    distinct: { nested_loop|group_by: ... }

  CTX_GROUP_BY:

    group_by: { nested_loop: [ ... ] }

  I.e. the most complex CTX_JOIN may have such a structure of JSON output as:

    join: {
      order_by: {
        distinct: {
          group_by: {
            nested_loop: [ ... ]
          }
        }
      }
    }
TODO
*/

class sort_ctx : public join_ctx {
  /** The clause creates temporary table. */
  const bool using_tmptable;
  /** The clause uses filesort. */
  const bool using_filesort;

 public:
  sort_ctx(enum_parsing_context type_arg, const char *name_arg,
           context *parent_arg, const Explain_format_flags *flags,
           Explain_sort_clause clause)
      : context(type_arg, name_arg, parent_arg),
        join_ctx(type_arg, name_arg, parent_arg),
        using_tmptable(flags->get(clause, ESP_USING_TMPTABLE)),
        using_filesort(flags->get(clause, ESP_USING_FILESORT)) {}

 protected:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    assert(!sort || join_tabs.is_empty());

    if (using_tmptable) obj->add(K_USING_TMP_TABLE, true);
    if (type != CTX_BUFFER_RESULT) obj->add(K_USING_FILESORT, using_filesort);

    return join_ctx::format_body(json, obj);
  }
  const char *get_cost_tag() override { return K_SORT_COST; }
};

class sort_with_subqueries_ctx : public sort_ctx {
  /** Subquery type for this clause. */
  const subquery_list_enum subquery_type;
  List<subquery_ctx> subqueries;

 public:
  sort_with_subqueries_ctx(enum_parsing_context type_arg, const char *name_arg,
                           context *parent_arg,
                           subquery_list_enum subquery_type_arg,
                           const Explain_format_flags *flags,
                           Explain_sort_clause clause)
      : context(type_arg, name_arg, parent_arg),
        sort_ctx(type_arg, name_arg, parent_arg, flags, clause),
        subquery_type(subquery_type_arg) {}

  bool add_subquery(subquery_list_enum subquery_type_arg,
                    subquery_ctx *ctx) override {
    if (subquery_type_arg != subquery_type)
      return sort_ctx::add_subquery(subquery_type_arg, ctx);
    return subqueries.push_back(ctx);
  }

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    return (sort_ctx::format_body(json, obj) ||
            format_list(json, subqueries, list_names[subquery_type]));
  }
};

/**
  When the query has window functions, an outer node named "windowing" is
  added:
    query_block: {
      select_id: 1,
      windowing: {
        windows: [ ... list of all windows' details ...
        ],
        nested_loop: [ ... ]

  Due to this "outer-node" layout, the implementation of window_ctx is similar
  to that of sort_ctx. Except that while it makes sense to describe to the
  user the "subqueries included in ORDER BY", it doesn't for "subqueries
  included in the window function's arguments" (the window function belongs to
  the SELECT list or ORDER BY: so does the subquery, simply), so there is no
  window_with_subqueries_ctx.
*/
class window_ctx : public join_ctx {
 public:
  window_ctx(context *parent_arg)
      : context(CTX_WINDOW, K_WINDOWING, parent_arg),
        join_ctx(CTX_WINDOW, K_WINDOWING, parent_arg) {}

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    Opt_trace_array windows(json, K_WINDOWS);
    List_iterator<Window> li(*m_windows);
    Window *w;

    while ((w = li++)) {
      Opt_trace_object to(json);
      to.add_utf8(K_NAME, w->printable_name());
      if (m_windows->elements > 1) {
        // Help the user relate this to a window in his query:
        if (w->def_pos())
          to.add(K_WINDOW_DEF_POS, w->def_pos());
        else
          to.add_alnum(K_WINDOW_DEF_POS, "artificial_window");  // dummy
        // Make him notice the top-to-bottom order of execution of windows:
        if (w->is_last()) to.add(K_WINDOW_LAST_EXECUTED, true);
      }
      if (!w->short_circuit()) to.add(K_USING_TMP_TABLE, true);
      if (w->needs_sorting()) {
        obj->add(K_USING_FILESORT, true);
        Opt_trace_array sort_order(json, K_FILESORT_KEY);
        ORDER *ord = w->sorting_order();
        for (; ord != nullptr; ord = ord->next) {
          String str;
          (*ord->item)
              ->print_for_order(current_thd, &str,
                                (enum_query_type)(QT_NO_DB | QT_NO_TABLE),
                                ord->used_alias);
          if (ord->direction == ORDER_DESC)
            str.append(STRING_WITH_LEN(" desc"));
          sort_order.add_utf8(str.ptr(), str.length());
        }
      }
      if (w->needs_buffering()) {
        Opt_trace_object to_buf(json, K_FRAME_BUFFER);
        to_buf.add(K_USING_TMP_TABLE, true);
        if (w->optimizable_range_aggregates() ||
            w->optimizable_row_aggregates() || w->static_aggregates())
          to_buf.add(K_OPTIMIZED_FRAME_EVALUATION, true);
      }
      Opt_trace_array wfs(json, K_FUNCTIONS);
      List_iterator<Item_sum> wfs_it(w->functions());
      Item_sum *wf;
      while ((wf = wfs_it++)) wfs.add_utf8(wf->func_name());
    }

    windows.end();
    return join_ctx::format_body(json, obj);
  }

 protected:
  const char *get_cost_tag() override { return K_SORT_COST; }
};

bool join_ctx::find_and_set_derived(context *subquery) {
  assert(subquery->id() != 0);

  if (sort) return sort->find_and_set_derived(subquery);
  if (window) return window->find_and_set_derived(subquery);

  List_iterator<joinable_ctx> it(join_tabs);
  joinable_ctx *t;
  while ((t = it++)) {
    if (t->find_and_set_derived(subquery)) return true;
  }
  return false;
}

bool join_ctx::add_subquery(subquery_list_enum subquery_type,
                            subquery_ctx *ctx) {
  if (sort) return sort->add_subquery(subquery_type, ctx);
  if (window) return window->add_subquery(subquery_type, ctx);

  if (subquery_type > SQ_toplevel) {
    List_iterator<joinable_ctx> it(join_tabs);
    joinable_ctx *j;
    while ((j = it++)) {
      switch (j->type) {
        case CTX_ORDER_BY:
        case CTX_DISTINCT:
        case CTX_GROUP_BY:
        case CTX_SIMPLE_ORDER_BY:
        case CTX_SIMPLE_DISTINCT:
        case CTX_SIMPLE_GROUP_BY:
          return j->add_subquery(subquery_type, ctx);
        case CTX_MESSAGE:  // The 'no plan' case
          assert(subquery_type == SQ_ORDER_BY || subquery_type == SQ_GROUP_BY);
          return unit_ctx::add_subquery(subquery_type, ctx);
        default:
          assert(0); /* purecov: inspected */
      }
    }
  } else
    return unit_ctx::add_subquery(subquery_type, ctx);
  return true; /* purecov: inspected */
}

bool join_ctx::format_body(Opt_trace_context *json, Opt_trace_object *obj) {
  if (type == CTX_JOIN) obj->add(K_SELECT_ID, id(true));

  format_extra(obj);

  if (!col_read_cost.is_empty()) {
    char buf[32];  // 32 is enough for digits of a double

    Opt_trace_object cost_info(json, K_COST_INFO);
    print_cost(buf, sizeof(buf), col_read_cost.value);
    cost_info.add_utf8(get_cost_tag(), buf);
  }
  // Print target table for INSERT/REPLACE SELECT outside of nested loop
  if (join_tabs.elements && (join_tabs.head()->get_mod_type() == MT_INSERT ||
                             join_tabs.head()->get_mod_type() == MT_REPLACE)) {
    join_tabs.head()->format(json);
    if (sort || join_tabs.elements > 1) {
      const Opt_trace_object insert_from(json, "insert_from");
      if (format_body_inner(json, obj)) return true; /* purecov: inspected */
    }
  } else if (format_body_inner(json, obj))
    return true; /* purecov: inspected */
  return format_query_expression(json);
}

bool join_ctx::format_body_inner(Opt_trace_context *json,
                                 Opt_trace_object *obj) {
  if (sort) {
    if (sort->format(json)) return true; /* purecov: inspected */
  } else if (window) {
    if (window->format(json)) return true; /* purecov: inspected */
  } else if (join_tabs.elements && join_tabs.head()->type == CTX_MESSAGE) {
    // Could be only 1 message per join
    assert(join_tabs.elements == 1);
    message_ctx *msg = (message_ctx *)join_tabs.head();
    obj->add_alnum(K_MESSAGE, msg->entry()->col_message.str);
    if (msg->derived_from.elements)
      msg->format(json);
    else if (msg->where_subqueries.elements)
      msg->format_where(json);
  } else if (format_nested_loop(json))
    return true;
  return false;
}

bool join_ctx::format_nested_loop(Opt_trace_context *json) {
  List_iterator<joinable_ctx> it(join_tabs);
  uint join_tab_num = join_tabs.elements;
  assert(join_tabs.elements > 0);

  if (join_tabs.head()->get_mod_type() == MT_INSERT ||
      join_tabs.head()->get_mod_type() == MT_REPLACE) {
    it++;
    join_tab_num--;
  }
  /*
    For single table skip "nested_loop" object creation and
    format its contents only (the 1st join_tab).
  */
  if (join_tab_num == 1) return (it++)->format(json);

  const Opt_trace_array loops(json, K_NESTED_LOOP);

  joinable_ctx *t;
  while ((t = it++)) {
    const Opt_trace_object anonymous_wrapper(json);
    if (t->format(json)) return true;
  }
  return false;
}

bool setop_result_ctx::format_body(Opt_trace_context *json,
                                   Opt_trace_object *obj) {
  obj->add(K_USING_TMP_TABLE, true);

  if (table_base_ctx::format_body(json, obj))
    return true; /* purecov: inspected */

  if (message) {
    auto *msg = (message_ctx *)message;
    obj->add_alnum(K_MESSAGE, msg->entry()->col_message.str);
  }

  const Opt_trace_array specs(json, K_QUERY_SPECIFICATIONS);

  List_iterator<context> it(*query_specs);
  context *ctx;
  while ((ctx = it++)) {
    if (ctx->format(json)) return true; /* purecov: inspected */
  }
  return false;
}

/**
  Auxiliary function to walk through the list and propagate "hide" value

  @param list   list of context (*_ctx)  objects
  @param hide   if true, ban the output of K_SELECT_ID JSON property
                in the underlying table_with_where_and_derived_ctx
                and materialize_ctx objects

  @return       id of underlying objects
*/
template <typename T>
static size_t get_id(List<T> &list, bool hide) {
  if (!hide) return list.head()->id();

  List_iterator<T> it(list);
  T *j;
  size_t ret = 0;
  while ((j = it++)) ret = j->id(hide);
  return ret;
}

size_t join_ctx::id(bool hide) {
  return (sort ? sort->id(hide)
               : (window ? window->id(hide) : get_id(join_tabs, hide)));
}

bool join_ctx::cacheable() {
  return (sort
              ? sort->cacheable()
              : (window ? window->cacheable() : join_tabs.head()->cacheable()));
}

bool join_ctx::dependent() {
  return (sort
              ? sort->dependent()
              : (window ? window->dependent() : join_tabs.head()->dependent()));
}

int join_ctx::add_where_subquery(subquery_ctx *ctx,
                                 Query_expression *subquery) {
  if (sort) return sort->join_ctx::add_where_subquery(ctx, subquery);
  if (window) return window->join_ctx::add_where_subquery(ctx, subquery);

  List_iterator<joinable_ctx> it(join_tabs);
  joinable_ctx *j;
  bool found = false;
  while ((j = it++)) {
    const int ret = j->add_where_subquery(ctx, subquery);
    if (ret > 0) return true;
    found |= (ret == 0);
  }
  if (!found) return add_subquery(SQ_OPTIMIZED_AWAY, ctx);
  return false;
}

/**
  Context class to group materialized JOIN_TABs to "materialized" array.
  Is used for semijoin materialization.
*/

class materialize_ctx : public joinable_ctx,
                        public join_ctx,
                        public table_base_ctx {
 public:
  explicit materialize_ctx(context *parent_arg)
      : context(CTX_MATERIALIZATION, K_TABLE, parent_arg),
        joinable_ctx(CTX_MATERIALIZATION, K_TABLE, parent_arg),
        join_ctx(CTX_MATERIALIZATION, K_TABLE, parent_arg),
        table_base_ctx(CTX_MATERIALIZATION, K_TABLE, parent_arg) {}

  size_t id(bool hide) override {
    if (hide) {
      is_hidden_id = true;
      /* Set the materizlize table's id to hide */
      join_ctx::id(hide);
    }
    return table_base_ctx::id(hide);
  }
  bool cacheable() override { return join_ctx::cacheable(); }
  bool dependent() override { return join_ctx::dependent(); }

  // Remove warnings: 'inherits ... from ... via dominance'
  qep_row *entry() override { return table_base_ctx::entry(); }
  bool add_subquery(subquery_list_enum subquery_type,
                    subquery_ctx *ctx) override {
    return join_ctx::add_subquery(subquery_type, ctx);
  }
  bool add_join_tab(joinable_ctx *ctx) override {
    return join_ctx::add_join_tab(ctx);
  }
  int add_where_subquery(subquery_ctx *ctx,
                         Query_expression *subquery) override {
    return join_ctx::add_where_subquery(ctx, subquery);
  }
  bool find_and_set_derived(context *subquery) override {
    return join_ctx::find_and_set_derived(subquery);
  }
  bool format_query_expression(Opt_trace_context *json) override {
    return unit_ctx::format_query_expression(json);
  }
  bool format_nested_loop(Opt_trace_context *json) override {
    return join_ctx::format_nested_loop(json);
  }
  void set_sort(sort_ctx *ctx) override { return join_ctx::set_sort(ctx); }
  void set_window(window_ctx *ctx) override {
    return join_ctx::set_window(ctx);
  }

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    assert(!col_join_type.is_empty());

    if (!col_table_name.is_empty())
      obj->add_utf8(K_TABLE_NAME, col_table_name.str);

    obj->add_alnum(K_ACCESS_TYPE, col_join_type.str);

    if (!col_key.is_empty()) obj->add_utf8(K_KEY, col_key.str);

    if (!col_key_len.is_empty()) obj->add_alnum(K_KEY_LENGTH, col_key_len.str);

    add_string_array(json, K_REF, col_ref);

    if (!col_rows.is_empty()) obj->add(K_ROWS, col_rows.value);

    format_extra(obj);

    /*
      Currently K-REF/col_ref is not shown; it would always be "func", since
      {subquery,semijoin} materialization use store_key; using
      get_store_key() instead would allow "const" and outer column's name,
      if applicable.
      The looked up expression can anyway be inferred from the condition:
    */
    if (!col_attached_condition.is_empty())
      obj->add_utf8(K_ATTACHED_CONDITION, col_attached_condition.str);
    if (format_where(json)) return true;

    const Opt_trace_object m(json, K_MATERIALIZED_FROM_SUBQUERY);
    obj->add(K_USING_TMP_TABLE, true);
    const Opt_trace_object q(json, K_QUERY_BLOCK);
    return format_nested_loop(json);
  }
};

/**
  Context class to represent JOIN_TABs in duplication weedout sequence
*/

class duplication_weedout_ctx : public joinable_ctx, public join_ctx {
 public:
  explicit duplication_weedout_ctx(context *parent_arg)
      : context(CTX_DUPLICATES_WEEDOUT, K_DUPLICATES_REMOVAL, parent_arg),
        joinable_ctx(CTX_DUPLICATES_WEEDOUT, K_DUPLICATES_REMOVAL, parent_arg),
        join_ctx(CTX_DUPLICATES_WEEDOUT, K_DUPLICATES_REMOVAL, parent_arg) {}

  size_t id(bool hide) override { return join_ctx::id(hide); }
  bool cacheable() override { return join_ctx::cacheable(); }
  bool dependent() override { return join_ctx::dependent(); }

  // Remove warnings: 'inherits ... from ... via dominance'
  bool add_join_tab(joinable_ctx *ctx) override {
    return join_ctx::add_join_tab(ctx);
  }
  bool add_subquery(subquery_list_enum subquery_type,
                    subquery_ctx *ctx) override {
    return join_ctx::add_subquery(subquery_type, ctx);
  }
  int add_where_subquery(subquery_ctx *ctx,
                         Query_expression *subquery) override {
    return join_ctx::add_where_subquery(ctx, subquery);
  }
  bool find_and_set_derived(context *subquery) override {
    return join_ctx::find_and_set_derived(subquery);
  }
  bool format_nested_loop(Opt_trace_context *json) override {
    return join_ctx::format_nested_loop(json);
  }
  bool format_query_expression(Opt_trace_context *json) override {
    return unit_ctx::format_query_expression(json);
  }
  void set_sort(sort_ctx *ctx) override { return join_ctx::set_sort(ctx); }
  void set_window(window_ctx *ctx) override {
    return join_ctx::set_window(ctx);
  }
  qep_row *entry() override { return join_ctx::entry(); }

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *obj) override {
    obj->add(K_USING_TMP_TABLE, true);
    return format_nested_loop(json);
  }
};

/**
  Node class for UNION, EXCEPT, INTERSECT, UNARY (query expression)
*/

class setop_ctx : public unit_ctx {
  setop_result_ctx *setop_result;  ///< associated e.g. CTX_UNION_RESULT node
  List<context> query_specs;  ///< query specification nodes (inner selects)

 public:
  explicit setop_ctx(context *parent_arg, enum_parsing_context ctx,
                     const char *str)
      : context(ctx, str, parent_arg),
        unit_ctx(ctx, str, parent_arg),
        setop_result(nullptr) {}

 private:
  bool format_body(Opt_trace_context *json, Opt_trace_object *) override {
    if (setop_result)
      return (setop_result->format(json)) || format_query_expression(json);
    else {
      /*
        UNION without temporary table. There is no union_result since
        there is no fake_query_block.
      */
      Opt_trace_object union_res(json, K_UNION_RESULT);
      union_res.add(K_USING_TMP_TABLE, false);
      const Opt_trace_array specs(json, K_QUERY_SPECIFICATIONS);
      List_iterator<context> it(query_specs);
      context *ctx;
      while ((ctx = it++)) {
        if (ctx->format(json)) return true; /* purecov: inspected */
      }
      return format_query_expression(json);
    }
  }

 public:
  size_t id(bool hide) override { return get_id(query_specs, hide); }
  bool cacheable() override { return query_specs.head()->cacheable(); }
  bool dependent() override { return query_specs.head()->dependent(); }

  void set_setop_result(setop_result_ctx *ctx) override {
    assert(setop_result == nullptr);
    setop_result = ctx;
    setop_result->push_down_query_specs(&query_specs);
  }
  bool add_query_spec(context *ctx) override {
    return query_specs.push_back(ctx);
  }
};

bool setop_result_ctx::format(Opt_trace_context *json) {
  if (order_by_subqueries.is_empty() && optimized_away_subqueries.is_empty())
    return table_base_ctx::format(json);

  Opt_trace_object order_by(json, K_ORDERING_OPERATION);

  order_by.add(K_USING_FILESORT, !order_by_subqueries.is_empty());

  if (table_base_ctx::format(json)) return true; /* purecov: inspected */

  if (!order_by_subqueries.is_empty() &&
      format_list(json, order_by_subqueries, K_ORDER_BY_SUBQUERIES))
    return true; /* purecov: inspected */

  if (!optimized_away_subqueries.is_empty() &&
      format_list(json, optimized_away_subqueries, K_OPTIMIZED_AWAY_SUBQUERIES))
    return true; /* purecov: inspected */

  return false;
}

}  // namespace opt_explain_json_namespace

qep_row *Explain_format_JSON::entry() { return current_context->entry(); }

bool Explain_format_JSON::begin_context(enum_parsing_context ctx_arg,
                                        Query_expression *subquery,
                                        const Explain_format_flags *flags) {
  using namespace opt_explain_json_namespace;

  context *prev_context = current_context;
  switch (ctx_arg) {
    case CTX_JOIN:
      assert(current_context == nullptr || current_context->type == CTX_UNION ||
             current_context->type == CTX_INTERSECT ||
             current_context->type == CTX_EXCEPT ||
             current_context->type == CTX_UNARY ||
             // subqueries:
             current_context->type == CTX_SELECT_LIST ||
             current_context->type == CTX_UPDATE_VALUE ||
             current_context->type == CTX_INSERT_VALUES ||
             current_context->type == CTX_INSERT_UPDATE ||
             current_context->type == CTX_DERIVED ||
             current_context->type == CTX_OPTIMIZED_AWAY_SUBQUERY ||
             current_context->type == CTX_WHERE ||
             current_context->type == CTX_HAVING ||
             current_context->type == CTX_QUALIFY ||
             current_context->type == CTX_ORDER_BY_SQ ||
             current_context->type == CTX_GROUP_BY_SQ ||
             current_context->type == CTX_QUERY_SPEC);
      if ((current_context = new (*THR_MALLOC)
               join_ctx(CTX_JOIN, K_QUERY_BLOCK, current_context)) == nullptr)
        return true;
      break;
    case CTX_ORDER_BY: {
      assert(current_context->type == CTX_JOIN);
      sort_ctx *ctx = new (*THR_MALLOC) sort_with_subqueries_ctx(
          CTX_ORDER_BY, K_ORDERING_OPERATION, current_context, SQ_ORDER_BY,
          flags, ESC_ORDER_BY);
      if (ctx == nullptr) return true;
      current_context->set_sort(ctx);
      current_context = ctx;
      break;
    }
    case CTX_GROUP_BY: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW);
      sort_ctx *ctx = new (*THR_MALLOC) sort_with_subqueries_ctx(
          CTX_GROUP_BY, K_GROUPING_OPERATION, current_context, SQ_GROUP_BY,
          flags, ESC_GROUP_BY);
      if (ctx == nullptr) return true;
      current_context->set_sort(ctx);
      current_context = ctx;
      break;
    }
    case CTX_DISTINCT: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_ORDER_BY);
      sort_ctx *ctx =
          new (*THR_MALLOC) sort_ctx(CTX_DISTINCT, K_DUPLICATES_REMOVAL,
                                     current_context, flags, ESC_DISTINCT);
      if (ctx == nullptr) return true;
      current_context->set_sort(ctx);
      current_context = ctx;
      break;
    }
    case CTX_BUFFER_RESULT: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_GROUP_BY);
      sort_ctx *ctx =
          new (*THR_MALLOC) sort_ctx(CTX_BUFFER_RESULT, K_BUFFER_RESULT,
                                     current_context, flags, ESC_BUFFER_RESULT);
      if (ctx == nullptr) return true;
      current_context->set_sort(ctx);
      current_context = ctx;
      break;
    }
    case CTX_QEP_TAB: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_MATERIALIZATION ||
             current_context->type == CTX_DUPLICATES_WEEDOUT ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_SIMPLE_GROUP_BY ||
             current_context->type == CTX_SIMPLE_ORDER_BY ||
             current_context->type == CTX_SIMPLE_DISTINCT);
      join_tab_ctx *ctx =
          new (*THR_MALLOC) join_tab_ctx(CTX_QEP_TAB, current_context);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_SIMPLE_ORDER_BY: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_MATERIALIZATION ||
             current_context->type == CTX_DUPLICATES_WEEDOUT ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_DISTINCT);
      simple_sort_ctx *ctx = new (*THR_MALLOC) simple_sort_with_subqueries_ctx(
          CTX_SIMPLE_ORDER_BY, K_ORDERING_OPERATION, current_context,
          SQ_ORDER_BY, flags, ESC_ORDER_BY);

      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_SIMPLE_GROUP_BY: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_MATERIALIZATION ||
             current_context->type == CTX_DUPLICATES_WEEDOUT ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_SIMPLE_ORDER_BY ||
             current_context->type == CTX_SIMPLE_DISTINCT);
      simple_sort_ctx *ctx = new (*THR_MALLOC) simple_sort_with_subqueries_ctx(
          CTX_SIMPLE_GROUP_BY, K_GROUPING_OPERATION, current_context,
          SQ_GROUP_BY, flags, ESC_GROUP_BY);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_SIMPLE_DISTINCT: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_MATERIALIZATION ||
             current_context->type == CTX_DUPLICATES_WEEDOUT ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_SIMPLE_ORDER_BY);
      simple_sort_ctx *ctx = new (*THR_MALLOC)
          simple_sort_ctx(CTX_SIMPLE_DISTINCT, K_DUPLICATES_REMOVAL,
                          current_context, flags, ESC_DISTINCT);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_MATERIALIZATION: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_DUPLICATES_WEEDOUT);
      materialize_ctx *ctx = new (*THR_MALLOC) materialize_ctx(current_context);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_DUPLICATES_WEEDOUT: {
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_MATERIALIZATION);
      duplication_weedout_ctx *ctx =
          new (*THR_MALLOC) duplication_weedout_ctx(current_context);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_SELECT_LIST: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_SELECT_LIST, nullptr, current_context);
      if (ctx == nullptr || current_context->add_subquery(SQ_SELECT_LIST, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_UPDATE_VALUE: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_UPDATE_VALUE, nullptr, current_context);
      if (ctx == nullptr || current_context->add_subquery(SQ_UPDATE_VALUE, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_INSERT_VALUES: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_INSERT_VALUES, nullptr, current_context);
      if (ctx == nullptr ||
          current_context->add_subquery(SQ_INSERT_VALUES, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_INSERT_UPDATE: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_INSERT_UPDATE, nullptr, current_context);
      if (ctx == nullptr ||
          current_context->add_subquery(SQ_INSERT_UPDATE, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_DERIVED: {
      current_context = new (*THR_MALLOC) subquery_ctx(
          CTX_DERIVED, K_MATERIALIZED_FROM_SUBQUERY, current_context);
      if (current_context == nullptr) return true;
      break;
    }
    case CTX_OPTIMIZED_AWAY_SUBQUERY: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_OPTIMIZED_AWAY_SUBQUERY, nullptr, current_context);
      if (ctx == nullptr ||
          current_context->add_subquery(SQ_OPTIMIZED_AWAY, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_WHERE: {
      assert(subquery != nullptr);
      subquery_ctx *ctx =
          new (*THR_MALLOC) subquery_ctx(CTX_WHERE, nullptr, current_context);
      if (ctx == nullptr || current_context->add_where_subquery(ctx, subquery))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_HAVING: {
      subquery_ctx *ctx =
          new (*THR_MALLOC) subquery_ctx(CTX_HAVING, nullptr, current_context);
      if (ctx == nullptr || current_context->add_subquery(SQ_HAVING, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_ORDER_BY_SQ: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_ORDER_BY_SQ, nullptr, current_context);
      if (ctx == nullptr || current_context->add_subquery(SQ_ORDER_BY, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_GROUP_BY_SQ: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_GROUP_BY_SQ, nullptr, current_context);
      if (ctx == nullptr || current_context->add_subquery(SQ_GROUP_BY, ctx))
        return true;
      current_context = ctx;
      break;
    }
    case CTX_UNION: {
      assert(current_context == nullptr || current_context->type == CTX_UNION ||
             current_context->type == CTX_INTERSECT ||
             current_context->type == CTX_EXCEPT ||
             current_context->type == CTX_UNARY ||
             // subqueries:
             current_context->type == CTX_SELECT_LIST ||
             current_context->type == CTX_UPDATE_VALUE ||
             current_context->type == CTX_INSERT_VALUES ||
             current_context->type == CTX_DERIVED ||
             current_context->type == CTX_OPTIMIZED_AWAY_SUBQUERY ||
             current_context->type == CTX_WHERE ||
             current_context->type == CTX_HAVING ||
             current_context->type == CTX_QUALIFY ||
             current_context->type == CTX_ORDER_BY_SQ ||
             current_context->type == CTX_GROUP_BY_SQ ||
             current_context->type == CTX_QUERY_SPEC);
      setop_ctx *ctx = new (*THR_MALLOC)
          setop_ctx(current_context, CTX_UNION, K_QUERY_BLOCK);
      if (ctx == nullptr) return true;
      if (current_context != nullptr) current_context->add_query_spec(ctx);
      current_context = ctx;
      break;
    }
    case CTX_INTERSECT: {
      setop_ctx *ctx = new (*THR_MALLOC)
          setop_ctx(current_context, CTX_INTERSECT, K_QUERY_BLOCK);
      if (ctx == nullptr) return true;
      if (current_context != nullptr) current_context->add_query_spec(ctx);
      current_context = ctx;
      break;
    }
    case CTX_EXCEPT: {
      setop_ctx *ctx = new (*THR_MALLOC)
          setop_ctx(current_context, CTX_EXCEPT, K_QUERY_BLOCK);
      if (ctx == nullptr) return true;
      if (current_context != nullptr) current_context->add_query_spec(ctx);
      current_context = ctx;
      break;
    }
    case CTX_UNARY: {
      setop_ctx *ctx = new (*THR_MALLOC)
          setop_ctx(current_context, CTX_UNARY, K_QUERY_BLOCK);
      if (ctx == nullptr) return true;
      if (current_context != nullptr) current_context->add_query_spec(ctx);
      current_context = ctx;
      break;
    }
    case CTX_UNION_RESULT: {
      setop_result_ctx *ctx = new (*THR_MALLOC)
          setop_result_ctx(current_context, CTX_UNION_RESULT, K_UNION_RESULT);
      if (ctx == nullptr) return true;
      current_context->set_setop_result(ctx);
      current_context = ctx;
      break;
    }
    case CTX_INTERSECT_RESULT: {
      setop_result_ctx *ctx = new (*THR_MALLOC) setop_result_ctx(
          current_context, CTX_INTERSECT_RESULT, K_INTERSECT_RESULT);
      if (ctx == nullptr) return true;
      current_context->set_setop_result(ctx);
      current_context = ctx;
      break;
    }
    case CTX_EXCEPT_RESULT: {
      setop_result_ctx *ctx = new (*THR_MALLOC)
          setop_result_ctx(current_context, CTX_EXCEPT_RESULT, K_EXCEPT_RESULT);
      if (ctx == nullptr) return true;
      current_context->set_setop_result(ctx);
      current_context = ctx;
      break;
    }
    case CTX_UNARY_RESULT: {
      setop_result_ctx *ctx = new (*THR_MALLOC)
          setop_result_ctx(current_context, CTX_UNARY_RESULT, K_UNARY_RESULT);
      if (ctx == nullptr) return true;
      current_context->set_setop_result(ctx);
      current_context = ctx;
      break;
    }
    case CTX_QUERY_SPEC: {
      subquery_ctx *ctx = new (*THR_MALLOC)
          subquery_ctx(CTX_QUERY_SPEC, nullptr, current_context);
      if (ctx == nullptr || current_context->add_query_spec(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_MESSAGE: {
      /*
        Like CTX_QEP_TAB:
      */
      assert(current_context->type == CTX_JOIN ||
             current_context->type == CTX_MATERIALIZATION ||
             current_context->type == CTX_DUPLICATES_WEEDOUT ||
             current_context->type == CTX_GROUP_BY ||
             current_context->type == CTX_ORDER_BY ||
             current_context->type == CTX_DISTINCT ||
             current_context->type == CTX_WINDOW ||
             current_context->type == CTX_BUFFER_RESULT ||
             current_context->type == CTX_SIMPLE_GROUP_BY ||
             current_context->type == CTX_SIMPLE_ORDER_BY ||
             current_context->type == CTX_SIMPLE_DISTINCT ||
             current_context->type == CTX_UNION_RESULT ||
             current_context->type == CTX_INTERSECT_RESULT ||
             current_context->type == CTX_EXCEPT_RESULT);
      joinable_ctx *ctx = new (*THR_MALLOC) message_ctx(current_context);
      if (ctx == nullptr || current_context->add_join_tab(ctx)) return true;
      current_context = ctx;
      break;
    }
    case CTX_WINDOW: {
      window_ctx *ctx = new (*THR_MALLOC) window_ctx(current_context);
      if (ctx == nullptr) return true;
      current_context->set_window(ctx);
      current_context = ctx;
      break;
    }
    default:
      assert(!"Unknown EXPLAIN context!");
      return true;
  }

  if (prev_context) prev_context->set_child(current_context);

  return false;
}

bool Explain_format_JSON::end_context(enum_parsing_context ctx) {
  assert(current_context->type == ctx);

  bool ret = false;
  if (current_context->parent == nullptr) {
    Item *item;
    Opt_trace_context json;
    const size_t max_size = ULONG_MAX;
    if (json.start(true,   // support_I_S (enable JSON generation)
                   false,  // support_dbug_or_missing_priv
                   current_thd->variables.end_markers_in_json,  // end_marker
                   false,                                       // one_line
                   0,                                           // offset
                   1,                                           // limit
                   max_size,                                    // max_mem_size
                   Opt_trace_context::MISC))
      return true;

    {
      const Opt_trace_object braces(&json);

      if (current_context->format(&json)) return true;
    }
    json.end();

    const Opt_trace_iterator it(&json);
    if (!it.at_end()) {
      Opt_trace_info info;
      it.get_value(&info);
      item =
          new Item_string(info.trace_ptr, static_cast<uint>(info.trace_length),
                          system_charset_info);
    } else
      item = new Item_null();

    mem_root_deque<Item *> field_list(current_thd->mem_root);
    field_list.push_back(item);
    ret = (item == nullptr || output->send_data(current_thd, field_list));
  } else if (ctx == CTX_DERIVED) {
    if (!current_context->parent->find_and_set_derived(current_context)) {
      assert(!"No derived table found!");
      return true;
    }
  }

  current_context = current_context->parent;
  return ret;
}

bool Explain_format_JSON::send_headers(Query_result *result) {
  if (Explain_format::send_headers(result)) return true;

  mem_root_deque<Item *> field_list(current_thd->mem_root);
  Item *item = new Item_empty_string("EXPLAIN", 78, system_charset_info);
  if (item == nullptr) return true;
  field_list.push_back(item);
  return result->send_result_set_metadata(
      current_thd, field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
}

void qep_row::format_extra(Opt_trace_object *obj) {
  List_iterator<qep_row::extra> it(col_extra);
  qep_row::extra *e;
  while ((e = it++)) {
    assert(json_extra_tags[e->tag] != nullptr);
    if (e->data)
      obj->add_utf8(json_extra_tags[e->tag], e->data);
    else
      obj->add(json_extra_tags[e->tag], true);
  }
}

/* Convert Json object to string */
std::string Explain_format_JSON::ExplainJsonToString(Json_object *json) {
  // Serialize the JSON object to a string.
  Json_wrapper wrapper(json, /*alias=*/true);
  StringBuffer<STRING_BUFFER_USUAL_SIZE> explain;
  if (wrapper.to_pretty_string(&explain, "ExplainJsonToString()",
                               JsonDepthErrorHandler)) {
    return "";
  }
  return {explain.ptr(), explain.length()};
}

bool Explain_format_JSON::is_iterator_based(THD *explain_thd,
                                            const THD *query_thd) const {
  return query_thd->optimizer_switch_flag(
             OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER) ||
         explain_thd->variables.explain_json_format_version == 2;
}

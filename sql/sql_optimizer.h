#ifndef SQL_OPTIMIZER_INCLUDED
#define SQL_OPTIMIZER_INCLUDED

/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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
  @file sql/sql_optimizer.h
  Classes used for query optimizations.
*/

#include <sys/types.h>

#include <cstring>
#include <memory>
#include <utility>

#include "field_types.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_table_map.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/iterators/row_iterator.h"
#include "sql/mem_root_array.h"
#include "sql/opt_explain_format.h"  // Explain_sort_clause
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_select.h"  // Key_use
#include "sql/table.h"
#include "sql/temp_table_param.h"

enum class Subquery_strategy : int;
class COND_EQUAL;
class Item_subselect;
class Item_sum;
class Opt_trace_context;
class THD;
class Window;
struct AccessPath;
struct MYSQL_LOCK;

class Item_equal;
template <class T>
class mem_root_deque;

// Key_use has a trivial destructor, no need to run it from Mem_root_array.
typedef Mem_root_array<Key_use> Key_use_array;

class Cost_model_server;

/*
   This structure is used to collect info on potentially sargable
   predicates in order to check whether they become sargable after
   reading const tables.
   We form a bitmap of indexes that can be used for sargable predicates.
   Only such indexes are involved in range analysis.
*/

struct SARGABLE_PARAM {
  Field *field;     /* field against which to check sargability */
  Item **arg_value; /* values of potential keys for lookups     */
  uint num_values;  /* number of values in the above array      */
};

/**
  Wrapper for ORDER* pointer to trace origins of ORDER list

  As far as ORDER is just a head object of ORDER expression
  chain, we need some wrapper object to associate flags with
  the whole ORDER list.
*/
class ORDER_with_src {
 public:
  ORDER *order;  ///< ORDER expression that we are wrapping with this class
  Explain_sort_clause src;  ///< origin of order list

 private:
  int flags;  ///< bitmap of Explain_sort_property

 public:
  ORDER_with_src() { clean(); }

  ORDER_with_src(ORDER *order_arg, Explain_sort_clause src_arg)
      : order(order_arg),
        src(src_arg),
        flags(order_arg ? ESP_EXISTS : ESP_none) {}

  bool empty() const { return order == nullptr; }

  void clean() {
    order = nullptr;
    src = ESC_none;
    flags = ESP_none;
  }

  int get_flags() const {
    assert(order);
    return flags;
  }
};

class JOIN {
 public:
  JOIN(THD *thd_arg, Query_block *select);
  JOIN(const JOIN &rhs) = delete;
  JOIN &operator=(const JOIN &rhs) = delete;

  /// Query expression referring this query block
  Query_expression *query_expression() const {
    return query_block->master_query_expression();
  }

  /// Query block that is optimized and executed using this JOIN
  Query_block *const query_block;
  /// Thread handler
  THD *const thd;

  /**
    Optimal query execution plan. Initialized with a tentative plan in
    JOIN::make_join_plan() and later replaced with the optimal plan in
    get_best_combination().
  */
  JOIN_TAB *join_tab{nullptr};
  /// Array of QEP_TABs
  QEP_TAB *qep_tab{nullptr};

  /**
    Array of plan operators representing the current (partial) best
    plan. The array is allocated in JOIN::make_join_plan() and is valid only
    inside this function. Initially (*best_ref[i]) == join_tab[i].
    The optimizer reorders best_ref.
  */
  JOIN_TAB **best_ref{nullptr};
  /// mapping between table indexes and JOIN_TABs
  JOIN_TAB **map2table{nullptr};
  /*
    The table which has an index that allows to produce the required ordering.
    A special value of 0x1 means that the ordering will be produced by
    passing 1st non-const table to filesort(). NULL means no such table exists.
  */
  TABLE *sort_by_table{nullptr};

  // Temporary tables that need to be cleaned up after the query.
  // Only used for the hypergraph optimizer; the non-hypergraph optimizer
  // uses QEP_TABs to hold the list of tables (including temporary tables).
  struct TemporaryTableToCleanup {
    TABLE *table;

    // Allocated on the MEM_ROOT, but can hold some objects
    // that allocate on the heap and thus need destruction.
    Temp_table_param *temp_table_param;
  };
  Prealloced_array<TemporaryTableToCleanup, 1> temp_tables{
      PSI_NOT_INSTRUMENTED};

  // Similarly, filesorts that need to be cleaned up after the query.
  // Only used for the hypergraph optimizer, for the same reason as above.
  Prealloced_array<Filesort *, 1> filesorts_to_cleanup{PSI_NOT_INSTRUMENTED};

  /**
    Before plan has been created, "tables" denote number of input tables in the
    query block and "primary_tables" is equal to "tables".
    After plan has been created (after JOIN::get_best_combination()),
    the JOIN_TAB objects are enumerated as follows:
    - "tables" gives the total number of allocated JOIN_TAB objects
    - "primary_tables" gives the number of input tables, including
      materialized temporary tables from semi-join operation.
    - "const_tables" are those tables among primary_tables that are detected
      to be constant.
    - "tmp_tables" is 0, 1 or 2 (more if windows) and counts the maximum
      possible number of intermediate tables in post-processing (ie sorting and
      duplicate removal).
      Later, tmp_tables will be adjusted to the correct number of
      intermediate tables, @see JOIN::make_tmp_tables_info.
    - The remaining tables (ie. tables - primary_tables - tmp_tables) are
      input tables to materialized semi-join operations.
    The tables are ordered as follows in the join_tab array:
     1. const primary table
     2. non-const primary tables
     3. intermediate sort/group tables
     4. possible holes in array
     5. semi-joined tables used with materialization strategy
  */
  uint tables{0};          ///< Total number of tables in query block
  uint primary_tables{0};  ///< Number of primary input tables in query block
  uint const_tables{0};    ///< Number of primary tables deemed constant
  uint tmp_tables{0};      ///< Number of temporary tables used by query
  uint send_group_parts{0};
  /**
    Indicates that the data will be aggregated (typically GROUP BY),
    _and_ that it is already processed in an order that is compatible with
    the grouping in use (e.g. because we are scanning along an index,
    or because an earlier step sorted the data in a group-compatible order).

    Note that this flag changes value at multiple points during optimization;
    if it's set when a temporary table is created, this means we aggregate
    into said temporary table (end_write_group is chosen instead of end_write),
    but if it's set later, it means that we can aggregate as we go,
    just before sending the data to the client (end_send_group is chosen
    instead of end_send).

    @see make_group_fields, alloc_group_fields, JOIN::exec
  */
  bool streaming_aggregation{false};
  /// If query contains GROUP BY clause
  bool grouped;
  /// If true, send produced rows using query_result
  bool do_send_rows{true};
  /// Set of tables contained in query
  table_map all_table_map{0};
  table_map const_table_map;  ///< Set of tables found to be const
  /**
     Const tables which are either:
     - not empty
     - empty but inner to a LEFT JOIN, thus "considered" not empty for the
     rest of execution (a NULL-complemented row will be used).
  */
  table_map found_const_table_map;
  /**
     This is the bitmap of all tables which are dependencies of
     lateral derived tables which are not (yet) part of the partial
     plan.  (The value is a logical 'or' of zero or more
     Table_ref.map() values.)

     When we are building the join order, there is a partial plan (an
     ordered sequence of JOIN_TABs), and an unordered set of JOIN_TABs
     not yet added to the plan. Due to backtracking, the partial plan
     may both grow and shrink. When we add a new table to the plan, we
     may wish to set up join buffering, so that rows from the preceding
     table are buffered. If any of the remaining tables are derived
     tables that depends on any of the predecessors of the table we
     are adding (i.e. a lateral dependency), join buffering would be
     inefficient. (@see setup_join_buffering() for a detailed
     explanation of why this is so.)

     For this reason we need to maintain this table_map of lateral
     dependencies of tables not yet in the plan. Whenever we add a new
     table to the plan, we update the map by calling
     Optimize_table_order::recalculate_lateral_deps_incrementally().
     And when we remove a table, we restore the previous map value
     using a Tabel_map_restorer object.

     As an example, assume that we join four tables, t1, t2, t3 and
     d1, where d1 is a derived table that depends on t1:

     SELECT * FROM t1 JOIN t2 ON t1.a=t2.b JOIN t3 ON t2.c=t3.d
       JOIN LATERAL (SELECT DISTINCT e AS x FROM t4 WHERE t4.f=t1.c)
       AS d1 ON t3.e=d1.x;

     Now, if our partial plan is t1->t2, the map (of lateral
     dependencies of the remaining tables) will contain t1.
     This tells us that we should not use join buffering when joining t1
     with t2. But if the partial plan is t1->d2->t2, the map will be
     empty. We may thus use join buffering when joining d2 with t2.
  */
  table_map deps_of_remaining_lateral_derived_tables{0};

  /* Number of records produced after join + group operation */
  ha_rows send_records{0};
  ha_rows found_records{0};
  ha_rows examined_rows{0};
  ha_rows row_limit{0};
  // m_select_limit is used to decide if we are likely to scan the whole table.
  ha_rows m_select_limit{0};
  /**
    Used to fetch no more than given amount of rows per one
    fetch operation of server side cursor.
    The value is checked in end_send and end_send_group in fashion, similar
    to offset_limit_cnt:
      - fetch_limit= HA_POS_ERROR if there is no cursor.
      - when we open a cursor, we set fetch_limit to 0,
      - on each fetch iteration we add num_rows to fetch to fetch_limit
  */
  ha_rows fetch_limit{HA_POS_ERROR};

  /**
    This is the result of join optimization.

    @note This is a scratch array, not used after get_best_combination().
  */
  POSITION *best_positions{nullptr};

  /******* Join optimization state members start *******/

  /* Current join optimization state */
  POSITION *positions{nullptr};

  /* We also maintain a stack of join optimization states in * join->positions[]
   */
  /******* Join optimization state members end *******/

  /// A hook that secondary storage engines can use to override the executor
  /// completely.
  using Override_executor_func = bool (*)(JOIN *, Query_result *);
  Override_executor_func override_executor_func = nullptr;

  /**
    The cost of best complete join plan found so far during optimization,
    after optimization phase - cost of picked join order (not taking into
    account the changes made by test_if_skip_sort_order()).
  */
  double best_read{0.0};
  /**
    The estimated row count of the plan with best read time (see above).
  */
  ha_rows best_rowcount{0};
  /// Expected cost of filesort.
  double sort_cost{0.0};
  /// Expected cost of windowing;
  double windowing_cost{0.0};
  mem_root_deque<Item *> *fields;
  List<Cached_item> group_fields{};
  List<Cached_item> group_fields_cache{};

  // For destroying fields otherwise owned by RemoveDuplicatesIterator.
  List<Cached_item> semijoin_deduplication_fields{};

  Item_sum **sum_funcs{nullptr};
  /**
     Describes a temporary table.
     Each tmp table has its own tmp_table_param.
     The one here is transiently used as a model by create_intermediate_table(),
     to build the tmp table's own tmp_table_param.
  */
  Temp_table_param tmp_table_param;
  MYSQL_LOCK *lock;

  enum class RollupState { NONE, INITED, READY };
  RollupState rollup_state;
  bool implicit_grouping;  ///< True if aggregated but no GROUP BY

  /**
    At construction time, set if SELECT DISTINCT. May be reset to false
    later, when we set up a temporary table operation that deduplicates for us.
   */
  bool select_distinct;

  /**
    If we have the GROUP BY statement in the query,
    but the group_list was emptied by optimizer, this
    flag is true.
    It happens when fields in the GROUP BY are from
    constant table
  */
  bool group_optimized_away{false};

  /*
    simple_xxxxx is set if ORDER/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if ORDER/GROUP BY is empty.
    Used for deciding for or against using a temporary table to compute
    GROUP/ORDER BY.
  */
  bool simple_order{false};
  bool simple_group{false};

  /*
    m_ordered_index_usage is set if an ordered index access
    should be used instead of a filesort when computing
    ORDER/GROUP BY.
  */
  enum {
    ORDERED_INDEX_VOID,      // No ordered index avail.
    ORDERED_INDEX_GROUP_BY,  // Use index for GROUP BY
    ORDERED_INDEX_ORDER_BY   // Use index for ORDER BY
  } m_ordered_index_usage{ORDERED_INDEX_VOID};

  /**
    Is set if we have a GROUP BY and we have ORDER BY on a constant or when
    sorting isn't required.
  */
  bool skip_sort_order{false};

  /**
    If true we need a temporary table on the result set before any
    windowing steps, e.g. for DISTINCT or we have a query ORDER BY.
    See details in JOIN::optimize
  */
  bool need_tmp_before_win{false};

  /// If JOIN has lateral derived tables (is set at start of planning)
  bool has_lateral{false};

  /// Used and updated by JOIN::make_join_plan() and optimize_keyuse()
  Key_use_array keyuse_array;

  /**
    Array of pointers to lists of expressions.
    Each list represents the SELECT list at a certain stage of execution,
    and also contains necessary extras: expressions added for ORDER BY,
    GROUP BY, window clauses, underlying items of split items.
    This array is only used when the query makes use of tmp tables: after
    writing to tmp table (e.g. for GROUP BY), if this write also does a
    function's calculation (e.g. of SUM), after the write the function's value
    is in a column of the tmp table. If a SELECT list expression is the SUM,
    and we now want to read that materialized SUM and send it forward, a new
    expression (Item_field type instead of Item_sum), is needed. The new
    expressions are listed in JOIN::tmp_fields_list[x]; 'x' is a number
    (REF_SLICE_).
    @see JOIN::make_tmp_tables_info()
  */
  mem_root_deque<Item *> *tmp_fields = nullptr;

  int error{0};  ///< set in optimize(), exec(), prepare_result()

  /**
    Incremented each time clear_hash_tables() is run, signaling to
    HashJoinIterators that they cannot keep their hash tables anymore
    (since outer references may have changed).
   */
  uint64_t hash_table_generation{0};

  /**
    ORDER BY and GROUP BY lists, to transform with prepare,optimize and exec
  */
  ORDER_with_src order, group_list;

  // Used so that AggregateIterator knows which items to signal when the rollup
  // level changes. Obviously only used in the presence of rollup.
  Prealloced_array<Item_rollup_group_item *, 4> rollup_group_items{
      PSI_NOT_INSTRUMENTED};
  Prealloced_array<Item_rollup_sum_switcher *, 4> rollup_sums{
      PSI_NOT_INSTRUMENTED};

  /**
    Any window definitions
  */
  List<Window> m_windows;

  /**
    True if a window requires a certain order of rows, which implies that any
    order of rows coming out of the pre-window join will be disturbed.
  */
  bool m_windows_sort{false};

  /// If we have set up tmp tables for windowing, @see make_tmp_tables_info
  bool m_windowing_steps{false};

  /**
    Buffer to gather GROUP BY, ORDER BY and DISTINCT QEP details for EXPLAIN
  */
  Explain_format_flags explain_flags{};

  /**
    JOIN::having_cond is initially equal to query_block->having_cond, but may
    later be changed by optimizations performed by JOIN.
    The relationship between the JOIN::having_cond condition and the
    associated variable query_block->having_value is so that
    having_value can be:
     - COND_UNDEF if a having clause was not specified in the query or
       if it has not been optimized yet
     - COND_TRUE if the having clause is always true, in which case
       JOIN::having_cond is set to NULL.
     - COND_FALSE if the having clause is impossible, in which case
       JOIN::having_cond is set to NULL
     - COND_OK otherwise, meaning that the having clause needs to be
       further evaluated
    All of the above also applies to the where_cond/query_block->cond_value
    pair.
  */
  /**
    Optimized WHERE clause item tree (valid for one single execution).
    Used in JOIN execution if no tables. Otherwise, attached in pieces to
    JOIN_TABs and then not used in JOIN execution.
    Printed by EXPLAIN EXTENDED.
    Initialized by Query_block::get_optimizable_conditions().
  */
  Item *where_cond;
  /**
    Optimized HAVING clause item tree (valid for one single execution).
    Used in JOIN execution, as last "row filtering" step. With one exception:
    may be pushed to the JOIN_TABs of temporary tables used in DISTINCT /
    GROUP BY (see JOIN::make_tmp_tables_info()); in that case having_cond is
    set to NULL, but is first saved to having_for_explain so that EXPLAIN
    EXTENDED can still print it.
    Initialized by Query_block::get_optimizable_conditions().
  */
  Item *having_cond;
  Item *having_for_explain;  ///< Saved optimized HAVING for EXPLAIN
  /**
    Pointer set to query_block->get_table_list() at the start of
    optimization. May be changed (to NULL) only if optimize_aggregated_query()
    optimizes tables away.
  */
  Table_ref *tables_list;
  COND_EQUAL *cond_equal{nullptr};
  /*
    Join tab to return to. Points to an element of join->join_tab array, or to
    join->join_tab[-1].
    This is used at execution stage to shortcut join enumeration. Currently
    shortcutting is done to handle outer joins or handle semi-joins with
    FirstMatch strategy.
  */
  plan_idx return_tab{0};

  /**
    ref_items is an array of 4+ slices, each containing an array of Item
    pointers. ref_items is used in different phases of query execution.
    - slice 0 is initially the same as Query_block::base_ref_items, ie it is
      the set of items referencing fields from base tables. During optimization
      and execution it may be temporarily overwritten by slice 1-3.
    - slice 1 is a representation of the used items when being read from
      the first temporary table.
    - slice 2 is a representation of the used items when being read from
      the second temporary table.
    - slice 3 is a copy of the original slice 0. It is created if
      slice overwriting is necessary, and it is used to restore
      original values in slice 0 after having been overwritten.
    - slices 4 -> N are used by windowing: all the window's out tmp tables,

      Two windows:     4: window 1's out table
                       5: window 2's out table

      and so on.

    Slice 0 is allocated for the lifetime of a statement, whereas slices 1-3
    are associated with a single optimization. The size of slice 0 determines
    the slice size used when allocating the other slices.
   */
  Ref_item_array *ref_items{
      nullptr};  // cardinality: REF_SLICE_SAVED_BASE + 1 + #windows*2

  /**
    The slice currently stored in ref_items[0].
    Used to restore the base ref_items slice from the "save" slice after it
    has been overwritten by another slice (1-3).
  */
  uint current_ref_item_slice;

  /**
    Used only if this query block is recursive. Contains count of
    all executions of this recursive query block, since the last
    this->reset().
  */
  uint recursive_iteration_count{0};

  /**
    <> NULL if optimization has determined that execution will produce an
    empty result before aggregation, contains a textual explanation on why
    result is empty. Implicitly grouped queries may still produce an
    aggregation row.
    @todo - suggest to set to "Preparation determined that query is empty"
            when Query_block::is_empty_query() is true.
  */
  const char *zero_result_cause{nullptr};

  /**
     True if, at this stage of processing, subquery materialization is allowed
     for children subqueries of this JOIN (those in the SELECT list, in WHERE,
     etc). If false, and we have to evaluate a subquery at this stage, then we
     must choose EXISTS.
  */
  bool child_subquery_can_materialize{false};
  /**
     True if plan search is allowed to use references to expressions outer to
     this JOIN (for example may set up a 'ref' access looking up an outer
     expression in the index, etc).
  */
  bool allow_outer_refs{false};

  /* Temporary tables used to weed-out semi-join duplicates */
  List<TABLE> sj_tmp_tables{};
  List<Semijoin_mat_exec> sjm_exec_list{};
  /* end of allocation caching storage */

  /** Exec time only: true <=> current group has been sent */
  bool group_sent{false};
  /// If true, calculate found rows for this query block
  bool calc_found_rows{false};

  /**
    This will force tmp table to NOT use index + update for group
    operation as it'll cause [de]serialization for each json aggregated
    value and is very ineffective (times worse).
    Server should use filesort, or tmp table + filesort to resolve GROUP BY
    with JSON aggregate functions.
  */
  bool with_json_agg;

  /// True if plan is const, ie it will return zero or one rows.
  bool plan_is_const() const { return const_tables == primary_tables; }

  /**
    True if plan contains one non-const primary table (ie not including
    tables taking part in semi-join materialization).
  */
  bool plan_is_single_table() { return primary_tables - const_tables == 1; }

  /**
    Returns true if any of the items in JOIN::fields contains a call to the
    full-text search function MATCH, which is not wrapped in an aggregation
    function.
  */
  bool contains_non_aggregated_fts() const;

  bool optimize(bool finalize_access_paths);
  void reset();
  bool prepare_result();
  void destroy();
  bool alloc_func_list();
  bool make_sum_func_list(const mem_root_deque<Item *> &fields,
                          bool before_group_by, bool recompute = false);

  /**
     Overwrites one slice of ref_items with the contents of another slice.
     In the normal case, dst and src have the same size().
     However: the rollup slices may have smaller size than slice_sz.
   */
  void copy_ref_item_slice(uint dst_slice, uint src_slice) {
    copy_ref_item_slice(ref_items[dst_slice], ref_items[src_slice]);
  }
  void copy_ref_item_slice(Ref_item_array dst_arr, Ref_item_array src_arr) {
    assert(dst_arr.size() >= src_arr.size());
    void *dest = dst_arr.array();
    const void *src = src_arr.array();
    if (!src_arr.is_null())
      memcpy(dest, src, src_arr.size() * src_arr.element_size());
  }

  /**
    Allocate a ref_item slice, assume that slice size is in ref_items[0]

    @param thd_arg  thread handler
    @param sliceno  The slice number to allocate in JOIN::ref_items

    @returns false if success, true if error
  */
  bool alloc_ref_item_slice(THD *thd_arg, int sliceno);

  /**
    Overwrite the base slice of ref_items with the slice supplied as argument.

    @param sliceno number to overwrite the base slice with, must be 1-4 or
           4 + windowno.
  */
  void set_ref_item_slice(uint sliceno) {
    assert((int)sliceno >= 1);
    if (current_ref_item_slice != sliceno) {
      copy_ref_item_slice(REF_SLICE_ACTIVE, sliceno);
      DBUG_PRINT("info", ("JOIN %p ref slice %u -> %u", this,
                          current_ref_item_slice, sliceno));
      current_ref_item_slice = sliceno;
    }
  }

  /// @note do also consider Switch_ref_item_slice
  uint get_ref_item_slice() const { return current_ref_item_slice; }

  /**
     Returns the clone of fields_list which is appropriate for evaluating
     expressions at the current stage of execution; which stage is denoted by
     the value of current_ref_item_slice.
  */
  mem_root_deque<Item *> *get_current_fields();

  bool optimize_rollup();
  bool finalize_table_conditions(THD *thd);
  /**
    Release memory and, if possible, the open tables held by this execution
    plan (and nested plans). It's used to release some tables before
    the end of execution in order to increase concurrency and reduce
    memory consumption.
  */
  void join_free();
  /** Cleanup this JOIN. Not a full cleanup. reusable? */
  void cleanup();

  bool clear_fields(table_map *save_nullinfo);
  void restore_fields(table_map save_nullinfo);

 private:
  /**
    Return whether the caller should send a row even if the join
    produced no rows if:
     - there is an aggregate function (sum_func_count!=0), and
     - the query is not grouped, and
     - a possible HAVING clause evaluates to TRUE.

    @note: if there is a having clause, it must be evaluated before
    returning the row.
  */
  bool send_row_on_empty_set() const {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
            group_list.empty() && !group_optimized_away &&
            query_block->having_value != Item::COND_FALSE);
  }

 public:
  bool generate_derived_keys();
  void finalize_derived_keys();
  bool get_best_combination();
  bool attach_join_conditions(plan_idx last_tab);

 private:
  bool attach_join_condition_to_nest(plan_idx first_inner, plan_idx last_tab,
                                     Item *join_cond, bool is_sj_mat_cond);

 public:
  bool update_equalities_for_sjm();
  bool add_sorting_to_table(uint idx, ORDER_with_src *order,
                            bool sort_before_group);
  bool decide_subquery_strategy();
  void refine_best_rowcount();
  table_map calculate_deps_of_remaining_lateral_derived_tables(
      table_map plan_tables, uint idx) const;
  bool clear_sj_tmp_tables();
  bool clear_corr_derived_tmp_tables();
  void clear_hash_tables() { ++hash_table_generation; }

  void mark_const_table(JOIN_TAB *table, Key_use *key);
  /// State of execution plan. Currently used only for EXPLAIN
  enum enum_plan_state {
    NO_PLAN,      ///< No plan is ready yet
    ZERO_RESULT,  ///< Zero result cause is set
    NO_TABLES,    ///< Plan has no tables
    PLAN_READY    ///< Plan is ready
  };
  /// See enum_plan_state
  enum_plan_state get_plan_state() const { return plan_state; }
  bool is_optimized() const { return optimized; }
  void set_optimized() { optimized = true; }
  bool is_executed() const { return executed; }
  void set_executed() { executed = true; }

  /**
    Retrieve the cost model object to be used for this join.

    @return Cost model object for the join
  */

  const Cost_model_server *cost_model() const;

  /**
    Check if FTS index only access is possible
  */
  bool fts_index_access(JOIN_TAB *tab);

  QEP_TAB::enum_op_type get_end_select_func();
  /**
     Propagate dependencies between tables due to outer join relations.

     @returns false if success, true if error
  */
  bool propagate_dependencies();

  /**
    Handle offloading of query parts to the underlying engines, when
    such is supported by their implementation.

    @returns false if success, true if error
  */
  bool push_to_engines();

  AccessPath *root_access_path() const { return m_root_access_path; }
  void set_root_access_path(AccessPath *path) { m_root_access_path = path; }

  /**
    If this query block was planned twice, once with and once without conditions
    added by in2exists, changes the root access path to the one without
    in2exists. If not (ie., there were never any such conditions in the first
    place), does nothing.
   */
  void change_to_access_path_without_in2exists();

  /**
    In the case of rollup (only): After the base slice list was made, we may
    have modified the field list to add rollup group items and sum switchers,
    but there may be Items with refs that refer to the base slice. This function
    refreshes the base slice (and its copy, REF_SLICE_SAVED_BASE) with a fresh
    copy of the list from “fields”.

    When we get rid of slices entirely, we can get rid of this, too.
   */
  void refresh_base_slice();

  /**
    Whether this query block needs finalization (see
    FinalizePlanForQueryBlock()) before it can be actually used.
    This only happens when using the hypergraph join optimizer.
   */
  bool needs_finalize{false};

 private:
  bool optimized{false};  ///< flag to avoid double optimization in EXPLAIN

  /**
    Set by exec(), reset by reset(). Note that this needs to be set
    _during_ the query (not only when it's done executing), or the
    dynamic range optimizer will not understand which tables have been
    read.
   */
  bool executed{false};

  /// Final execution plan state. Currently used only for EXPLAIN
  enum_plan_state plan_state{NO_PLAN};

 public:
  /*
    When join->select_count is set, tables will not be optimized away.
    The call to records() will be delayed until the execution phase and
    the counting will be done on an index of Optimizer's choice.
    The index will be decided in find_shortest_key(), called from
    optimize_aggregated_query().
  */
  bool select_count{false};

 private:
  /**
    Create a temporary table to be used for processing DISTINCT/ORDER
    BY/GROUP BY.

    @note Will modify JOIN object wrt sort/group attributes

    @param tab              the JOIN_TAB object to attach created table to
    @param tmp_table_fields List of items that will be used to define
                            column types of the table.
    @param tmp_table_group  Group key to use for temporary table, empty if none.
    @param save_sum_fields  If true, do not replace Item_sum items in
                            @c tmp_fields list with Item_field items referring
                            to fields in temporary table.

    @returns false on success, true on failure
  */
  bool create_intermediate_table(QEP_TAB *tab,
                                 const mem_root_deque<Item *> &tmp_table_fields,
                                 ORDER_with_src &tmp_table_group,
                                 bool save_sum_fields);

  /**
    Optimize distinct when used on a subset of the tables.

    E.g.,: SELECT DISTINCT t1.a FROM t1,t2 WHERE t1.b=t2.b
    In this case we can stop scanning t2 when we have found one t1.a
  */
  void optimize_distinct();

  /**
    Function sets FT hints, initializes FT handlers and
    checks if FT index can be used as covered.
  */
  bool optimize_fts_query();

  /**
    Checks if the chosen plan suffers from a problem related to full-text search
    and streaming aggregation, which is likely to cause wrong results or make
    the query misbehave in other ways, and raises an error if so. Only to be
    called for queries with full-text search and GROUP BY WITH ROLLUP.

    If there are calls to MATCH in the SELECT list (including the hidden
    elements lifted there from other clauses), and they are not inside an
    aggregate function, the results of the MATCH clause need to be materialized
    before streaming aggregation is performed. The hypergraph optimizer adds a
    materialization step before aggregation if needed (see
    CreateStreamingAggregationPath()), but the old optimizer only does that for
    implicitly grouped queries. For explicitly grouped queries, it instead
    disables streaming aggregation for the queries that would need a
    materialization step to work correctly (see JOIN::test_skip_sort()).

    For explicitly grouped queries WITH ROLLUP, however, streaming aggregation
    is currently the only alternative. In many cases it still works correctly
    because an intermediate materialization step has been added for some other
    reason, typically for a sort. For now, in those cases where a
    materialization step has not been added, we raise an error instead of going
    ahead with an invalid execution plan.

    @return true if an error was raised.
  */
  bool check_access_path_with_fts() const;

  bool prune_table_partitions();
  /**
    Initialize key dependencies for join tables.

    TODO figure out necessity of this method. Current test
         suite passed without this initialization.
  */
  void init_key_dependencies() {
    JOIN_TAB *const tab_end = join_tab + tables;
    for (JOIN_TAB *tab = join_tab; tab < tab_end; tab++)
      tab->key_dependent = tab->dependent;
  }

 private:
  void set_prefix_tables();
  void cleanup_item_list(const mem_root_deque<Item *> &items) const;
  void set_semijoin_embedding();
  bool make_join_plan();
  bool init_planner_arrays();
  bool extract_const_tables();
  bool extract_func_dependent_tables();
  void update_sargable_from_const(SARGABLE_PARAM *sargables);
  bool estimate_rowcount();
  void optimize_keyuse();
  void set_semijoin_info();
  /**
   An utility function - apply heuristics and optimize access methods to tables.
   @note Side effect - this function could set 'Impossible WHERE' zero
   result.
  */
  void adjust_access_methods();
  void update_depend_map();
  void update_depend_map(ORDER *order);
  /**
    Fill in outer join related info for the execution plan structure.

      For each outer join operation left after simplification of the
      original query the function set up the following pointers in the linear
      structure join->join_tab representing the selected execution plan.
      The first inner table t0 for the operation is set to refer to the last
      inner table tk through the field t0->last_inner.
      Any inner table ti for the operation are set to refer to the first
      inner table ti->first_inner.
      The first inner table t0 for the operation is set to refer to the
      first inner table of the embedding outer join operation, if there is any,
      through the field t0->first_upper.
      The on expression for the outer join operation is attached to the
      corresponding first inner table through the field t0->on_expr_ref.
      Here ti are structures of the JOIN_TAB type.

    EXAMPLE. For the query:
    @code
          SELECT * FROM t1
                        LEFT JOIN
                        (t2, t3 LEFT JOIN t4 ON t3.a=t4.a)
                        ON (t1.a=t2.a AND t1.b=t3.b)
            WHERE t1.c > 5,
    @endcode

      given the execution plan with the table order t1,t2,t3,t4
      is selected, the following references will be set;
      t4->last_inner=[t4], t4->first_inner=[t4], t4->first_upper=[t2]
      t2->last_inner=[t4], t2->first_inner=t3->first_inner=[t2],
      on expression (t1.a=t2.a AND t1.b=t3.b) will be attached to
      *t2->on_expr_ref, while t3.a=t4.a will be attached to *t4->on_expr_ref.

    @note
      The function assumes that the simplification procedure has been
      already applied to the join query (see simplify_joins).
      This function can be called only after the execution plan
      has been chosen.
  */
  void make_outerjoin_info();

  /**
    Initialize ref access for all tables that use it.

    @return False if success, True if error

    @note We cannot setup fields used for ref access before we have sorted
          the items within multiple equalities according to the final order of
          the tables involved in the join operation. Currently, this occurs in
          @see substitute_for_best_equal_field().
  */
  bool init_ref_access();
  bool alloc_qep(uint n);
  void unplug_join_tabs();
  bool setup_semijoin_materialized_table(JOIN_TAB *tab, uint tableno,
                                         POSITION *inner_pos,
                                         POSITION *sjm_pos);

  bool add_having_as_tmp_table_cond(uint curr_tmp_table);
  bool make_tmp_tables_info();
  void set_plan_state(enum_plan_state plan_state_arg);
  bool compare_costs_of_subquery_strategies(Subquery_strategy *method);
  ORDER *remove_const(ORDER *first_order, Item *cond, bool change_list,
                      bool *simple_order, bool group_by);

  /**
    Check whether this is a subquery that can be evaluated by index look-ups.
    If so, change subquery engine to subselect_indexsubquery_engine.

    @retval  1   engine was changed
    @retval  0   engine wasn't changed
    @retval -1   OOM or other error
  */
  int replace_index_subquery();

  /**
    Optimize DISTINCT, GROUP BY, ORDER BY clauses

    @retval false ok
    @retval true  an error occurred
  */
  bool optimize_distinct_group_order();

  /**
    Test if an index could be used to replace filesort for ORDER BY/GROUP BY

    @details
      Investigate whether we may use an ordered index as part of either
      DISTINCT, GROUP BY or ORDER BY execution. An ordered index may be
      used for only the first of any of these terms to be executed. This
      is reflected in the order which we check for test_if_skip_sort_order()
      below. However we do not check for DISTINCT here, as it would have
      been transformed to a GROUP BY at this stage if it is a candidate for
      ordered index optimization.
      If a decision was made to use an ordered index, the availability
      if such an access path is stored in 'm_ordered_index_usage' for later
      use by 'execute' or 'explain'
  */
  void test_skip_sort();

  bool alloc_indirection_slices();

  /**
    Convert the executor structures to a set of access paths, storing
    the result in m_root_access_path.
   */
  void create_access_paths();

 public:
  /**
    Create access paths with the knowledge that there are going to be zero rows
    coming from tables (before aggregation); typically because we know that
    all of them would be filtered away by WHERE (e.g. SELECT * FROM t1
    WHERE 1=2). This will normally yield no output rows, but if we have implicit
    aggregation, it might yield a single one.
   */
  void create_access_paths_for_zero_rows();

 private:
  void create_access_paths_for_index_subquery();

  /** @{ Helpers for create_access_paths. */
  AccessPath *create_root_access_path_for_join();
  AccessPath *attach_access_paths_for_having_and_limit(AccessPath *path);
  AccessPath *attach_access_path_for_update_or_delete(AccessPath *path);
  /** @} */

  /**
    An access path you can read from to get all records for this query
    (after you create an iterator from it).
   */
  AccessPath *m_root_access_path = nullptr;

  /**
    If this query block contains conditions synthesized during IN-to-EXISTS
    conversion: A second query plan with all such conditions removed.
    See comments in JOIN::optimize().
   */
  AccessPath *m_root_access_path_no_in2exists = nullptr;
};

/**
  Use this in a function which depends on best_ref listing tables in the
  final join order. If 'tables==0', one is not expected to consult best_ref
  cells, and best_ref may not even have been allocated.
*/
#define ASSERT_BEST_REF_IN_JOIN_ORDER(join)                                 \
  do {                                                                      \
    assert((join)->tables == 0 || ((join)->best_ref && !(join)->join_tab)); \
  } while (0)

/**
  RAII class to ease the temporary switching to a different slice of
  the ref item array.
*/
class Switch_ref_item_slice {
  JOIN *join;
  uint saved;

 public:
  Switch_ref_item_slice(JOIN *join_arg, uint new_v)
      : join(join_arg), saved(join->get_ref_item_slice()) {
    if (!join->ref_items[new_v].is_null()) join->set_ref_item_slice(new_v);
  }
  ~Switch_ref_item_slice() { join->set_ref_item_slice(saved); }
};

bool uses_index_fields_only(Item *item, TABLE *tbl, uint keyno,
                            bool other_tbls_ok);
bool remove_eq_conds(THD *thd, Item *cond, Item **retcond,
                     Item::cond_result *cond_value);
bool optimize_cond(THD *thd, Item **conds, COND_EQUAL **cond_equal,
                   mem_root_deque<Table_ref *> *join_list,
                   Item::cond_result *cond_value);
Item *substitute_for_best_equal_field(THD *thd, Item *cond,
                                      COND_EQUAL *cond_equal,
                                      JOIN_TAB **table_join_idx);
bool build_equal_items(THD *thd, Item *cond, Item **retcond,
                       COND_EQUAL *inherited, bool do_inherit,
                       mem_root_deque<Table_ref *> *join_list,
                       COND_EQUAL **cond_equal_ref);
bool is_indexed_agg_distinct(JOIN *join,
                             mem_root_deque<Item_field *> *out_args);
Key_use_array *create_keyuse_for_table(
    THD *thd, uint keyparts, Item_field **fields,
    const mem_root_deque<Item *> &outer_exprs);
Item_field *get_best_field(Item_field *item_field, COND_EQUAL *cond_equal);
Item *make_cond_for_table(THD *thd, Item *cond, table_map tables,
                          table_map used_table, bool exclude_expensive_cond);
uint build_bitmap_for_nested_joins(mem_root_deque<Table_ref *> *join_list,
                                   uint first_unused);

/**
  Create an order list that consists of all non-const fields and items.
  This is usable for e.g. converting DISTINCT into GROUP or ORDER BY.
  Is ref_item_array is non-null (is_null() returns false), the items
  will point into the slice given by it. Otherwise, it points directly
  into *fields (this is the only reason why fields is not const).

  Try to put the items in "order_list" first, to allow one to optimize away
  a later ORDER BY.
 */
ORDER *create_order_from_distinct(THD *thd, Ref_item_array ref_item_array,
                                  ORDER *order_list,
                                  mem_root_deque<Item *> *fields,
                                  bool skip_aggregates,
                                  bool convert_bit_fields_to_long,
                                  bool *all_order_by_fields_used);

/**
   Returns true if arguments are a temporal Field having no date,
   part and a temporal expression having a date part.
   @param  f  Field
   @param  v  Expression
 */
inline bool field_time_cmp_date(const Field *f, const Item *v) {
  const enum_field_types ft = f->type();
  return is_temporal_type(ft) && !is_temporal_type_with_date(ft) &&
         v->is_temporal_with_date();
}

bool substitute_gc(THD *thd, Query_block *query_block, Item *where_cond,
                   ORDER *group_list, ORDER *order);

/**
   This class restores a table_map object to its original value
   when '*this' is destroyed.
 */
class Table_map_restorer final {
  /** The location to be restored.*/
  table_map *const m_location;
  /** The original value to restore.*/
  const table_map m_saved_value;

 public:
  /**
     Constructor.
     @param map The table map that we wish to restore.
  */
  explicit Table_map_restorer(table_map *map)
      : m_location(map), m_saved_value(*map) {}

  // This class is not intended to be copied.
  Table_map_restorer(const Table_map_restorer &) = delete;
  Table_map_restorer &operator=(const Table_map_restorer &) = delete;

  ~Table_map_restorer() { restore(); }
  void restore() { *m_location = m_saved_value; }
  void assert_unchanged() const { assert(*m_location == m_saved_value); }
};

/**
  Estimates how many times a subquery will be executed as part of a
  query execution. If it is a cacheable subquery, the estimate tells
  how many times the subquery will be executed if it is not cached.

  @param[in]     subquery  the Item that represents the subquery
  @param[in,out] trace     optimizer trace context

  @return the number of times the subquery is expected to be executed
*/
double calculate_subquery_executions(const Item_subselect *subquery,
                                     Opt_trace_context *trace);

extern const char *antijoin_null_cond;

/**
  Checks if an Item, which is constant for execution, can be evaluated during
  optimization. It cannot be evaluated if it contains a subquery and the
  OPTION_NO_SUBQUERY_DURING_OPTIMIZATION query option is active.

  @param item    the Item to check
  @param select  the query block that contains the Item
  @return false if this Item contains a subquery and subqueries cannot be
  evaluated during optimization, or true otherwise
*/
bool evaluate_during_optimization(const Item *item, const Query_block *select);

/**
  Find the multiple equality predicate containing a field.

  The function retrieves the multiple equalities accessed through
  the cond_equal structure from current level and up looking for
  an equality containing a field. It stops retrieval as soon as the equality
  is found and set up inherited_fl to true if it's found on upper levels.

  @param cond_equal          multiple equalities to search in
  @param item_field          field to look for
  @param[out] inherited_fl   set up to true if multiple equality is found
                             on upper levels (not on current level of
                             cond_equal)

  @return
    - Item_equal for the found multiple equality predicate if a success;
    - nullptr otherwise.
*/
Item_equal *find_item_equal(COND_EQUAL *cond_equal,
                            const Item_field *item_field, bool *inherited_fl);

/**
  Find an artificial cap for ref access. This is mostly a crutch to mitigate
  that we don't estimate the cache effects of ref accesses properly
  (ie., normally, if we do many, they will hit cache instead of being
  separate seeks). Given to find_cost_for_ref().
 */
double find_worst_seeks(const TABLE *table, double num_rows,
                        double table_scan_cost);

/**
  Whether a ref lookup of “right_item” on “field” will give an exact
  comparison in all cases, ie., one can remove any further checks on
  field = right_item. If not, there may be false positives, and one
  needs to keep the comparison after the ref lookup.

  @param thd            thread handler
  @param field          field that is looked up through an index
  @param right_item     value used to perform look up
  @param[out] subsumes  true if an exact comparison can be done, false otherwise

  @returns false if success, true if error
 */
bool ref_lookup_subsumes_comparison(THD *thd, Field *field, Item *right_item,
                                    bool *subsumes);

/**
  Checks if we need to create iterators for this query. We usually have to. The
  exception is if a secondary engine is used, and that engine will offload the
  query execution to an external executor using #JOIN::override_executor_func.
  In this case, the external executor will use its own execution structures and
  we don't need to bother with creating the iterators needed by the MySQL
  executor.
 */
bool IteratorsAreNeeded(const THD *thd, AccessPath *root_path);

/**
  Estimates the number of base table row accesses that will be performed when
  executing a query using the given plan.

  @param path The access path representing the plan.
  @param num_evaluations The number of times this path is expected to be
  evaluated during a single execution of the query.
  @param limit The maximum number of rows expected to be read from this path.
  @return An estimate of the number of row accesses.
 */
double EstimateRowAccesses(const AccessPath *path, double num_evaluations,
                           double limit);

#endif /* SQL_OPTIMIZER_INCLUDED */

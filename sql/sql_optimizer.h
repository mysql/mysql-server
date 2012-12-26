#ifndef SQL_OPTIMIZER_INCLUDED
#define SQL_OPTIMIZER_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


/** @file Classes used for query optimizations */

/*
   This structure is used to collect info on potentially sargable
   predicates in order to check whether they become sargable after
   reading const tables.
   We form a bitmap of indexes that can be used for sargable predicates.
   Only such indexes are involved in range analysis.
*/

#include "opt_explain_format.h"

typedef struct st_sargable_param
{
  Field *field;              /* field against which to check sargability */
  Item **arg_value;          /* values of potential keys for lookups     */
  uint num_values;           /* number of values in the above array      */
} SARGABLE_PARAM;

typedef struct st_rollup
{
  enum State { STATE_NONE, STATE_INITED, STATE_READY };
  State state;
  Item_null_array null_items;
  Ref_ptr_array *ref_pointer_arrays;
  List<Item> *fields;
} ROLLUP;

class JOIN :public Sql_alloc
{
  JOIN(const JOIN &rhs);                        /**< not implemented */
  JOIN& operator=(const JOIN &rhs);             /**< not implemented */
public:
  JOIN_TAB *join_tab,**best_ref;
  JOIN_TAB **map2table;    ///< mapping between table indexes and JOIN_TABs
  TABLE    **table;
  /*
    The table which has an index that allows to produce the requried ordering.
    A special value of 0x1 means that the ordering will be produced by
    passing 1st non-const table to filesort(). NULL means no such table exists.
  */
  TABLE    *sort_by_table;
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
    - "tmp_tables" is 0, 1 or 2 and counts the maximum possible number of
      intermediate tables in post-processing (ie sorting and duplicate removal).
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
  uint     tables;         ///< Total number of tables in query block
  uint     primary_tables; ///< Number of primary input tables in query block
  uint     const_tables;   ///< Number of primary tables deemed constant
  uint     tmp_tables;     ///< Number of temporary tables used by query
  uint     send_group_parts;
  /**
    Indicates that grouping will be performed on the result set during
    query execution. This field belongs to query execution.

    @see make_group_fields, alloc_group_fields, JOIN::exec
  */
  bool     sort_and_group; 
  bool     first_record,full_join, no_field_update;
  bool     group;            ///< If query contains GROUP BY clause
  bool     do_send_rows;
  table_map all_table_map;   ///< Set of tables contained in query
  table_map const_table_map; ///< Set of tables found to be const
  /**
     Const tables which are either:
     - not empty
     - empty but inner to a LEFT JOIN, thus "considered" not empty for the
     rest of execution (a NULL-complemented row will be used).
  */
  table_map found_const_table_map;
  table_map outer_join;      ///< Bitmap of all inner tables from outer joins
  /* Number of records produced after join + group operation */
  ha_rows  send_records;
  ha_rows found_records,examined_rows,row_limit;
  // m_select_limit is used to decide if we are likely to scan the whole table.
  ha_rows m_select_limit;
  /**
    Used to fetch no more than given amount of rows per one
    fetch operation of server side cursor.
    The value is checked in end_send and end_send_group in fashion, similar
    to offset_limit_cnt:
      - fetch_limit= HA_POS_ERROR if there is no cursor.
      - when we open a cursor, we set fetch_limit to 0,
      - on each fetch iteration we add num_rows to fetch to fetch_limit
  */
  ha_rows  fetch_limit;

  /**
     Minimum number of matches that is needed to use JT_FT access.
     @see optimize_fts_limit_query
  */
  ha_rows  min_ft_matches;

  /* Finally picked QEP. This is result of join optimization */
  POSITION *best_positions;

/******* Join optimization state members start *******/
  
  /* Current join optimization state */
  POSITION *positions;  

  /* We also maintain a stack of join optimization states in * join->positions[] */
/******* Join optimization state members end *******/


  Next_select_func first_select;
  /**
    The cost of best complete join plan found so far during optimization,
    after optimization phase - cost of picked join order (not taking into
    account the changes made by test_if_skip_sort_order()).
  */
  double   best_read;
  /**
    The estimated row count of the plan with best read time (see above).
  */
  ha_rows  best_rowcount;
  List<Item> *fields;
  List<Cached_item> group_fields, group_fields_cache;
  THD	   *thd;
  Item_sum  **sum_funcs, ***sum_funcs_end;
  /** second copy of sumfuncs (for queries with 2 temporary tables */
  Item_sum  **sum_funcs2, ***sum_funcs_end2;
  ulonglong  select_options;
  select_result *result;
  TMP_TABLE_PARAM tmp_table_param;
  MYSQL_LOCK *lock;
  /// unit structure (with global parameters) for this select
  SELECT_LEX_UNIT *unit;
  /// select that processed
  SELECT_LEX *select_lex;
  /** 
    TRUE <=> optimizer must not mark any table as a constant table.
    This is needed for subqueries in form "a IN (SELECT .. UNION SELECT ..):
    when we optimize the select that reads the results of the union from a
    temporary table, we must not mark the temp. table as constant because
    the number of rows in it may vary from one subquery execution to another.
  */
  bool no_const_tables; 
  
  ROLLUP rollup;				///< Used with rollup

  bool select_distinct;				///< Set if SELECT DISTINCT
  /**
    If we have the GROUP BY statement in the query,
    but the group_list was emptied by optimizer, this
    flag is TRUE.
    It happens when fields in the GROUP BY are from
    constant table
  */
  bool group_optimized_away;

  /*
    simple_xxxxx is set if ORDER/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if ORDER/GROUP BY is empty.
    Used for deciding for or against using a temporary table to compute 
    GROUP/ORDER BY.
  */
  bool simple_order, simple_group;

  /*
    ordered_index_usage is set if an ordered index access
    should be used instead of a filesort when computing 
    ORDER/GROUP BY.
  */
  enum
  {
    ordered_index_void,       // No ordered index avail.
    ordered_index_group_by,   // Use index for GROUP BY
    ordered_index_order_by    // Use index for ORDER BY
  } ordered_index_usage;

  /**
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool no_order;
  /** Is set if we have a GROUP BY and we have ORDER BY on a constant. */
  bool          skip_sort_order;

  bool need_tmp, hidden_group_fields;

  Key_use_array keyuse;

  List<Item> all_fields; ///< to store all expressions used in query
  ///Above list changed to use temporary table
  List<Item> tmp_all_fields1, tmp_all_fields2, tmp_all_fields3;
  ///Part, shared with list above, emulate following list
  List<Item> tmp_fields_list1, tmp_fields_list2, tmp_fields_list3;
  List<Item> &fields_list; ///< hold field list passed to mysql_select
  List<Item> procedure_fields_list;
  int error; ///< set in optimize(), exec(), prepare_result()

  /**
    Wrapper for ORDER* pointer to trace origins of ORDER list 
    
    As far as ORDER is just a head object of ORDER expression
    chain, we need some wrapper object to associate flags with
    the whole ORDER list.
  */
  class ORDER_with_src
  {
    /**
      Private empty class to implement type-safe NULL assignment

      This private utility class allows us to implement a constructor
      from NULL and only NULL (or 0 -- this is the same thing) and
      an assignment operator from NULL.
      Assignments from other pointers still prohibited since other
      pointer types are incompatible with the "null" type, and the
      casting is impossible outside of ORDER_with_src class, since
      the "null" type is private.
    */
    struct null {};

  public:
    ORDER *order;  //< ORDER expression that we are wrapping with this class
    Explain_sort_clause src; //< origin of order list

  private:
    int flags; //< bitmap of Explain_sort_property

  public:
    ORDER_with_src() { clean(); }

    ORDER_with_src(ORDER *order_arg, Explain_sort_clause src_arg)
    : order(order_arg), src(src_arg), flags(order_arg ? ESP_EXISTS : ESP_none)
    {}

    /**
      Type-safe NULL assignment

      See a commentary for the "null" type above.
    */
    ORDER_with_src &operator=(null *) { clean(); return *this; }

    /**
      Type-safe constructor from NULL

      See a commentary for the "null" type above.
    */
    ORDER_with_src(null *) { clean(); }

    /**
      Transparent access to the wrapped order list

      These operators are safe, since we don't do any conversion of
      ORDER_with_src value, but just an access to the wrapped
      ORDER pointer value. 
      We can use ORDER_with_src objects instead ORDER pointers in
      a transparent way without accessor functions.

      @note     This operator also implements safe "operator bool()"
                functionality.
    */
    operator       ORDER *()       { return order; }
    operator const ORDER *() const { return order; }

    ORDER* operator->() const { return order; }
 
    void clean() { order= NULL; src= ESC_none; flags= ESP_none; }

    void set_flag(Explain_sort_property flag)
    {
      DBUG_ASSERT(order);
      flags|= flag;
    }
    void reset_flag(Explain_sort_property flag) { flags&= ~flag; }
    bool get_flag(Explain_sort_property flag) const {
      DBUG_ASSERT(order);
      return flags & flag;
    }
    int get_flags() const { DBUG_ASSERT(order); return flags; }
  };

  /**
    ORDER BY and GROUP BY lists, to transform with prepare,optimize and exec
  */
  ORDER_with_src order, group_list;

  /**
    Buffer to gather GROUP BY, ORDER BY and DISTINCT QEP details for EXPLAIN
  */
  Explain_format_flags explain_flags;

  /** 
    JOIN::having is initially equal to select_lex->having, but may
    later be changed by optimizations performed by JOIN.
    The relationship between the JOIN::having condition and the
    associated variable select_lex->having_value is so that
    having_value can be:
     - COND_UNDEF if a having clause was not specified in the query or
       if it has not been optimized yet
     - COND_TRUE if the having clause is always true, in which case
       JOIN::having is set to NULL.
     - COND_FALSE if the having clause is impossible, in which case
       JOIN::having is set to NULL
     - COND_OK otherwise, meaning that the having clause needs to be
       further evaluated
    All of the above also applies to the conds/select_lex->cond_value
    pair.
  */
  Item       *conds;                      ///< The where clause item tree
  Item       *having;                     ///< The having clause item tree
  Item       *having_for_explain;    ///< Saved optimized HAVING for EXPLAIN
  TABLE_LIST *tables_list;           ///<hold 'tables' parameter of mysql_select
  List<TABLE_LIST> *join_list;       ///< list of joined tables in reverse order
  COND_EQUAL *cond_equal;
  /*
    Join tab to return to. Points to an element of join->join_tab array, or to
    join->join_tab[-1].
    This is used at execution stage to shortcut join enumeration. Currently
    shortcutting is done to handle outer joins or handle semi-joins with
    FirstMatch strategy.
  */
  JOIN_TAB *return_tab;

  /*
    Used pointer reference for this select.
    select_lex->ref_pointer_array contains five "slices" of the same length:
    |========|========|========|========|========|
     ref_ptrs items0   items1   items2   items3
   */
  Ref_ptr_array ref_ptrs;
  // Copy of the initial slice above, to be used with different lists
  Ref_ptr_array items0, items1, items2, items3;
  // Used by rollup, to restore ref_ptrs after overwriting it.
  Ref_ptr_array current_ref_ptrs;

  const char *zero_result_cause; ///< not 0 if exec must return zero result
  
  bool union_part; ///< this subselect is part of union 
  bool optimized; ///< flag to avoid double optimization in EXPLAIN

  /**
     True if, at this stage of processing, subquery materialization is allowed
     for children subqueries of this JOIN (those in the SELECT list, in WHERE,
     etc). If false, and we have to evaluate a subquery at this stage, then we
     must choose EXISTS.
  */
  bool child_subquery_can_materialize;
  /**
     True if plan search is allowed to use references to expressions outer to
     this JOIN (for example may set up a 'ref' access looking up an outer
     expression in the index, etc).
  */
  bool allow_outer_refs;

  // true: No need to run DTORs on pointers.
  Mem_root_array<Item_exists_subselect*, true> sj_subselects;

  /* Temporary tables used to weed-out semi-join duplicates */
  List<TABLE> sj_tmp_tables;
  List<Semijoin_mat_exec> sjm_exec_list;
  /* end of allocation caching storage */

  /** TRUE <=> ref_pointer_array is set to items3. */
  bool set_group_rpa;
  /** Exec time only: TRUE <=> current group has been sent */
  bool group_sent;

  JOIN(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
    : keyuse(thd_arg->mem_root),
      fields_list(fields_arg),
      sj_subselects(thd_arg->mem_root)
  {
    init(thd_arg, fields_arg, select_options_arg, result_arg);
  }

  void init(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
  {
    join_tab= 0;
    tables= 0;
    primary_tables= 0;
    const_tables= 0;
    tmp_tables= 0;
    const_table_map= 0;
    join_list= 0;
    implicit_grouping= FALSE;
    sort_and_group= 0;
    first_record= 0;
    do_send_rows= 1;
    send_records= 0;
    found_records= 0;
    fetch_limit= HA_POS_ERROR;
    min_ft_matches= HA_POS_ERROR;
    examined_rows= 0;
    thd= thd_arg;
    sum_funcs= sum_funcs2= 0;
    having= having_for_explain= 0;
    select_options= select_options_arg;
    result= result_arg;
    lock= thd_arg->lock;
    select_lex= 0; //for safety
    select_distinct= test(select_options & SELECT_DISTINCT);
    no_order= 0;
    simple_order= 0;
    simple_group= 0;
    ordered_index_usage= ordered_index_void;
    skip_sort_order= 0;
    need_tmp= 0;
    hidden_group_fields= 0; /*safety*/
    error= 0;
    return_tab= 0;
    ref_ptrs.reset();
    items0.reset();
    items1.reset();
    items2.reset();
    items3.reset();
    zero_result_cause= 0;
    optimized= child_subquery_can_materialize= false;
    cond_equal= 0;
    group_optimized_away= 0;

    all_fields= fields_arg;
    if (&fields_list != &fields_arg)      /* Avoid valgrind-warning */
      fields_list= fields_arg;
    keyuse.clear();
    tmp_table_param.init();
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;

    no_const_tables= FALSE;
    /* can help debugging (makes smaller test cases): */
    DBUG_EXECUTE_IF("no_const_tables",no_const_tables= TRUE;);
    first_select= sub_select;
    set_group_rpa= false;
    group_sent= 0;
  }

  /// True if plan is const, ie it will return zero or one rows.
  bool plan_is_const() const { return const_tables == primary_tables; }

  /**
    True if plan contains one non-const primary table (ie not including
    tables taking part in semi-join materialization).
  */
  bool plan_is_single_table() { return primary_tables - const_tables == 1; }

  int prepare(TABLE_LIST *tables, uint wind_num,
	      Item *conds, uint og_num, ORDER *order, ORDER *group,
              Item *having,
              SELECT_LEX *select, SELECT_LEX_UNIT *unit);
  int optimize();
  void reset();
  void exec();
  bool prepare_result(List<Item> **columns_list);
  void explain();
  bool destroy();
  void restore_tmp();
  bool alloc_func_list();
  bool flatten_subqueries();
  bool make_sum_func_list(List<Item> &all_fields,
                          List<Item> &send_fields,
                          bool before_group_by, bool recompute= FALSE);

  /// Initialzes a slice, see comments for ref_ptrs above.
  Ref_ptr_array ref_ptr_array_slice(size_t slice_num)
  {
    size_t slice_sz= select_lex->ref_pointer_array.size() / 5U;
    DBUG_ASSERT(select_lex->ref_pointer_array.size() % 5 == 0);
    DBUG_ASSERT(slice_num < 5U);
    return Ref_ptr_array(&select_lex->ref_pointer_array[slice_num * slice_sz],
                         slice_sz);
  }

  /**
     Overwrites one slice with the contents of another slice.
     In the normal case, dst and src have the same size().
     However: the rollup slices may have smaller size than slice_sz.
   */
  void copy_ref_ptr_array(Ref_ptr_array dst_arr, Ref_ptr_array src_arr)
  {
    DBUG_ASSERT(dst_arr.size() >= src_arr.size());
    void *dest= dst_arr.array();
    const void *src= src_arr.array();
    memcpy(dest, src, src_arr.size() * src_arr.element_size());
  }

  /// Overwrites 'ref_ptrs' and remembers the the source as 'current'.
  void set_items_ref_array(Ref_ptr_array src_arr)
  {
    copy_ref_ptr_array(ref_ptrs, src_arr);
    current_ref_ptrs= src_arr;
  }

  /// Initializes 'items0' and remembers that it is 'current'.
  void init_items_ref_array()
  {
    items0= ref_ptr_array_slice(1);
    copy_ref_ptr_array(items0, ref_ptrs);
    current_ref_ptrs= items0;
  }

  bool rollup_init();
  bool rollup_process_const_fields();
  bool rollup_make_fields(List<Item> &all_fields, List<Item> &fields,
			  Item_sum ***func);
  int rollup_send_data(uint idx);
  int rollup_write_data(uint idx, TABLE *table);
  void remove_subq_pushed_predicates(Item **where);
  /**
    Release memory and, if possible, the open tables held by this execution
    plan (and nested plans). It's used to release some tables before
    the end of execution in order to increase concurrency and reduce
    memory consumption.
  */
  void join_free();
  /** Cleanup this JOIN, possibly for reuse */
  void cleanup(bool full);
  void clear();
  bool save_join_tab();
  void restore_join_tab();
  bool init_save_join_tab();
  /**
    Return whether the caller should send a row even if the join 
    produced no rows if:
     - there is an aggregate function (sum_func_count!=0), and
     - the query is not grouped, and
     - a possible HAVING clause evaluates to TRUE.

    @note: if there is a having clause, it must be evaluated before
    returning the row.
  */
  bool send_row_on_empty_set() const
  {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
	    group_list == NULL && !group_optimized_away &&
            select_lex->having_value != Item::COND_FALSE);
  }
  bool change_result(select_result *result);
  bool is_top_level_join() const
  {
    return (unit == &thd->lex->unit && (unit->fake_select_lex == 0 ||
                                        select_lex == unit->fake_select_lex));
  }
  bool cache_const_exprs();
  bool generate_derived_keys();
  void drop_unused_derived_keys();
  bool get_best_combination();
  bool update_equalities_for_sjm();
  bool add_sorting_to_table(JOIN_TAB *tab, ORDER_with_src *order);
  bool decide_subquery_strategy();
  void refine_best_rowcount();

private:
  /**
    Execute current query. To be called from @c JOIN::exec.

    If current query is a dependent subquery, this execution is performed on a
    temporary copy of the original JOIN object in order to be able to restore
    the original content for re-execution and EXPLAIN. (@note Subqueries may
    be executed as part of EXPLAIN.) In such cases, execution data that may be
    reused for later executions will be copied to the original 
    @c JOIN object (@c parent).

    @param parent Original @c JOIN object when current object is a temporary 
                  copy. @c NULL, otherwise
  */
  void execute(JOIN *parent);
  /**
    Send current query result set to the client. To be called from JOIN::execute

    @note       Explain skips this call during JOIN::execute() execution
  */
  void send_data();
  
  /**
    Create a temporary table to be used for processing DISTINCT/ORDER
    BY/GROUP BY.

    @note Will modify JOIN object wrt sort/group attributes

    @param tab              the JOIN_TAB object to attach created table to
    @param tmp_table_fields List of items that will be used to define
                            column types of the table.
    @param tmp_table_group  Group key to use for temporary table, NULL if none.
    @param save_sum_fields  If true, do not replace Item_sum items in 
                            @c tmp_fields list with Item_field items referring 
                            to fields in temporary table.

    @returns false on success, true on failure
  */
  bool create_intermediate_table(JOIN_TAB *tab, List<Item> *tmp_table_fields,
                                 ORDER_with_src &tmp_table_group,
                                 bool save_sum_fields);
  /**
    Create the first temporary table to be used for processing DISTINCT/ORDER
    BY/GROUP BY.
  */
  bool create_first_intermediate_table();
  /**
    Optimize distinct when used on a subset of the tables.

    E.g.,: SELECT DISTINCT t1.a FROM t1,t2 WHERE t1.b=t2.b
    In this case we can stop scanning t2 when we have found one t1.a
  */
  void optimize_distinct();

  /** 
      Optimize FTS queries where JT_FT access has been selected.

      The following optimization is may be applied:
      1. Skip filesort if FTS result is ordered
      2. Skip accessing table rows if FTS result contains necessary information
      Also verifize that LIMIT optimization was sound.

      @note Optimizations are restricted to single table queries, and the table
            engine needs to support the extended FTS API.
   */
  void optimize_fts_query();


  /**
     Optimize FTS queries with ORDER BY/LIMIT, but no WHERE clause.
   */
  void optimize_fts_limit_query();

  /**
     Replace all Item_field objects with the given field name with the
     given item in all parts of the query.

     @todo So far this function only handles SELECT list and WHERE clause,
           For more general use, ON clause, ORDER BY list, GROUP BY list and
	   HAVING clause also needs to be handled.

     @param field_name Name of the field to search for
     @param new_item Replacement item
  */
  void replace_item_field(const char* field_name, Item* new_item);

  /**
    TRUE if the query contains an aggregate function but has no GROUP
    BY clause. 
  */
  bool implicit_grouping; 

  void set_prefix_tables();
  void cleanup_item_list(List<Item> &items) const;
  void set_semijoin_info();
  bool set_access_methods();
  bool setup_materialized_table(JOIN_TAB *tab, uint tableno,
                                const POSITION *inner_pos,
                                POSITION *sjm_pos);
  bool make_tmp_tables_info();
  bool compare_costs_of_subquery_strategies(
         Item_exists_subselect::enum_exec_method *method);

  /// RAII class to ease the call of LEX::mark_broken() if error
  class Prepare_error_tracker
  {
public:
    Prepare_error_tracker(THD *thd_arg) : thd(thd_arg) {}
    ~Prepare_error_tracker()
    {
      if (unlikely(thd->is_error()))
        thd->lex->mark_broken();
    }
private:
    THD *const thd;
  };

};


bool uses_index_fields_only(Item *item, TABLE *tbl, uint keyno, 
                            bool other_tbls_ok);
void update_depend_map(JOIN *join);
void reset_nj_counters(List<TABLE_LIST> *join_list);
Item *remove_eq_conds(THD *thd, Item *cond, Item::cond_result *cond_value);
Item *optimize_cond(THD *thd, Item *conds, COND_EQUAL **cond_equal,
                    List<TABLE_LIST> *join_list,
                    bool build_equalities, Item::cond_result *cond_value);
Item* substitute_for_best_equal_field(Item *cond,
                                      COND_EQUAL *cond_equal,
                                      void *table_join_idx);
Item *build_equal_items(THD *thd, Item *cond,
                        COND_EQUAL *inherited, bool do_inherit,
                        List<TABLE_LIST> *join_list,
                        COND_EQUAL **cond_equal_ref);
bool is_indexed_agg_distinct(JOIN *join, List<Item_field> *out_args);
Key_use_array *create_keyuse_for_table(THD *thd, TABLE *table, uint keyparts,
                                       Item_field **fields,
                                       List<Item> outer_exprs);
Item_equal *find_item_equal(COND_EQUAL *cond_equal, Field *field,
                            bool *inherited_fl);
Item_field *get_best_field(Item_field *item_field, COND_EQUAL *cond_equal);
Item *
make_cond_for_table(Item *cond, table_map tables, table_map used_table,
                    bool exclude_expensive_cond);

/**
   Returns true if arguments are a temporal Field having no date,
   part and a temporal expression having a date part.
   @param  f  Field
   @param  v  Expression
 */
inline bool field_time_cmp_date(const Field *f, const Item *v)
{
  return f->is_temporal() && !f->is_temporal_with_date() &&
    v->is_temporal_with_date();
}

#endif /* SQL_OPTIMIZER_INCLUDED */

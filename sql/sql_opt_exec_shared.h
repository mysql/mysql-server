/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

/// @file Common types of the Optimizer, used by optimization and execution

#ifndef SQL_OPT_EXEC_SHARED_INCLUDED
#define SQL_OPT_EXEC_SHARED_INCLUDED

#include "my_base.h"
#include "item.h"        // Item
#include "sql_alloc.h"   // Sql_alloc
#include "sql_class.h"   // Temp_table_param

class JOIN;
class Item_func_match;
class store_key;
class QUICK_SELECT_I;

/**
   This represents the index of a JOIN_TAB/QEP_TAB in an array. "plan_idx": "Plan
   Table Index".
   It is signed, because:
   - firstmatch_return may be PRE_FIRST_PLAN_IDX (it can happen that the first
   table of the plan uses FirstMatch: SELECT ... WHERE literal IN (SELECT
   ...)).
   - it must hold the invalid value NO_PLAN_IDX (which means "no
   JOIN_TAB/QEP_TAB", equivalent of NULL pointer); this invalid value must
   itself be different from PRE_FIRST_PLAN_IDX, to distinguish "FirstMatch to
   before-first-table" (firstmatch_return==PRE_FIRST_PLAN_IDX) from "No
   FirstMatch" (firstmatch_return==NO_PLAN_IDX).
*/
typedef int8 plan_idx;
#define NO_PLAN_IDX (-2)          ///< undefined index
#define PRE_FIRST_PLAN_IDX (-1) ///< right before the first (first's index is 0)


typedef struct st_table_ref : public Sql_alloc
{
  bool		key_err;
  /** True if something was read into buffer in join_read_key.  */
  bool          has_record;
  uint          key_parts;                ///< num of ...
  uint          key_length;               ///< length of key_buff
  int           key;                      ///< key no
  uchar         *key_buff;                ///< value to look for with key
  uchar         *key_buff2;               ///< key_buff+key_length
  /**
     Used to store the value from each keypart field. These values are
     used for ref access. If key_copy[key_part] == NULL it means that
     the value is constant and does not need to be reevaluated
  */
  store_key     **key_copy;
  Item          **items;                  ///< val()'s for each keypart
  /*  
    Array of pointers to trigger variables. Some/all of the pointers may be
    NULL.  The ref access can be used iff
    
      for each used key part i, (!cond_guards[i] || *cond_guards[i]) 

    This array is used by subquery code. The subquery code may inject
    triggered conditions, i.e. conditions that can be 'switched off'. A ref 
    access created from such condition is not valid when at least one of the 
    underlying conditions is switched off (see subquery code for more details).
    If a table in a subquery has this it means that the table access 
    will switch from ref access to table scan when the outer query 
    produces a NULL value to be checked for in the subquery. This will
    be used by NOT IN subqueries and IN subqueries for which 
    is_top_level_item() returns false.
  */
  bool          **cond_guards;
  /**
    (null_rejecting & (1<<i)) means the condition is '=' and no matching
    rows will be produced if items[i] IS NULL (see add_not_null_conds())
  */
  key_part_map  null_rejecting;
  table_map	depend_map;		  ///< Table depends on these tables.
  /* null byte position in the key_buf. Used for REF_OR_NULL optimization */
  uchar          *null_ref_key;
  /*
    The number of times the record associated with this key was used
    in the join.
  */
  ha_rows       use_count;

  /*
    TRUE <=> disable the "cache" as doing lookup with the same key value may
    produce different results (because of Index Condition Pushdown)
  */
  bool          disable_cache;

  st_table_ref()
    : key_err(TRUE),
      has_record(FALSE),
      key_parts(0),
      key_length(0),
      key(-1),
      key_buff(NULL),
      key_buff2(NULL),
      key_copy(NULL),
      items(NULL),
      cond_guards(NULL),
      null_rejecting(0),
      depend_map(0),
      null_ref_key(NULL),
      use_count(0),
      disable_cache(FALSE)
  {
  }

  /**
    @returns whether the reference contains NULL values which could never give
    a match.
  */
  bool impossible_null_ref() const
  {
    if (null_rejecting != 0)
    {
      for (uint i= 0 ; i < key_parts ; i++)
      {
        if ((null_rejecting & 1 << i) && items[i]->is_null())
          return TRUE;
      }
    }
    return FALSE;
  }


  /**
    Check if there are triggered/guarded conditions that might be
    'switched off' by the subquery code when executing 'Full scan on
    NULL key' subqueries.

    @return true if there are guarded conditions, false otherwise
  */

  bool has_guarded_conds() const
  {
    DBUG_ASSERT(key_parts == 0 || cond_guards != NULL);

    for (uint i = 0; i < key_parts; i++)
    {
      if (cond_guards[i])
        return true;
    }
    return false;
  }
} TABLE_REF;


struct st_cache_field;
class QEP_operation;
class Filesort;
typedef struct st_position POSITION;
class Semijoin_mat_exec;

/*
  The structs which holds the join connections and join states
*/
enum join_type { /*
                   Initial state. Access type has not yet been decided
                   for the table
                 */
                 JT_UNKNOWN,
                 /* Table has exactly one row */
                 JT_SYSTEM,
                 /*
                   Table has at most one matching row. Values read
                   from this row can be treated as constants. Example:
                   "WHERE table.pk = 3"
                  */
                 JT_CONST,
                 /*
                   '=' operator is used on unique index. At most one
                   row is read for each combination of rows from
                   preceding tables
                 */
                 JT_EQ_REF,
                 /*
                   '=' operator is used on non-unique index
                 */
                 JT_REF,
                 /*
                   Full table scan.
                 */
                 JT_ALL,
                 /*
                   Range scan.
                 */
                 JT_RANGE,
                 /*
                   Like table scan, but scans index leaves instead of
                   the table
                 */
                 JT_INDEX_SCAN,
                 /* Fulltext index is used */
                 JT_FT,
                 /*
                   Like ref, but with extra search for NULL values.
                   E.g. used for "WHERE col = ... OR col IS NULL"
                  */
                 JT_REF_OR_NULL,
                 /*
                   Like eq_ref for subqueries: Replaces subquery with
                   index lookup in unique index
                  */
                 JT_UNIQUE_SUBQUERY,
                 /*
                   Like unique_subquery but for non-unique index
                 */
                 JT_INDEX_SUBQUERY,
                 /*
                   Do multiple range scans over one table and combine
                   the results into one. The merge can be used to
                   produce unions and intersections
                 */
                 JT_INDEX_MERGE};


/// Holds members common to JOIN_TAB and QEP_TAB.
class QEP_shared : public Sql_alloc
{
public:
  QEP_shared() :
    m_join(NULL),
    m_idx(NO_PLAN_IDX),
    m_table(NULL),
    m_position(NULL),
    m_sj_mat_exec(NULL),
    m_first_sj_inner(NO_PLAN_IDX),
    m_last_sj_inner(NO_PLAN_IDX),
    m_first_inner(NO_PLAN_IDX),
    m_last_inner(NO_PLAN_IDX),
    m_first_upper(NO_PLAN_IDX),
    m_ref(),
    m_index(0),
    m_type(JT_UNKNOWN),
    m_condition(NULL),
    m_keys(),
    m_records(0),
    m_quick(NULL),
    prefix_tables_map(0),
    added_tables_map(0),
    m_ft_func(NULL)
    {}

  /*
    Simple getters and setters. They are public. However, this object is
    protected in QEP_shared_owner, so only that class and its children
    (JOIN_TAB, QEP_TAB) can access the getters and setters.
  */

  JOIN *join() const { return m_join; }
  void set_join(JOIN *j) { m_join= j; }
  plan_idx idx() const
  {
    DBUG_ASSERT(m_idx >= 0);                    // Index must be valid
    return m_idx;
  }
  void set_idx(plan_idx i)
  {
    DBUG_ASSERT(m_idx == NO_PLAN_IDX);      // Index should not change in lifetime
    m_idx= i;
  }
  TABLE *table() const { return m_table; }
  void set_table(TABLE *t) { m_table= t; }
  POSITION *position() const { return m_position; }
  void set_position(POSITION *p) { m_position= p; }
  Semijoin_mat_exec *sj_mat_exec() const { return m_sj_mat_exec; }
  void set_sj_mat_exec(Semijoin_mat_exec *s) { m_sj_mat_exec= s; }
  plan_idx first_sj_inner() { return m_first_sj_inner; }
  plan_idx last_sj_inner() { return m_last_sj_inner; }
  plan_idx first_inner() { return m_first_inner; }
  void set_first_inner(plan_idx i) { m_first_inner= i; }
  void set_last_inner(plan_idx i) { m_last_inner= i; }
  void set_first_sj_inner(plan_idx i) { m_first_sj_inner= i; }
  void set_last_sj_inner(plan_idx i) { m_last_sj_inner= i; }
  void set_first_upper(plan_idx i) { m_first_upper= i; }
  plan_idx last_inner() { return m_last_inner; }
  plan_idx first_upper() { return m_first_upper; }
  TABLE_REF &ref() { return m_ref; }
  uint index() const { return m_index; }
  void set_index(uint i) { m_index= i; }
  enum join_type type() const { return m_type; }
  void set_type(enum join_type t) { m_type= t; }
  Item *condition() const { return m_condition; }
  void set_condition(Item *c) { m_condition= c; }
  key_map &keys() { return m_keys; }
  ha_rows records() const { return m_records; }
  void set_records(ha_rows r) { m_records= r; }
  QUICK_SELECT_I *quick() const { return m_quick; }
  void set_quick(QUICK_SELECT_I *q) { m_quick= q; }
  table_map prefix_tables() const { return prefix_tables_map; }
  table_map added_tables() const { return added_tables_map; }
  Item_func_match *ft_func() const { return m_ft_func; }
  void set_ft_func(Item_func_match *f) { m_ft_func= f; }

  // More elaborate functions:

  /**
    Set available tables for a table in a join plan.

    @param prefix_tables_arg: Set of tables available for this plan
    @param prev_tables_arg: Set of tables available for previous table, used to
                            calculate set of tables added for this table.
  */
  void set_prefix_tables(table_map prefix_tables_arg, table_map prev_tables_arg)
  {
    prefix_tables_map= prefix_tables_arg;
    added_tables_map= prefix_tables_arg & ~prev_tables_arg;
  }

  /**
    Add an available set of tables for a table in a join plan.

    @param tables: Set of tables added for this table in plan.
  */
  void add_prefix_tables(table_map tables)
  { prefix_tables_map|= tables; added_tables_map|= tables; }

  bool is_first_inner_for_outer_join() const
  {
    return m_first_inner == m_idx;
  }

  bool is_inner_table_of_outer_join() const
  {
    return m_first_inner != NO_PLAN_IDX;
  }
  bool is_single_inner_of_semi_join() const
  {
    return m_first_sj_inner == m_idx && m_last_sj_inner == m_idx;
  }
  bool is_single_inner_of_outer_join() const
  {
    return m_first_inner == m_idx && m_last_inner == m_idx;
  }

private:

  JOIN	*m_join;

  /**
     Index of structure in array:
     - NO_PLAN_IDX if before get_best_combination()
     - index of pointer to this JOIN_TAB, in JOIN::best_ref array
     - index of this QEP_TAB, in JOIN::qep array.
  */
  plan_idx  m_idx;

  /// Corresponding table. Might be an internal temporary one.
  TABLE *m_table;

  /// Points into best_positions array. Includes cost info.
  POSITION      *m_position;

  /*
    semijoin-related members.
  */

  /**
    Struct needed for materialization of semi-join. Set for a materialized
    temporary table, and NULL for all other join_tabs (except when
    materialization is in progress, @see join_materialize_semijoin()).
  */
  Semijoin_mat_exec *m_sj_mat_exec;

  /**
    Boundaries of semijoin inner tables around this table. Valid only once
    final QEP has been chosen. Depending on the strategy, they may define an
    interval (all tables inside are inner of a semijoin) or
    not. last_sj_inner is not set for Duplicates Weedout.
  */
  plan_idx m_first_sj_inner, m_last_sj_inner;

  /*
    outer-join-related members.
  */
  plan_idx m_first_inner;   ///< first inner table for including outer join
  plan_idx m_last_inner;    ///< last table table for embedding outer join
  plan_idx m_first_upper;   ///< first inner table for embedding outer join

  /**
     Used to do index-based look up based on a key value.
     Used when we read constant tables, in misc optimization (like
     remove_const()), and in execution.
  */
  TABLE_REF	m_ref;

  /// ID of index used for index scan or semijoin LooseScan
  uint		m_index;

  /// Type of chosen access method (scan, etc).
  enum join_type m_type;

  /**
    Table condition, ie condition to be evaluated for a row from this table.
    Notice that the condition may refer to rows from previous tables in the
    join prefix, as well as outer tables.
  */
  Item          *m_condition;

  /**
     All keys with can be used.
     Used by add_key_field() (optimization time) and execution of dynamic
     range (join_init_quick_record()), and EXPLAIN.
  */
  key_map       m_keys;

  /**
     Either #rows in the table or 1 for const table.
     Used in optimization, and also in execution for FOUND_ROWS().
  */
  ha_rows	m_records;

  /**
     Non-NULL if quick-select used.
     Filled in optimization, used in execution to find rows, and in EXPLAIN.
  */
  QUICK_SELECT_I *m_quick;

  /*
    Maps below are shared because of dynamic range: in execution, it needs to
    know the prefix tables, to find the possible QUICK methods.
  */

  /**
    The set of all tables available in the join prefix for this table,
    including the table handled by this JOIN_TAB.
  */
  table_map     prefix_tables_map;
  /**
    The set of tables added for this table, compared to the previous table
    in the join prefix.
  */
  table_map     added_tables_map;

  /** FT function */
  Item_func_match *m_ft_func;
};


/// Owner of a QEP_shared; parent of JOIN_TAB and QEP_TAB.
class QEP_shared_owner
{
public:
  QEP_shared_owner() : m_qs(NULL) {}

  /// Instructs to share the QEP_shared with another owner
  void share_qs(QEP_shared_owner *other) { other->set_qs(m_qs); }
  void set_qs(QEP_shared *q) { DBUG_ASSERT(!m_qs); m_qs= q; }

  // Getters/setters forwarding to QEP_shared:

  JOIN *join() const { return m_qs->join(); }
  void set_join(JOIN *j) { return m_qs->set_join(j); }
  plan_idx idx() const { return m_qs->idx(); }
  void set_idx(plan_idx i) { return m_qs->set_idx(i); }
  TABLE *table() const { return m_qs->table(); }
  POSITION *position() const { return m_qs->position(); }
  void set_position(POSITION *p) { return m_qs->set_position(p); }
  Semijoin_mat_exec *sj_mat_exec() const { return m_qs->sj_mat_exec(); }
  void set_sj_mat_exec(Semijoin_mat_exec *s) { return m_qs->set_sj_mat_exec(s); }
  plan_idx first_sj_inner() const { return m_qs->first_sj_inner(); }
  plan_idx last_sj_inner() const { return m_qs->last_sj_inner(); }
  plan_idx first_inner() const { return m_qs->first_inner(); }
  plan_idx last_inner() const { return m_qs->last_inner(); }
  plan_idx first_upper() const { return m_qs->first_upper(); }
  void set_first_inner(plan_idx i) { return m_qs->set_first_inner(i); }
  void set_last_inner(plan_idx i) { return m_qs->set_last_inner(i); }
  void set_first_sj_inner(plan_idx i) { return m_qs->set_first_sj_inner(i); }
  void set_last_sj_inner(plan_idx i) { return m_qs->set_last_sj_inner(i); }
  void set_first_upper(plan_idx i) { return m_qs->set_first_upper(i); }
  TABLE_REF &ref() const { return m_qs->ref(); }
  uint index() const { return m_qs->index(); }
  void set_index(uint i) { return m_qs->set_index(i); }
  enum join_type type() const { return m_qs->type(); }
  void set_type(enum join_type t) { return m_qs->set_type(t); }
  Item *condition() const { return m_qs->condition(); }
  void set_condition(Item *to) { return m_qs->set_condition(to); }
  key_map &keys() { return m_qs->keys(); }
  ha_rows records() const { return m_qs->records(); }
  void set_records(ha_rows r) { return m_qs->set_records(r); }
  QUICK_SELECT_I *quick() const { return m_qs->quick(); }
  void set_quick(QUICK_SELECT_I *q) { return m_qs->set_quick(q); }
  table_map prefix_tables() const { return m_qs->prefix_tables(); }
  table_map added_tables() const { return m_qs->added_tables(); }
  Item_func_match *ft_func() const { return m_qs->ft_func(); }
  void set_ft_func(Item_func_match *f) { return m_qs->set_ft_func(f); }
  void set_prefix_tables(table_map prefix_tables, table_map prev_tables)
  { return m_qs->set_prefix_tables(prefix_tables, prev_tables); }
  void add_prefix_tables(table_map tables)
  { return m_qs->add_prefix_tables(tables); }
  bool is_single_inner_of_semi_join() const
  { return m_qs->is_single_inner_of_semi_join(); }
  bool is_inner_table_of_outer_join() const
  { return m_qs->is_inner_table_of_outer_join(); }
  bool is_first_inner_for_outer_join() const
  { return m_qs->is_first_inner_for_outer_join(); }
  bool is_single_inner_for_outer_join() const
  { return m_qs->is_single_inner_of_outer_join(); }

  bool has_guarded_conds() const
  { return ref().has_guarded_conds(); }
  bool and_with_condition(Item *tmp_cond);

  void qs_cleanup();

protected:
  QEP_shared *m_qs; // qs stands for Qep_Shared
};

#endif // SQL_OPT_EXEC_SHARED_INCLUDED

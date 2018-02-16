#ifndef SQL_SELECT_INCLUDED
#define SQL_SELECT_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file sql/sql_select.h
*/

#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <functional>

#include "binary_log_types.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_table_map.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"  // Item_cond_and
#include "sql/opt_costmodel.h"
#include "sql/set_var.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"    // THD
#include "sql/sql_cmd_dml.h"  // Sql_cmd_dml
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"  // join_type
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"

class Item_func;
class JOIN_TAB;
class KEY;
class QEP_TAB;
class Query_result;
class Temp_table_param;
template <class T>
class List;

typedef ulonglong nested_join_map;

class Sql_cmd_select : public Sql_cmd_dml {
 public:
  explicit Sql_cmd_select(Query_result *result_arg) : Sql_cmd_dml() {
    result = result_arg;
  }

  virtual enum_sql_command sql_command_code() const { return SQLCOM_SELECT; }

  virtual bool is_data_change_stmt() const { return false; }

 protected:
  virtual bool precheck(THD *thd);

  virtual bool prepare_inner(THD *thd);
};

/**
   Returns a constant of type 'type' with the 'A' lowest-weight bits set.
   Example: LOWER_BITS(uint, 3) == 7.
   Requirement: A < sizeof(type) * 8.
*/
#define LOWER_BITS(type, A) ((type)(((type)1 << (A)) - 1))

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS 1
#define KEY_OPTIMIZE_REF_OR_NULL 2
#define FT_KEYPART (MAX_REF_PARTS + 10)

/**
  A Key_use represents an equality predicate of the form (table.column = val),
  where the column is indexed by @c keypart in @c key and @c val is either a
  constant, a column from other table, or an expression over column(s) from
  other table(s). If @c val is not a constant, then the Key_use specifies an
  equi-join predicate, and @c table must be put in the join plan after all
  tables in @c used_tables.

  At an abstract level a Key_use instance can be viewed as a directed arc
  of an equi-join graph, where the arc originates from the table(s)
  containing the column(s) that produce the values used for index lookup
  into @c table, and the arc points into @c table.

  For instance, assuming there is only an index t3(c), the query

  @code
    SELECT * FROM t1, t2, t3
    WHERE t1.a = t3.c AND
          t2.b = t3.c;
  @endcode

  would generate two arcs (instances of Key_use)

  @code
     t1-- a ->- c --.
                    |
                    V
                    t3
                    ^
                    |
     t2-- b ->- c --'
  @endcode

  If there were indexes t1(a), and t2(b), then the equi-join graph
  would have two additional arcs "c->a" and "c->b" recording the fact
  that it is possible to perform lookup in either direction.

  @code
    t1-- a ->- c --.    ,-- c -<- b --- t2
     ^             |    |               ^
     |             |    |               |
     `-- a -<- c - v    v-- c ->- b ----'
                     t3
  @endcode

  The query

  @code
    SELECT * FROM t1, t2, t3 WHERE t1.a + t2.b = t3.c;
  @endcode

  can be viewed as a graph with one "multi-source" arc:

  @code
    t1-- a ---
              |
               >-- c --> t3
              |
    t2-- b ---
  @endcode

  The graph of all equi-join conditions usable for index lookup is
  stored as an ordered sequence of Key_use elements in
  JOIN::keyuse_array. See sort_keyuse() for details on the
  ordering. Each JOIN_TAB::keyuse points to the first array element
  with the same table.
*/
class Key_use {
 public:
  // We need the default constructor for unit testing.
  Key_use()
      : table_ref(NULL),
        val(NULL),
        used_tables(0),
        key(0),
        keypart(0),
        optimize(0),
        keypart_map(0),
        ref_table_rows(0),
        null_rejecting(false),
        cond_guard(NULL),
        sj_pred_no(UINT_MAX),
        bound_keyparts(0),
        fanout(0.0),
        read_cost(0.0) {}

  Key_use(TABLE_LIST *table_ref_arg, Item *val_arg, table_map used_tables_arg,
          uint key_arg, uint keypart_arg, uint optimize_arg,
          key_part_map keypart_map_arg, ha_rows ref_table_rows_arg,
          bool null_rejecting_arg, bool *cond_guard_arg, uint sj_pred_no_arg)
      : table_ref(table_ref_arg),
        val(val_arg),
        used_tables(used_tables_arg),
        key(key_arg),
        keypart(keypart_arg),
        optimize(optimize_arg),
        keypart_map(keypart_map_arg),
        ref_table_rows(ref_table_rows_arg),
        null_rejecting(null_rejecting_arg),
        cond_guard(cond_guard_arg),
        sj_pred_no(sj_pred_no_arg),
        bound_keyparts(0),
        fanout(0.0),
        read_cost(0.0) {}

  TABLE_LIST *table_ref;  ///< table owning the index

  /**
    Value used for lookup into @c key. It may be an Item_field, a
    constant or any other expression. If @c val contains a field from
    another table, then we have a join condition, and the table(s) of
    the field(s) in @c val should be before @c table in the join plan.
  */
  Item *val;

  /**
    All tables used in @c val, that is all tables that provide bindings
    for the expression @c val. These tables must be in the plan before
    executing the equi-join described by a Key_use.
  */
  table_map used_tables;
  uint key;                  ///< number of index
  uint keypart;              ///< used part of the index
  uint optimize;             ///< 0, or KEY_OPTIMIZE_*
  key_part_map keypart_map;  ///< like keypart, but as a bitmap
  ha_rows ref_table_rows;    ///< Estimate of how many rows for a key value
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
    Not used if the index is fulltext (such index cannot be used for
    equalities).
  */
  bool null_rejecting;
  /**
    !NULL - This Key_use was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of
            this Key_use element) can be turned on and off. The on/off state
            is indicted by the pointed value:
              *cond_guard == true @<=@> equality condition is on
              *cond_guard == false @<=@> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)

    Not used if the index is fulltext (such index cannot be used for
    equalities).
  */
  bool *cond_guard;
  /**
     0..63    @<=@> This was created from semi-join IN-equality # sj_pred_no.
     UINT_MAX  Otherwise

     Not used if the index is fulltext (such index cannot be used for
     semijoin).

     @see get_semi_join_select_list_index()
  */
  uint sj_pred_no;

  /*
    The three members below are different from the rest of Key_use: they are
    set only by Optimize_table_order, and they change with the currently
    considered join prefix.
  */

  /**
     The key columns which are equal to expressions depending only of earlier
     tables of the current join prefix.
     This information is stored only in the first Key_use of the index.
  */
  key_part_map bound_keyparts;

  /**
     Fanout of the ref access path for this index, in the current join
     prefix.
     This information is stored only in the first Key_use of the index.
  */
  double fanout;

  /**
    Cost of the ref access path for the current join prefix, i.e. the
    cost of using ref access once multiplied by estimated number of
    partial rows from tables earlier in the join sequence.
    read_cost does NOT include cost of processing rows on the
    server side (row_evaluate_cost).

    Example: If the cost of ref access on this index is 5, and the
    estimated number of partial rows from earlier tables is 10,
    read_cost=50.

    This information is stored only in the first Key_use of the index.
  */
  double read_cost;
};

/// @returns join type according to quick select type used
join_type calc_join_type(int quick_type);

class JOIN;

#define SJ_OPT_NONE 0
#define SJ_OPT_DUPS_WEEDOUT 1
#define SJ_OPT_LOOSE_SCAN 2
#define SJ_OPT_FIRST_MATCH 3
#define SJ_OPT_MATERIALIZE_LOOKUP 4
#define SJ_OPT_MATERIALIZE_SCAN 5

inline bool sj_is_materialize_strategy(uint strategy) {
  return strategy >= SJ_OPT_MATERIALIZE_LOOKUP;
}

/**
    Bits describing quick select type
*/
enum quick_type { QS_NONE, QS_RANGE, QS_DYNAMIC_RANGE };

/**
  A position of table within a join order. This structure is primarily used
  as a part of @c join->positions and @c join->best_positions arrays.

  One POSITION element contains information about:
   - Which table is accessed
   - Which access method was chosen
      = Its cost and \#of output records
   - Semi-join strategy choice. Note that there are two different
     representation formats:
      1. The one used during join optimization
      2. The one used at plan refinement/code generation stage.
      We call fix_semijoin_strategies_for_picked_join_order() to switch
      between #1 and #2. See that function's comment for more details.

   - Semi-join optimization state. When we're running join optimization,
     we main a state for every semi-join strategy which are various
     variables that tell us if/at which point we could consider applying the
     strategy.
     The variables are really a function of join prefix but they are too
     expensive to re-caclulate for every join prefix we consider, so we
     maintain current state in join->positions[\#tables_in_prefix]. See
     advance_sj_state() for details.

  This class has to stay a POD, because it is memcpy'd in many places.
*/

struct POSITION {
  /**
    The number of rows that will be fetched by the chosen access
    method per each row combination of previous tables. That is:

      rows_fetched = selectivity(access_condition) * cardinality(table)

    where 'access_condition' is whatever condition was chosen for
    index access, depending on the access method ('ref', 'range',
    etc.)

    @note For index/table scans, rows_fetched may be less than
    the number of rows in the table because the cost of evaluating
    constant conditions is included in the scan cost, and the number
    of rows produced by these scans is the estimated number of rows
    that pass the constant conditions. @see
    Optimize_table_order::calculate_scan_cost() . But this is only during
    planning; make_join_readinfo() simplifies it for EXPLAIN.
  */
  double rows_fetched;

  /**
    Cost of accessing the table in course of the entire complete join
    execution, i.e. cost of one access method use (e.g. 'range' or
    'ref' scan ) multiplied by estimated number of rows from tables
    earlier in the join sequence.

    read_cost does NOT include cost of processing rows within the
    executor (row_evaluate_cost).
  */
  double read_cost;

  /**
    The fraction of the 'rows_fetched' rows that will pass the table
    conditions that were NOT used by the access method. If, e.g.,

      "SELECT ... WHERE t1.colx = 4 and t1.coly @> 5"

    is resolved by ref access on t1.colx, filter_effect will be the
    fraction of rows that will pass the "t1.coly @> 5" predicate. The
    valid range is 0..1, where 0.0 means that no rows will pass the
    table conditions and 1.0 means that all rows will pass.

    It is used to calculate how many row combinations will be joined
    with the next table, @see prefix_rowcount below.

    @note With condition filtering enabled, it is possible to get
    a fanout = rows_fetched * filter_effect that is less than 1.0.
    Consider, e.g., a join between t1 and t2:

       "SELECT ... WHERE t1.col1=t2.colx and t2.coly OP @<something@>"

    where t1 is a prefix table and the optimizer currently calculates
    the cost of adding t2 to the join. Assume that the chosen access
    method on t2 is a 'ref' access on 'colx' that is estimated to
    produce 2 rows per row from t1 (i.e., rows_fetched = 2). It will
    in this case be perfectly fine to calculate a filtering effect
    @<0.5 (resulting in "rows_fetched * filter_effect @< 1.0") from the
    predicate "t2.coly OP @<something@>". If so, the number of row
    combinations from (t1,t2) is lower than the prefix_rowcount of t1.

    The above is just an example of how the fanout of a table can
    become less than one. It can happen for any access method.
  */
  float filter_effect;

  /**
    prefix_rowcount and prefix_cost form a stack of partial join
    order costs and output sizes

    prefix_rowcount: The number of row combinations that will be
    joined to the next table in the join sequence.

    For a joined table it is calculated as
      prefix_rowcount =
          last_table.prefix_rowcount * rows_fetched * filter_effect

    @see filter_effect

    For a semijoined table it may be less than this formula due to
    duplicate elimination.
  */
  double prefix_rowcount;
  double prefix_cost;

  JOIN_TAB *table;

  /**
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  Key_use *key;

  /** If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;
  bool use_join_buffer;

  /**
    Current optimization state: Semi-join strategy to be used for this
    and preceding join tables.

    Join optimizer sets this for the *last* join_tab in the
    duplicate-generating range. That is, in order to interpret this field,
    one needs to traverse join->[best_]positions array from right to left.
    When you see a join table with sj_strategy!= SJ_OPT_NONE, some other
    field (depending on the strategy) tells how many preceding positions
    this applies to. The values of covered_preceding_positions->sj_strategy
    must be ignored.
  */
  uint sj_strategy;
  /**
    Valid only after fix_semijoin_strategies_for_picked_join_order() call:
    if sj_strategy!=SJ_OPT_NONE, this is the number of subsequent tables that
    are covered by the specified semi-join strategy
  */
  uint n_sj_tables;

  /**
    Bitmap of semi-join inner tables that are in the join prefix and for
    which there's no provision yet for how to eliminate semi-join duplicates
    which they produce.
  */
  table_map dups_producing_tables;

  /* LooseScan strategy members */

  /* The first (i.e. driving) table we're doing loose scan for */
  uint first_loosescan_table;
  /*
     Tables that need to be in the prefix before we can calculate the cost
     of using LooseScan strategy.
  */
  table_map loosescan_need_tables;

  /*
    keyno  -  Planning to do LooseScan on this key. If keyuse is NULL then
              this is a full index scan, otherwise this is a ref+loosescan
              scan (and keyno matches the KEUSE's)
    MAX_KEY - Not doing a LooseScan
  */
  uint loosescan_key;   // final (one for strategy instance )
  uint loosescan_parts; /* Number of keyparts to be kept distinct */

  /* FirstMatch strategy */
  /*
    Index of the first inner table that we intend to handle with this
    strategy
  */
  uint first_firstmatch_table;
  /*
    Tables that were not in the join prefix when we've started considering
    FirstMatch strategy.
  */
  table_map first_firstmatch_rtbl;
  /*
    Tables that need to be in the prefix before we can calculate the cost
    of using FirstMatch strategy.
   */
  table_map firstmatch_need_tables;

  /* Duplicate Weedout strategy */
  /* The first table that the strategy will need to handle */
  uint first_dupsweedout_table;
  /*
    Tables that we will need to have in the prefix to do the weedout step
    (all inner and all outer that the involved semi-joins are correlated with)
  */
  table_map dupsweedout_tables;

  /* SJ-Materialization-Scan strategy */
  /* The last inner table (valid once we're after it) */
  uint sjm_scan_last_inner;
  /*
    Tables that we need to have in the prefix to calculate the correct cost.
    Basically, we need all inner tables and outer tables mentioned in the
    semi-join's ON expression so we can correctly account for fanout.
  */
  table_map sjm_scan_need_tables;

  /**
     Even if the query has no semijoin, two sj-related members are read and
     must thus have been set, by this function.
  */
  void no_semijoin() {
    sj_strategy = SJ_OPT_NONE;
    dups_producing_tables = 0;
  }
  /**
    Set complete estimated cost and produced rowcount for the prefix of tables
    up to and including this table, in the join plan.

    @param cost     Estimated cost
    @param rowcount Estimated row count
  */
  void set_prefix_cost(double cost, double rowcount) {
    prefix_cost = cost;
    prefix_rowcount = rowcount;
  }
  /**
    Set complete estimated cost and produced rowcount for the prefix of tables
    up to and including this table, calculated from the cost of the previous
    stage, the fanout of the current stage and the cost to process a row at
    the current stage.

    @param idx      Index of position object within array, if zero there is no
                    "previous" stage that can be added.
    @param cm       Cost model that provides the actual calculation
  */
  void set_prefix_join_cost(uint idx, const Cost_model_server *cm) {
    if (idx == 0) {
      prefix_rowcount = rows_fetched;
      prefix_cost = read_cost + cm->row_evaluate_cost(prefix_rowcount);
    } else {
      prefix_rowcount = (this - 1)->prefix_rowcount * rows_fetched;
      prefix_cost = (this - 1)->prefix_cost + read_cost +
                    cm->row_evaluate_cost(prefix_rowcount);
    }
    prefix_rowcount *= filter_effect;
  }
};

/**
   Use this in a function which depends on best_ref listing tables in the
   final join order. If 'tables==0', one is not expected to consult best_ref
   cells, and best_ref may not even have been allocated.
*/
#define ASSERT_BEST_REF_IN_JOIN_ORDER(join)                                \
  do {                                                                     \
    DBUG_ASSERT(join->tables == 0 || (join->best_ref && !join->join_tab)); \
  } while (0)

/**
  Query optimization plan node.

  Specifies:

  - a table access operation on the table specified by this node, and

  - a join between the result of the set of previous plan nodes and
    this plan node.
*/
class JOIN_TAB : public QEP_shared_owner {
 public:
  JOIN_TAB();

  void set_table(TABLE *t) {
    if (t) t->reginfo.join_tab = this;
    m_qs->set_table(t);
  }

  /// Sets the pointer to the join condition of TABLE_LIST
  void init_join_cond_ref(TABLE_LIST *tl) {
    m_join_cond_ref = tl->join_cond_optim_ref();
  }

  /// @returns join condition
  Item *join_cond() const { return *m_join_cond_ref; }

  /**
     Sets join condition
     @note this also changes TABLE_LIST::m_join_cond.
  */
  void set_join_cond(Item *cond) { *m_join_cond_ref = cond; }

  /// Set the combined condition for a table (may be performed several times)
  void set_condition(Item *to) {
    if (condition() != to) {
      m_qs->set_condition(to);
      // Condition changes, so some indexes may become usable or not:
      quick_order_tested.clear_all();
    }
  }

  uint use_join_cache() const { return m_use_join_cache; }
  void set_use_join_cache(uint u) { m_use_join_cache = u; }
  Key_use *keyuse() const { return m_keyuse; }
  void set_keyuse(Key_use *k) { m_keyuse = k; }

  TABLE_LIST *table_ref; /**< points to table reference               */

 private:
  Key_use *m_keyuse; /**< pointer to first used key               */

  /**
    Pointer to the associated join condition:

    - if this is a table with position==NULL (e.g. internal sort/group
      temporary table), pointer is NULL

    - otherwise, pointer is the address of some TABLE_LIST::m_join_cond.
      Thus, the pointee is the same as TABLE_LIST::m_join_cond (changing one
      changes the other; thus, optimizations made on the second are reflected
      in SELECT_LEX::print_table_array() which uses the first one).
  */
  Item **m_join_cond_ref;

 public:
  COND_EQUAL *cond_equal; /**< multiple equalities for the on expression*/

  /**
    The maximum value for the cost of seek operations for key lookup
    during ref access. The cost model for ref access assumes every key
    lookup will cause reading a block from disk. With many key lookups
    into the same table, most of the blocks will soon be in a memory
    buffer. As a consequence, there will in most cases be an upper
    limit on the number of actual disk accesses the ref access will
    cause. This variable is used for storing a maximum cost estimate
    for the disk accesses for ref access. It is used for limiting the
    cost estimate for ref access to a more realistic value than
    assuming every key lookup causes a random disk access. Without
    having this upper limit for the cost of ref access, table scan
    would be more likely to be chosen for cases where ref access
    performs better.
  */
  double worst_seeks;
  /** Keys with constant part. Subset of keys. */
  Key_map const_keys;
  Key_map checked_keys; /**< Keys checked */
  Key_map needed_reg;

  /**
    Used to avoid repeated range analysis for the same key in
    test_if_skip_sort_order(). This would otherwise happen if the best
    range access plan found for a key is turned down.
    quick_order_tested is cleared every time the select condition for
    this JOIN_TAB changes since a new condition may give another plan
    and cost from range analysis.
   */
  Key_map quick_order_tested;

  /*
    Number of records that will be scanned (yes scanned, not returned) by the
    best 'independent' access method, i.e. table scan or QUICK_*_SELECT)
  */
  ha_rows found_records;
  /*
    Cost of accessing the table using "ALL" or range/index_merge access
    method (but not 'index' for some reason), i.e. this matches method which
    E(#records) is in found_records.
  */
  double read_time;
  /**
    The set of tables that this table depends on. Used for outer join and
    straight join dependencies.
  */
  table_map dependent;
  /**
    The set of tables that are referenced by key from this table.
  */
  table_map key_dependent;

 public:
  uint used_fieldlength;
  enum quick_type use_quick;

  /**
    Join buffering strategy.
    After optimization it contains chosen join buffering strategy (if any).
  */
  uint m_use_join_cache;

  /* SemiJoinDuplicateElimination variables: */
  /*
    Embedding SJ-nest (may be not the direct parent), or NULL if none.
    This variable holds the result of table pullout.
  */
  TABLE_LIST *emb_sj_nest;

  /* NestedOuterJoins: Bitmap of nested joins this table is part of */
  nested_join_map embedding_map;

  /** Flags from SE's MRR implementation, to be used by JOIN_CACHE */
  uint join_cache_flags;

  /** true <=> AM will scan backward */
  bool reversed_access;

  /** Clean up associated table after query execution, including resources */
  void cleanup();

  /// @returns semijoin strategy for this table.
  uint get_sj_strategy() const;

 private:
  JOIN_TAB(const JOIN_TAB &);             // not defined
  JOIN_TAB &operator=(const JOIN_TAB &);  // not defined
};

inline JOIN_TAB::JOIN_TAB()
    : QEP_shared_owner(),
      table_ref(NULL),
      m_keyuse(NULL),
      m_join_cond_ref(NULL),
      cond_equal(NULL),
      worst_seeks(0.0),
      const_keys(),
      checked_keys(),
      needed_reg(),
      quick_order_tested(),
      found_records(0),
      read_time(0),
      dependent(0),
      key_dependent(0),
      used_fieldlength(0),
      use_quick(QS_NONE),
      m_use_join_cache(0),
      emb_sj_nest(NULL),
      embedding_map(0),
      join_cache_flags(0),
      reversed_access(false) {}

/**
  "Less than" comparison function object used to compare two JOIN_TAB
  objects based on a number of factors in this order:

   - table before another table that depends on it (straight join,
     outer join etc), then
   - table before another table that depends on it to use a key
     as access method, then
   - table with smallest number of records first, then
   - the table with lowest-value pointer (i.e., the one located
     in the lowest memory address) first.

  @param jt1  first JOIN_TAB object
  @param jt2  second JOIN_TAB object

  @note The order relation implemented by Join_tab_compare_default is not
    transitive, i.e. it is possible to choose a, b and c such that
    (a @< b) && (b @< c) but (c @< a). This is the case in the
    following example:

      a: dependent = @<none@> found_records = 3
      b: dependent = @<none@> found_records = 4
      c: dependent = b        found_records = 2

        a @< b: because a has fewer records
        b @< c: because c depends on b (e.g outer join dependency)
        c @< a: because c has fewer records

    This implies that the result of a sort using the relation
    implemented by Join_tab_compare_default () depends on the order in
    which elements are compared, i.e. the result is
    implementation-specific.

  @return
    true if jt1 is smaller than jt2, false otherwise
*/
class Join_tab_compare_default
    : public std::binary_function<const JOIN_TAB *, const JOIN_TAB *, bool> {
 public:
  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2) {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    if (jt1->dependent & jt2->table_ref->map()) return false;
    if (jt2->dependent & jt1->table_ref->map()) return true;

    const bool jt1_keydep_jt2 = jt1->key_dependent & jt2->table_ref->map();
    const bool jt2_keydep_jt1 = jt2->key_dependent & jt1->table_ref->map();

    if (jt1_keydep_jt2 && !jt2_keydep_jt1) return false;
    if (jt2_keydep_jt1 && !jt1_keydep_jt2) return true;

    if (jt1->found_records > jt2->found_records) return false;
    if (jt1->found_records < jt2->found_records) return true;

    return jt1 < jt2;
  }
};

/**
  "Less than" comparison function object used to compare two JOIN_TAB
  objects that are joined using STRAIGHT JOIN. For STRAIGHT JOINs,
  the join order is dictated by the relative order of the tables in the
  query which is reflected in JOIN_TAB::dependent. Table size and key
  dependencies are ignored here.
*/
class Join_tab_compare_straight
    : public std::binary_function<const JOIN_TAB *, const JOIN_TAB *, bool> {
 public:
  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2) {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    /*
      We don't do subquery flattening if the parent or child select has
      STRAIGHT_JOIN modifier. It is complicated to implement and the semantics
      is hardly useful.
    */
    DBUG_ASSERT(!jt1->emb_sj_nest);
    DBUG_ASSERT(!jt2->emb_sj_nest);

    if (jt1->dependent & jt2->table_ref->map()) return false;
    if (jt2->dependent & jt1->table_ref->map()) return true;

    return jt1 < jt2;
  }
};

/*
  Same as Join_tab_compare_default but tables from within the given
  semi-join nest go first. Used when optimizing semi-join
  materialization nests.
*/
class Join_tab_compare_embedded_first
    : public std::binary_function<const JOIN_TAB *, const JOIN_TAB *, bool> {
 private:
  const TABLE_LIST *emb_nest;

 public:
  Join_tab_compare_embedded_first(const TABLE_LIST *nest) : emb_nest(nest) {}

  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2) {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    if (jt1->emb_sj_nest == emb_nest && jt2->emb_sj_nest != emb_nest)
      return true;
    if (jt1->emb_sj_nest != emb_nest && jt2->emb_sj_nest == emb_nest)
      return false;

    Join_tab_compare_default cmp;
    return cmp(jt1, jt2);
  }
};

/* Extern functions in sql_select.cc */
void count_field_types(SELECT_LEX *select_lex, Temp_table_param *param,
                       List<Item> &fields, bool reset_with_sum_func,
                       bool save_sum_fields);
uint find_shortest_key(TABLE *table, const Key_map *usable_keys);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(THD *thd, TABLE_LIST *tables, List<Item> &all_fields,
                  Item *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(const void *arg, const void *a, const void *b);

/** class to copying an field/item to a key struct */

class store_key {
 public:
  bool null_key; /* true <=> the value of the key has a null part */
  enum store_key_result { STORE_KEY_OK, STORE_KEY_FATAL, STORE_KEY_CONV };
  store_key(THD *thd, Field *field_arg, uchar *ptr, uchar *null, uint length)
      : null_key(0), null_ptr(null), err(0) {
    if (field_arg->type() == MYSQL_TYPE_BLOB ||
        field_arg->type() == MYSQL_TYPE_GEOMETRY) {
      /*
        Key segments are always packed with a 2 byte length prefix.
        See mi_rkey for details.
      */
      to_field = new (*THR_MALLOC) Field_varstring(
          ptr, length, 2, null, 1, Field::NONE, field_arg->field_name,
          field_arg->table->s, field_arg->charset());
      to_field->init(field_arg->table);
    } else
      to_field = field_arg->new_key_field(thd->mem_root, field_arg->table, ptr,
                                          null, 1);
  }
  virtual ~store_key() {} /** Not actually needed */
  virtual const char *name() const = 0;

  /**
    @brief sets ignore truncation warnings mode and calls the real copy method

    @details this function makes sure truncation warnings when preparing the
    key buffers don't end up as errors (because of an enclosing INSERT/UPDATE).
  */
  enum store_key_result copy() {
    enum store_key_result result;
    THD *thd = to_field->table->in_use;
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

 protected:
  Field *to_field;  // Store data here
  uchar *null_ptr;
  uchar err;

  virtual enum store_key_result copy_inner() = 0;
};

static store_key::store_key_result type_conversion_status_to_store_key(
    type_conversion_status ts) {
  switch (ts) {
    case TYPE_OK:
      return store_key::STORE_KEY_OK;
    case TYPE_NOTE_TRUNCATED:
    case TYPE_WARN_TRUNCATED:
    case TYPE_NOTE_TIME_TRUNCATED:
      return store_key::STORE_KEY_CONV;
    case TYPE_WARN_OUT_OF_RANGE:
    case TYPE_WARN_INVALID_STRING:
    case TYPE_ERR_NULL_CONSTRAINT_VIOLATION:
    case TYPE_ERR_BAD_VALUE:
    case TYPE_ERR_OOM:
      return store_key::STORE_KEY_FATAL;
    default:
      DBUG_ASSERT(false);  // not possible
  }

  return store_key::STORE_KEY_FATAL;
}

class store_key_field : public store_key {
  Copy_field copy_field;
  const char *field_name;

 public:
  store_key_field(THD *thd, Field *to_field_arg, uchar *ptr,
                  uchar *null_ptr_arg, uint length, Field *from_field,
                  const char *name_arg)
      : store_key(thd, to_field_arg, ptr,
                  null_ptr_arg ? null_ptr_arg
                               : from_field->maybe_null() ? &err : (uchar *)0,
                  length),
        field_name(name_arg) {
    if (to_field) {
      copy_field.set(to_field, from_field, 0);
    }
  }
  const char *name() const { return field_name; }

 protected:
  enum store_key_result copy_inner() {
    TABLE *table = copy_field.to_field()->table;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
    copy_field.invoke_do_copy(&copy_field);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key = to_field->is_null();
    return err != 0 ? STORE_KEY_FATAL : STORE_KEY_OK;
  }
};

class store_key_item : public store_key {
 protected:
  Item *item;

 public:
  store_key_item(THD *thd, Field *to_field_arg, uchar *ptr, uchar *null_ptr_arg,
                 uint length, Item *item_arg)
      : store_key(thd, to_field_arg, ptr,
                  null_ptr_arg ? null_ptr_arg
                               : item_arg->maybe_null ? &err : (uchar *)0,
                  length),
        item(item_arg) {}
  const char *name() const { return "func"; }

 protected:
  enum store_key_result copy_inner() {
    TABLE *table = to_field->table;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
    type_conversion_status save_res = item->save_in_field(to_field, true);
    store_key_result res;
    /*
     Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
     we need to check for errors executing it and react accordingly
    */
    if (save_res != TYPE_OK && table->in_use->is_error())
      res = STORE_KEY_FATAL;
    else
      res = type_conversion_status_to_store_key(save_res);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key = to_field->is_null() || item->null_value;
    return (err != 0) ? STORE_KEY_FATAL : res;
  }
};

/*
  Class used for unique constraint implementation by subselect_hash_sj_engine.
  It uses store_key_item implementation to do actual copying, but after
  that, copy_inner calculates hash of each key part for unique constraint.
*/

class store_key_hash_item : public store_key_item {
 protected:
  Item *item;
  ulonglong *hash;

 public:
  store_key_hash_item(THD *thd, Field *to_field_arg, uchar *ptr,
                      uchar *null_ptr_arg, uint length, Item *item_arg,
                      ulonglong *hash_arg)
      : store_key_item(thd, to_field_arg, ptr, null_ptr_arg, length, item_arg),
        hash(hash_arg) {}
  const char *name() const { return "func"; }

 protected:
  enum store_key_result copy_inner();
};

class store_key_const_item : public store_key_item {
  bool inited;

 public:
  store_key_const_item(THD *thd, Field *to_field_arg, uchar *ptr,
                       uchar *null_ptr_arg, uint length, Item *item_arg)
      : store_key_item(thd, to_field_arg, ptr, null_ptr_arg, length, item_arg),
        inited(0) {}
  static const char static_name[];  ///< used out of this class
  const char *name() const { return static_name; }

 protected:
  enum store_key_result copy_inner() {
    if (!inited) {
      inited = 1;
      store_key_result res = store_key_item::copy_inner();
      if (res && !err) err = res;
    }
    return (err > 2 ? STORE_KEY_FATAL : (store_key_result)err);
  }
};

bool error_if_full_join(JOIN *join);
bool handle_query(THD *thd, LEX *lex, Query_result *result,
                  ulonglong added_options, ulonglong removed_options);

// Statement timeout function(s)
bool set_statement_timer(THD *thd);
void reset_statement_timer(THD *thd);

void free_underlaid_joins(SELECT_LEX *select);

void calc_used_field_length(TABLE *table, bool keep_current_rowid,
                            uint *p_used_fields, uint *p_used_fieldlength,
                            uint *p_used_blobs, bool *p_used_null_fields,
                            bool *p_used_uneven_bit_fields);

ORDER *simple_remove_const(ORDER *order, Item *where);
bool const_expression_in_where(Item *cond, Item *comp_item,
                               Field *comp_field = NULL,
                               Item **const_item = NULL);
bool test_if_subpart(ORDER *a, ORDER *b);
void calc_group_buffer(JOIN *join, ORDER *group);
bool make_join_readinfo(JOIN *join, uint no_jbuf_after);
bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
                        table_map used_tables);
bool types_allow_materialization(Item *outer, Item *inner);
bool and_conditions(Item **e1, Item *e2);

/**
  Create a AND item of two existing items.
  A new Item_cond_and item is created with the two supplied items as
  arguments.

  @note About handling of null pointers as arguments: if the first
  argument is a null pointer, then the item given as second argument is
  returned (no new Item_cond_and item is created). The second argument
  must not be a null pointer.

  @param cond  the first argument to the new AND condition
  @param item  the second argument to the new AND condtion

  @return the new AND item
*/
static inline Item *and_items(Item *cond, Item *item) {
  DBUG_ASSERT(item != NULL);
  return (cond ? (new Item_cond_and(cond, item)) : item);
}

/// A variant of the above, guaranteed to return Item_bool_func.
static inline Item_bool_func *and_items(Item *cond, Item_bool_func *item) {
  DBUG_ASSERT(item != NULL);
  return (cond ? (new Item_cond_and(cond, item)) : item);
}

uint actual_key_parts(const KEY *key_info);

class ORDER_with_src;

uint get_index_for_order(ORDER_with_src *order, QEP_TAB *tab, ha_rows limit,
                         bool *need_sort, bool *reverse);
int test_if_order_by_key(ORDER_with_src *order, TABLE *table, uint idx,
                         uint *used_key_parts, bool *skip_quick);
bool test_if_cheaper_ordering(const JOIN_TAB *tab, ORDER_with_src *order,
                              TABLE *table, Key_map usable_keys, int key,
                              ha_rows select_limit, int *new_key,
                              int *new_key_direction, ha_rows *new_select_limit,
                              uint *new_used_key_parts = NULL,
                              uint *saved_best_key_parts = NULL);
/**
  Calculate properties of ref key: key length, number of used key parts,
  dependency map, possibility of null.

  @param keyuse               Array of keys to consider
  @param tab                  join_tab to calculate ref parameters for
  @param key                  number of the key to use
  @param used_tables          tables read prior to this table
  @param [out] chosen_keyuses when given, this function will fill array with
                              chosen keyuses
  @param [out] length_out     calculated length of the ref
  @param [out] keyparts_out   calculated number of used keyparts
  @param [out] dep_map        when given, calculated dependency map
  @param [out] maybe_null     when given, calculated maybe_null property
*/

void calc_length_and_keyparts(Key_use *keyuse, JOIN_TAB *tab, const uint key,
                              table_map used_tables, Key_use **chosen_keyuses,
                              uint *length_out, uint *keyparts_out,
                              table_map *dep_map, bool *maybe_null);

#endif /* SQL_SELECT_INCLUDED */

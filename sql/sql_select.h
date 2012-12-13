#ifndef SQL_SELECT_INCLUDED
#define SQL_SELECT_INCLUDED

/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  classes to use when handling where clause
*/

#include "procedure.h"
#include <myisam.h>
#include "sql_array.h"                        /* Array */
#include "records.h"                          /* READ_RECORD */
#include "opt_range.h"                /* SQL_SELECT, QUICK_SELECT_I */

#include "mem_root_array.h"
#include "sql_executor.h"
#include "opt_explain_format.h" // for Extra_tag

#include <functional>
/**
   Returns a constant of type 'type' with the 'A' lowest-weight bits set.
   Example: LOWER_BITS(uint, 3) == 7.
   Requirement: A < sizeof(type) * 8.
*/
#define LOWER_BITS(type,A)	((type) (((type) 1 << (A)) -1))

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2
#define FT_KEYPART                      (MAX_REF_PARTS+10)

/**
  Information about usage of an index to satisfy an equality condition.
*/
class Key_use {
public:
  // We need the default constructor for unit testing.
  Key_use()
    : table(NULL),
      val(NULL),
      used_tables(0),
      key(0),
      keypart(0),
      optimize(0),
      keypart_map(0),
      ref_table_rows(0),
      null_rejecting(false),
      cond_guard(NULL),
      sj_pred_no(UINT_MAX)
  {}

  Key_use(TABLE *table_arg, Item *val_arg, table_map used_tables_arg,
          uint key_arg, uint keypart_arg, uint optimize_arg,
          key_part_map keypart_map_arg, ha_rows ref_table_rows_arg,
          bool null_rejecting_arg, bool *cond_guard_arg,
          uint sj_pred_no_arg) :
  table(table_arg), val(val_arg), used_tables(used_tables_arg),
  key(key_arg), keypart(keypart_arg), optimize(optimize_arg),
  keypart_map(keypart_map_arg), ref_table_rows(ref_table_rows_arg),
  null_rejecting(null_rejecting_arg), cond_guard(cond_guard_arg),
  sj_pred_no(sj_pred_no_arg)
  {}
  TABLE *table;            ///< table owning the index
  Item	*val;              ///< other side of the equality, or value if no field
  table_map used_tables;   ///< tables used on other side of equality
  uint key;                ///< number of index
  uint keypart;            ///< used part of the index
  uint optimize;           ///< 0, or KEY_OPTIMIZE_*
  key_part_map keypart_map;       ///< like keypart, but as a bitmap
  ha_rows      ref_table_rows;    ///< Estimate of how many rows for a key value
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
              *cond_guard == TRUE <=> equality condition is on
              *cond_guard == FALSE <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)

    Not used if the index is fulltext (such index cannot be used for
    equalities).
  */
  bool *cond_guard;
  /**
     0..63    <=> This was created from semi-join IN-equality # sj_pred_no.
     UINT_MAX  Otherwise

     Not used if the index is fulltext (such index cannot be used for
     semijoin).
  */
  uint         sj_pred_no;
};


// Key_use has a trivial destructor, no need to run it from Mem_root_array.
typedef Mem_root_array<Key_use, true> Key_use_array;

class store_key;

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
                   Full table scan or range scan.
                   If select->quick != NULL, it is range access.
                   Otherwise it is table scan.
                 */
                 JT_ALL,
                 /*
                   Range scan. Note that range scan is not indicated
                   by JT_RANGE but by "JT_ALL + select->quick" except
                   when printing EXPLAIN output. @see calc_join_type()
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

class JOIN;

/* Values for JOIN_TAB::packed_info */
#define TAB_INFO_HAVE_VALUE 1
#define TAB_INFO_USING_INDEX 2
#define TAB_INFO_USING_WHERE 4
#define TAB_INFO_FULL_SCAN_ON_NULL 8

class JOIN_CACHE;
class SJ_TMP_TABLE;

#define SJ_OPT_NONE 0
#define SJ_OPT_DUPS_WEEDOUT 1
#define SJ_OPT_LOOSE_SCAN   2
#define SJ_OPT_FIRST_MATCH  3
#define SJ_OPT_MATERIALIZE_LOOKUP  4
#define SJ_OPT_MATERIALIZE_SCAN  5

inline bool sj_is_materialize_strategy(uint strategy)
{
  return strategy >= SJ_OPT_MATERIALIZE_LOOKUP;
}

/** 
    Bits describing quick select type
*/
enum quick_type { QS_NONE, QS_RANGE, QS_DYNAMIC_RANGE};


/**
  A position of table within a join order. This structure is primarily used
  as a part of join->positions and join->best_positions arrays.

  One POSITION element contains information about:
   - Which table is accessed
   - Which access method was chosen
      = Its cost and #of output records
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
     maintain current state in join->positions[#tables_in_prefix]. See
     advance_sj_state() for details.

  This class has to stay a POD, because it is memcpy'd in many places.
*/

typedef struct st_position : public Sql_alloc
{
  /*
    The "fanout" -  number of output rows that will be produced (after
    pushed down selection condition is applied) per each row combination of
    previous tables.
  */
  double records_read;

  /* 
    Cost accessing the table in course of the entire complete join execution,
    i.e. cost of one access method use (e.g. 'range' or 'ref' scan ) times 
    number the access method will be invoked.
  */
  double read_time;
  JOIN_TAB *table;

  /*
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  Key_use *key;

  /* If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;
  bool use_join_buffer; 
  
  
  /* These form a stack of partial join order costs and output sizes */
  Cost_estimate prefix_cost;
  double    prefix_record_count;

  /*
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
  /*
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
  uint        first_loosescan_table;
  /* 
     Tables that need to be in the prefix before we can calculate the cost
     of using LooseScan strategy.
  */
  table_map   loosescan_need_tables;

  /*
    keyno  -  Planning to do LooseScan on this key. If keyuse is NULL then 
              this is a full index scan, otherwise this is a ref+loosescan
              scan (and keyno matches the KEUSE's)
    MAX_KEY - Not doing a LooseScan
  */
  uint loosescan_key;  // final (one for strategy instance )
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
  uint  first_dupsweedout_table;
  /*
    Tables that we will need to have in the prefix to do the weedout step
    (all inner and all outer that the involved semi-joins are correlated with)
  */
  table_map dupsweedout_tables;

/* SJ-Materialization-Scan strategy */
  /* The last inner table (valid once we're after it) */
  uint      sjm_scan_last_inner;
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
  void no_semijoin()
  {
    sj_strategy= SJ_OPT_NONE;
    dups_producing_tables= 0;
  }
  void set_prefix_costs(double read_time_arg, double row_count_arg)
  {
    prefix_cost.reset();
    prefix_cost.add_io(read_time_arg);
    prefix_record_count= row_count_arg;
  }
} POSITION;


struct st_cache_field;
class QEP_operation;
class Filesort;

typedef struct st_join_table : public Sql_alloc
{
  st_join_table();

  table_map prefix_tables() const { return prefix_tables_map; }

  table_map added_tables() const { return added_tables_map; }

  /**
    Set available tables for a table in a join plan.

    @param prefix_tables: Set of tables available for this plan
    @param prev_tables: Set of tables available for previous table, used to
                        calculate set of tables added for this table.
  */
  void set_prefix_tables(table_map prefix_tables, table_map prev_tables)
  {
    prefix_tables_map= prefix_tables;
    added_tables_map= prefix_tables & ~prev_tables;
  }

  /**
    Add an available set of tables for a table in a join plan.

    @param tables: Set of tables added for this table in plan.
  */
  void add_prefix_tables(table_map tables)
  { prefix_tables_map|= tables; added_tables_map|= tables; }

  /// Return true if join_tab should perform a FirstMatch action
  bool do_firstmatch() const { return firstmatch_return; }

  /// Return true if join_tab should perform a LooseScan action
  bool do_loosescan() const { return loosescan_key_len; }

  /// Return true if join_tab starts a Duplicate Weedout action
  bool starts_weedout() const { return flush_weedout_table; }

  /// Return true if join_tab finishes a Duplicate Weedout action
  bool finishes_weedout() const { return check_weed_out_table; }

  TABLE         *table;
  POSITION      *position;      /**< points into best_positions array        */
  Key_use       *keyuse;        /**< pointer to first used key               */
  SQL_SELECT    *select;
private:
  Item          *m_condition;   /**< condition for this join_tab             */
public:
  QUICK_SELECT_I *quick;
  Item         **on_expr_ref;   /**< pointer to the associated on expression */
  COND_EQUAL    *cond_equal;    /**< multiple equalities for the on expression*/
  st_join_table *first_inner;   /**< first inner table for including outerjoin*/
  bool           found;         /**< true after all matches or null complement*/
  bool           not_null_compl;/**< true before null complement is added    */
  /// For a materializable derived or SJ table: true if has been materialized
  bool           materialized;
  st_join_table *last_inner;    /**< last table table for embedding outer join*/
  st_join_table *first_upper;  /**< first inner table for embedding outer join*/
  st_join_table *first_unmatched; /**< used for optimization purposes only   */
  /* 
    The value of m_condition before we've attempted to do Index Condition
    Pushdown. We may need to restore everything back if we first choose one
    index but then reconsider (see test_if_skip_sort_order() for such
    scenarios).
    NULL means no index condition pushdown was performed.
  */
  Item          *pre_idx_push_cond;
  
  /* Special content for EXPLAIN 'Extra' column or NULL if none */
  Extra_tag     info;
  /* 
    Bitmap of TAB_INFO_* bits that encodes special line for EXPLAIN 'Extra'
    column, or 0 if there is no info.
  */
  uint          packed_info;

  READ_RECORD::Setup_func materialize_table;
  /**
     Initialize table for reading and fetch the first row from the table. If
     table is a materialized derived one, function must materialize it with
     prepare_scan().
  */
  READ_RECORD::Setup_func read_first_record;
  Next_select_func next_select;
  READ_RECORD	read_record;
  /* 
    The following two fields are used for a [NOT] IN subquery if it is
    executed by an alternative full table scan when the left operand of
    the subquery predicate is evaluated to NULL.
  */  
  READ_RECORD::Setup_func save_read_first_record;/* to save read_first_record */
  READ_RECORD::Read_func save_read_record;/* to save read_record.read_record */
  /**
    Struct needed for materialization of semi-join. Set for a materialized
    temporary table, and NULL for all other join_tabs (except when
    materialization is in progress, @see join_materialize_semijoin()).
  */
  Semijoin_mat_exec *sj_mat_exec;          
  double	worst_seeks;
  key_map	const_keys;			/**< Keys with constant part */
  key_map	checked_keys;			/**< Keys checked */
  key_map	needed_reg;
  key_map       keys;                           /**< all keys with can be used */
  /**
    Used to avoid repeated range analysis for the same key in
    test_if_skip_sort_order(). This would otherwise happen if the best
    range access plan found for a key is turned down.
    quick_order_tested is cleared every time the select condition for
    this JOIN_TAB changes since a new condition may give another plan
    and cost from range analysis.
   */
  key_map       quick_order_tested;

  /* Either #rows in the table or 1 for const table.  */
  ha_rows	records;
  /*
    Number of records that will be scanned (yes scanned, not returned) by the
    best 'independent' access method, i.e. table scan or QUICK_*_SELECT)
  */
  ha_rows       found_records;
  /*
    Cost of accessing the table using "ALL" or range/index_merge access
    method (but not 'index' for some reason), i.e. this matches method which
    E(#records) is in found_records.
  */
  ha_rows       read_time;
  /**
    The set of tables that this table depends on. Used for outer join and
    straight join dependencies.
  */
  table_map     dependent;
  /**
    The set of tables that are referenced by key from this table.
  */
  table_map     key_dependent;
private:
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
public:
  uint		index;
  uint		used_fields,used_fieldlength,used_blobs;
  uint          used_null_fields;
  uint          used_rowid_fields;
  uint          used_uneven_bit_fields;
  enum quick_type use_quick;
  enum join_type type;
  bool          not_used_in_distinct;
  /* TRUE <=> index-based access method must return records in order */
  bool          sorted;
  /* 
    If it's not 0 the number stored this field indicates that the index
    scan has been chosen to access the table data and we expect to scan 
    this number of rows for the table.
  */ 
  ha_rows       limit; 
  TABLE_REF	ref;
  /**
    Join buffering strategy.
    After optimization it contains chosen join buffering strategy (if any).
   */
  uint          use_join_cache;
  QEP_operation *op;
  /*
    Index condition for BKA access join
  */
  Item          *cache_idx_cond;
  SQL_SELECT    *cache_select;
  JOIN		*join;

  /* SemiJoinDuplicateElimination variables: */
  /*
    Embedding SJ-nest (may be not the direct parent), or NULL if none.
    This variable holds the result of table pullout.
  */
  TABLE_LIST    *emb_sj_nest;

  /**
    Boundaries of semijoin inner tables around this table. Valid only once
    final QEP has been chosen. Depending on the strategy, they may define an
    interval (all tables inside are inner of a semijoin) or
    not. last_sj_inner_tab is not set for Duplicates Weedout.
  */
  struct st_join_table *first_sj_inner_tab;
  struct st_join_table *last_sj_inner_tab;

  /* Variables for semi-join duplicate elimination */
  SJ_TMP_TABLE  *flush_weedout_table;
  SJ_TMP_TABLE  *check_weed_out_table;
  
  /*
    If set, means we should stop join enumeration after we've got the first
    match and return to the specified join tab. May point to
    join->join_tab[-1] which means stop join execution after the first
    match.
  */
  struct st_join_table  *firstmatch_return;
 
  /*
    Length of key tuple (depends on #keyparts used) to store in loosescan_buf.
    If zero, means that loosescan is not used.
  */
  uint loosescan_key_len;

  /* Buffer to save index tuple to be able to skip duplicates */
  uchar *loosescan_buf;

  /* 
    If doing a LooseScan, this join tab is the first (i.e.  "driving") join
    tab, and match_tab points to the last join tab handled by the strategy.
    match_tab->found_match should be checked to see if the current value group
    had a match.
    If doing a FirstMatch, check this join tab to see if there is a match.
    Unless the FirstMatch performs a "split jump", this is equal to the
    current join_tab.
  */
  struct st_join_table *match_tab;
  /*
    Used by FirstMatch and LooseScan. TRUE <=> there is a matching
    record combination
  */
  bool found_match;
  
  /*
    Used by DuplicateElimination. tab->table->ref must have the rowid
    whenever we have a current record. copy_current_rowid needed because
    we cannot bind to the rowid buffer before the table has been opened.
  */
  int  keep_current_rowid;
  st_cache_field *copy_current_rowid;

  /* NestedOuterJoins: Bitmap of nested joins this table is part of */
  nested_join_map embedding_map;

  /* Tmp table info */
  TMP_TABLE_PARAM *tmp_table_param;

  /* Sorting related info */
  Filesort *filesort;

  /**
    List of topmost expressions in the select list. The *next* JOIN TAB
    in the plan should use it to obtain correct values. Same applicable to
    all_fields. These lists are needed because after tmp tables functions
    will be turned to fields. These variables are pointing to
    tmp_fields_list[123]. Valid only for tmp tables and the last non-tmp
    table in the query plan.
    @see JOIN::make_tmp_tables_info()
  */
  List<Item> *fields;
  /** List of all expressions in the select list */
  List<Item> *all_fields;
  /*
    Pointer to the ref array slice which to switch to before sending
    records. Valid only for tmp tables.
  */
  Ref_ptr_array *ref_array;

  /** Number of records saved in tmp table */
  ha_rows send_records;

  /** HAVING condition for checking prior saving a record into tmp table*/
  Item *having;

  /** TRUE <=> remove duplicates on this table. */
  bool distinct;

  /** Clean up associated table after query execution, including resources */
  void cleanup();
  inline bool is_using_loose_index_scan()
  {
    return (select && select->quick &&
            (select->quick->get_type() ==
             QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX));
  }
  bool is_using_agg_loose_index_scan ()
  {
    return (is_using_loose_index_scan() &&
            ((QUICK_GROUP_MIN_MAX_SELECT *)select->quick)->is_agg_distinct());
  }
  /* SemiJoinDuplicateElimination: reserve space for rowid */
  bool check_rowid_field()
  {
    if (keep_current_rowid && !used_rowid_fields)
    {
      used_rowid_fields= 1;
      used_fieldlength+= table->file->ref_length;
    }
    return test(used_rowid_fields);
  }
  bool is_inner_table_of_outer_join()
  {
    return first_inner != NULL;
  }
  bool is_single_inner_of_semi_join()
  {
    return first_sj_inner_tab == this && last_sj_inner_tab == this;
  }
  bool is_single_inner_of_outer_join()
  {
    return first_inner == this && first_inner->last_inner == this;
  }
  bool is_first_inner_for_outer_join()
  {
    return first_inner && first_inner == this;
  }
  Item *condition() const
  {
    return m_condition;
  }
  void set_condition(Item *to, uint line)
  {
    DBUG_PRINT("info", 
               ("JOIN_TAB::m_condition changes %p -> %p at line %u tab %p",
                m_condition, to, line, this));
    m_condition= to;
    quick_order_tested.clear_all();
  }

  Item *set_jt_and_sel_condition(Item *new_cond, uint line)
  {
    Item *tmp_cond= m_condition;
    set_condition(new_cond, line);
    if (select)
      select->cond= new_cond;
    return tmp_cond;
  }

  /// @returns semijoin strategy for this table.
  uint get_sj_strategy() const
  {
    if (first_sj_inner_tab == NULL)
      return SJ_OPT_NONE;
    DBUG_ASSERT(first_sj_inner_tab->position->sj_strategy != SJ_OPT_NONE);
    return first_sj_inner_tab->position->sj_strategy;
  }
  /**
     @returns query block id for an inner table of materialized semi-join, and
              0 for all other tables.
  */
  uint sjm_query_block_id() const;

  bool and_with_condition(Item *tmp_cond, uint line);
  bool and_with_jt_and_sel_condition(Item *tmp_cond, uint line);

  /**
    Check if there are triggered/guarded conditions that might be
    'switched off' by the subquery code when executing 'Full scan on
    NULL key' subqueries.

    @return true if there are guarded conditions, false otherwise
  */

  bool has_guarded_conds() const
  {
    return ref.has_guarded_conds();
  }
  bool prepare_scan();
  bool sort_table();
  bool remove_duplicates();
} JOIN_TAB;

inline
st_join_table::st_join_table()
  : table(NULL),
    position(NULL),
    keyuse(NULL),
    select(NULL),
    m_condition(NULL),
    quick(NULL),
    on_expr_ref(NULL),
    cond_equal(NULL),
    first_inner(NULL),
    found(false),
    not_null_compl(false),
    materialized(false),
    last_inner(NULL),
    first_upper(NULL),
    first_unmatched(NULL),
    pre_idx_push_cond(NULL),
    info(ET_none),
    packed_info(0),
    materialize_table(NULL),
    read_first_record(NULL),
    next_select(NULL),
    read_record(),
    save_read_first_record(NULL),
    save_read_record(NULL),
    sj_mat_exec(NULL),
    worst_seeks(0.0),
    const_keys(),
    checked_keys(),
    needed_reg(),
    keys(),
    quick_order_tested(),

    records(0),
    found_records(0),
    read_time(0),

    dependent(0),
    key_dependent(0),
    prefix_tables_map(0),
    added_tables_map(0),
    index(0),
    used_fields(0),
    used_fieldlength(0),
    used_blobs(0),
    used_null_fields(0),
    used_rowid_fields(0),
    used_uneven_bit_fields(0),
    use_quick(QS_NONE),
    type(JT_UNKNOWN),
    not_used_in_distinct(false),
    sorted(false),

    limit(0),
    ref(),
    use_join_cache(0),
    op(NULL),

    cache_idx_cond(NULL),
    cache_select(NULL),
    join(NULL),

    emb_sj_nest(NULL),
    first_sj_inner_tab(NULL),
    last_sj_inner_tab(NULL),

    flush_weedout_table(NULL),
    check_weed_out_table(NULL),
    firstmatch_return(NULL),
    loosescan_key_len(0),
    loosescan_buf(NULL),
    match_tab(NULL),
    found_match(FALSE),

    keep_current_rowid(0),
    copy_current_rowid(NULL),
    embedding_map(0),
    tmp_table_param(NULL),
    filesort(NULL),
    fields(NULL),
    all_fields(NULL),
    ref_array(NULL),
    send_records(0),
    having(NULL),
    distinct(false)
{
  /**
    @todo Add constructor to READ_RECORD.
    All users do init_read_record(), which does memset(),
    rather than invoking a constructor.
  */
  memset(&read_record, 0, sizeof(read_record));
}

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
    (a < b) && (b < c) but (c < a). This is the case in the
    following example: 

      a: dependent = <none>   found_records = 3
      b: dependent = <none>   found_records = 4
      c: dependent = b        found_records = 2

        a < b: because a has fewer records
        b < c: because c depends on b (e.g outer join dependency)
        c < a: because c has fewer records

    This implies that the result of a sort using the relation
    implemented by Join_tab_compare_default () depends on the order in
    which elements are compared, i.e. the result is
    implementation-specific.

  @return
    true if jt1 is smaller than jt2, false otherwise
*/
class Join_tab_compare_default :
  public std::binary_function<const JOIN_TAB*, const JOIN_TAB*, bool>
{
public:
  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2)
  {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    if (jt1->dependent & jt2->table->map)
      return false;
    if (jt2->dependent & jt1->table->map)
      return true;

    const bool jt1_keydep_jt2= jt1->key_dependent & jt2->table->map;
    const bool jt2_keydep_jt1= jt2->key_dependent & jt1->table->map;

    if (jt1_keydep_jt2 && !jt2_keydep_jt1)
      return false;
    if (jt2_keydep_jt1 && !jt1_keydep_jt2)
      return true;

    if (jt1->found_records > jt2->found_records)
      return false;
    if (jt1->found_records < jt2->found_records)
      return true;

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
class Join_tab_compare_straight :
  public std::binary_function<const JOIN_TAB*, const JOIN_TAB*, bool>
{
public:
  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2)
  {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    /*
      We don't do subquery flattening if the parent or child select has
      STRAIGHT_JOIN modifier. It is complicated to implement and the semantics
      is hardly useful.
    */
    DBUG_ASSERT(!jt1->emb_sj_nest);
    DBUG_ASSERT(!jt2->emb_sj_nest);

    if (jt1->dependent & jt2->table->map)
      return false;
    if (jt2->dependent & jt1->table->map)
      return true;

    return jt1 < jt2;
  }
};

/*
  Same as Join_tab_compare_default but tables from within the given
  semi-join nest go first. Used when optimizing semi-join
  materialization nests.
*/
class Join_tab_compare_embedded_first :
  public std::binary_function<const JOIN_TAB*, const JOIN_TAB*, bool>
{
private:
  const TABLE_LIST *emb_nest;
public:
  
  Join_tab_compare_embedded_first(const TABLE_LIST *nest) : emb_nest(nest){}

  bool operator()(const JOIN_TAB *jt1, const JOIN_TAB *jt2)
  {
    // Sorting distinct tables, so a table should not be compared with itself
    DBUG_ASSERT(jt1 != jt2);

    if (jt1->emb_sj_nest == emb_nest && jt2->emb_sj_nest != emb_nest)
      return true;
    if (jt1->emb_sj_nest != emb_nest && jt2->emb_sj_nest == emb_nest)
      return false;

    Join_tab_compare_default cmp;
    return cmp(jt1,jt2);
  }
};


typedef Bounds_checked_array<Item_null_result*> Item_null_array;

typedef struct st_select_check {
  uint const_ref,reg_ref;
} SELECT_CHECK;

/* Extern functions in sql_select.cc */
void count_field_types(SELECT_LEX *select_lex, TMP_TABLE_PARAM *param, 
                       List<Item> &fields, bool reset_with_sum_func,
                       bool save_sum_fields);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(THD* thd,
                  TABLE_LIST *tables, List<Item> &all_fields, Item *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(const void* arg, const void *a,const void *b);

/** class to copying an field/item to a key struct */

class store_key :public Sql_alloc
{
public:
  bool null_key; /* TRUE <=> the value of the key has a null part */
  enum store_key_result { STORE_KEY_OK, STORE_KEY_FATAL, STORE_KEY_CONV };
  store_key(THD *thd, Field *field_arg, uchar *ptr, uchar *null, uint length)
    :null_key(0), null_ptr(null), err(0)
  {
    if (field_arg->type() == MYSQL_TYPE_BLOB
        || field_arg->type() == MYSQL_TYPE_GEOMETRY)
    {
      /* 
        Key segments are always packed with a 2 byte length prefix.
        See mi_rkey for details.
      */
      to_field= new Field_varstring(ptr, length, 2, null, 1, 
                                    Field::NONE, field_arg->field_name,
                                    field_arg->table->s, field_arg->charset());
      to_field->init(field_arg->table);
    }
    else
      to_field=field_arg->new_key_field(thd->mem_root, field_arg->table,
                                        ptr, null, 1);
  }
  virtual ~store_key() {}			/** Not actually needed */
  virtual const char *name() const=0;

  /**
    @brief sets ignore truncation warnings mode and calls the real copy method

    @details this function makes sure truncation warnings when preparing the
    key buffers don't end up as errors (because of an enclosing INSERT/UPDATE).
  */
  enum store_key_result copy()
  {
    enum store_key_result result;
    THD *thd= to_field->table->in_use;
    enum_check_fields saved_count_cuted_fields= thd->count_cuted_fields;
    sql_mode_t sql_mode= thd->variables.sql_mode;
    thd->variables.sql_mode&= ~(MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE);

    thd->count_cuted_fields= CHECK_FIELD_IGNORE;

    result= copy_inner();

    thd->count_cuted_fields= saved_count_cuted_fields;
    thd->variables.sql_mode= sql_mode;

    return result;
  }

 protected:
  Field *to_field;				// Store data here
  uchar *null_ptr;
  uchar err;

  virtual enum store_key_result copy_inner()=0;
};


static store_key::store_key_result
type_conversion_status_to_store_key (type_conversion_status ts)
{
  switch (ts)
  {
  case TYPE_OK:
    return store_key::STORE_KEY_OK;
  case TYPE_NOTE_TIME_TRUNCATED:
    return store_key::STORE_KEY_CONV;
  case TYPE_WARN_OUT_OF_RANGE:
  case TYPE_NOTE_TRUNCATED:
  case TYPE_WARN_TRUNCATED:
  case TYPE_ERR_NULL_CONSTRAINT_VIOLATION:
  case TYPE_ERR_BAD_VALUE:
  case TYPE_ERR_OOM:
    return store_key::STORE_KEY_FATAL;
  default:
    DBUG_ASSERT(false); // not possible
  }

  return store_key::STORE_KEY_FATAL;
}

class store_key_field: public store_key
{
  Copy_field copy_field;
  const char *field_name;
 public:
  store_key_field(THD *thd, Field *to_field_arg, uchar *ptr,
                  uchar *null_ptr_arg,
		  uint length, Field *from_field, const char *name_arg)
    :store_key(thd, to_field_arg,ptr,
	       null_ptr_arg ? null_ptr_arg : from_field->maybe_null() ? &err
	       : (uchar*) 0, length), field_name(name_arg)
  {
    if (to_field)
    {
      copy_field.set(to_field,from_field,0);
    }
  }
  const char *name() const { return field_name; }

 protected: 
  enum store_key_result copy_inner()
  {
    TABLE *table= copy_field.to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);
    copy_field.do_copy(&copy_field);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key= to_field->is_null();
    return err != 0 ? STORE_KEY_FATAL : STORE_KEY_OK;
  }
};


class store_key_item :public store_key
{
 protected:
  Item *item;
public:
  store_key_item(THD *thd, Field *to_field_arg, uchar *ptr,
                 uchar *null_ptr_arg, uint length, Item *item_arg)
    :store_key(thd, to_field_arg, ptr,
	       null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
	       &err : (uchar*) 0, length), item(item_arg)
  {}
  const char *name() const { return "func"; }

 protected:  
  enum store_key_result copy_inner()
  {
    TABLE *table= to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);
    type_conversion_status save_res= item->save_in_field(to_field, true);
    store_key_result res;
    /*
     Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
     we need to check for errors executing it and react accordingly
    */
    if (save_res != TYPE_OK && table->in_use->is_error())
      res= STORE_KEY_FATAL;
    else
      res= type_conversion_status_to_store_key(save_res);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key= to_field->is_null() || item->null_value;
    return (err != 0) ? STORE_KEY_FATAL : res;
  }
};


class store_key_const_item :public store_key_item
{
  bool inited;
public:
  store_key_const_item(THD *thd, Field *to_field_arg, uchar *ptr,
		       uchar *null_ptr_arg, uint length,
		       Item *item_arg)
    :store_key_item(thd, to_field_arg, ptr,
                    null_ptr_arg, length, item_arg), inited(0)
  {
  }
  const char *name() const { return "const"; }

protected:  
  enum store_key_result copy_inner()
  {
    if (!inited)
    {
      inited=1;
      store_key_result res= store_key_item::copy_inner();
      if (res && !err)
        err= res;
    }
    return (err > 2 ? STORE_KEY_FATAL : (store_key_result) err);
  }
};

bool error_if_full_join(JOIN *join);
bool handle_select(THD *thd, select_result *result,
                   ulong setup_tables_done_option);
bool mysql_select(THD *thd,
                  TABLE_LIST *tables, uint wild_num,  List<Item> &list,
                  Item *conds, SQL_I_List<ORDER> *order,
                  SQL_I_List<ORDER> *group,
                  Item *having, ulonglong select_type, 
                  select_result *result, SELECT_LEX_UNIT *unit, 
                  SELECT_LEX *select_lex);
void free_underlaid_joins(THD *thd, SELECT_LEX *select);


void calc_used_field_length(THD *thd, JOIN_TAB *join_tab);

inline bool optimizer_flag(THD *thd, uint flag)
{ 
  return (thd->variables.optimizer_switch & flag);
}

uint get_index_for_order(ORDER *order, TABLE *table, SQL_SELECT *select,
                         ha_rows limit, bool *need_sort, bool *reverse);
ORDER *simple_remove_const(ORDER *order, Item *where);
bool const_expression_in_where(Item *cond, Item *comp_item,
                               Field *comp_field= NULL,
                               Item **const_item= NULL);
bool test_if_subpart(ORDER *a,ORDER *b);
void calc_group_buffer(JOIN *join,ORDER *group);
bool
test_if_skip_sort_order(JOIN_TAB *tab, ORDER *order, ha_rows select_limit,
                        const bool no_changes, const key_map *map,
                        const char *clause_type);
bool make_join_readinfo(JOIN *join, ulonglong options, uint no_jbuf_after);
bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
                        table_map used_tables);
bool types_allow_materialization(Item *outer, Item *inner);
bool and_conditions(Item **e1, Item *e2);

static inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}

uint actual_key_parts(KEY *key_info);
uint actual_key_flags(KEY *key_info);

#endif /* SQL_SELECT_INCLUDED */

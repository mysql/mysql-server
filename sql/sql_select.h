#ifndef SQL_SELECT_INCLUDED
#define SQL_SELECT_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2008-2011 Monty Program Ab

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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "procedure.h"
#include <myisam.h>
#include "sql_array.h"                        /* Array */
#include "records.h"                          /* READ_RECORD */
#include "opt_range.h"                /* SQL_SELECT, QUICK_SELECT_I */


#if defined(WITH_ARIA_STORAGE_ENGINE)
#include <maria.h>
#endif
#if defined(USE_ARIA_FOR_TMP_TABLES)
#define TMP_ENGINE_HTON maria_hton
#else
#define TMP_ENGINE_HTON myisam_hton
#endif
/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2
#define KEY_OPTIMIZE_EQ	                4

inline uint get_hash_join_key_no() { return MAX_KEY; }

inline bool is_hash_join_key_no(uint key) { return key == MAX_KEY; }

typedef struct keyuse_t {
  TABLE *table;
  Item	*val;				/**< or value if no field */
  table_map used_tables;
  uint	key, keypart, optimize;
  key_part_map keypart_map;
  ha_rows      ref_table_rows;
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
  */
  bool null_rejecting;
  /*
    !NULL - This KEYUSE was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of 
            this KEYUSE element) can be turned on and off. The on/off state 
            is indicted by the pointed value:
              *cond_guard == TRUE <=> equality condition is on
              *cond_guard == FALSE <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)
  */
  bool *cond_guard;
  /*
     0..64    <=> This was created from semi-join IN-equality # sj_pred_no.
     MAX_UINT  Otherwise
  */
  uint         sj_pred_no;

  bool is_for_hash_join() { return is_hash_join_key_no(key); }
} KEYUSE;

#define NO_KEYPART ((uint)(-1))

class store_key;

const int NO_REF_PART= uint(-1);

typedef struct st_table_ref
{
  bool		key_err;
  /** True if something was read into buffer in join_read_key.  */
  bool          has_record;
  uint          key_parts;                ///< num of ...
  uint          key_length;               ///< length of key_buff
  int           key;                      ///< key no
  uchar         *key_buff;                ///< value to look for with key
  uchar         *key_buff2;               ///< key_buff+key_length
  store_key     **key_copy;               //
  Item          **items;                  ///< val()'s for each keypart
  /*  
    Array of pointers to trigger variables. Some/all of the pointers may be
    NULL.  The ref access can be used iff
    
      for each used key part i, (!cond_guards[i] || *cond_guards[i]) 

    This array is used by subquery code. The subquery code may inject
    triggered conditions, i.e. conditions that can be 'switched off'. A ref 
    access created from such condition is not valid when at least one of the 
    underlying conditions is switched off (see subquery code for more details)
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
    ref_or_null optimization: number of key part that alternates between
    the lookup value or NULL (there's only one such part). 
    If we're not using ref_or_null, the value is NO_REF_PART
  */
  uint           null_ref_part;

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

  bool tmp_table_index_lookup_init(THD *thd, KEY *tmp_key, Item_iterator &it,
                                   bool value, uint skip= 0);
  bool is_access_triggered();
} TABLE_REF;


/*
  The structs which holds the join connections and join states
*/
enum join_type { JT_UNKNOWN,JT_SYSTEM,JT_CONST,JT_EQ_REF,JT_REF,JT_MAYBE_REF,
		 JT_ALL, JT_RANGE, JT_NEXT, JT_FT, JT_REF_OR_NULL,
		 JT_UNIQUE_SUBQUERY, JT_INDEX_SUBQUERY, JT_INDEX_MERGE,
                 JT_HASH, JT_HASH_RANGE, JT_HASH_NEXT, JT_HASH_INDEX_MERGE};

class JOIN;

enum enum_nested_loop_state
{
  NESTED_LOOP_KILLED= -2, NESTED_LOOP_ERROR= -1,
  NESTED_LOOP_OK= 0, NESTED_LOOP_NO_MORE_ROWS= 1,
  NESTED_LOOP_QUERY_LIMIT= 3, NESTED_LOOP_CURSOR_LIMIT= 4
};


/* Possible sj_strategy values */
enum sj_strategy_enum
{
  SJ_OPT_NONE=0,
  SJ_OPT_DUPS_WEEDOUT=1,
  SJ_OPT_LOOSE_SCAN  =2,
  SJ_OPT_FIRST_MATCH =3,
  SJ_OPT_MATERIALIZE =4,
  SJ_OPT_MATERIALIZE_SCAN=5
};

/* Values for JOIN_TAB::packed_info */
#define TAB_INFO_HAVE_VALUE 1
#define TAB_INFO_USING_INDEX 2
#define TAB_INFO_USING_WHERE 4
#define TAB_INFO_FULL_SCAN_ON_NULL 8

typedef enum_nested_loop_state
(*Next_select_func)(JOIN *, struct st_join_table *, bool);
Next_select_func setup_end_select_func(JOIN *join);
int rr_sequential(READ_RECORD *info);
int rr_sequential_and_unpack(READ_RECORD *info);


class JOIN_CACHE;
class SJ_TMP_TABLE;
class JOIN_TAB_RANGE;

typedef struct st_join_table {
  st_join_table() {}                          /* Remove gcc warning */
  TABLE		*table;
  KEYUSE	*keyuse;			/**< pointer to first used key */
  KEY           *hj_key;       /**< descriptor of the used best hash join key
				    not supported by any index                 */
  SQL_SELECT	*select;
  COND		*select_cond;
  COND          *on_precond;    /**< part of on condition to check before
				     accessing the first inner table           */  
  QUICK_SELECT_I *quick;
  /* 
    The value of select_cond before we've attempted to do Index Condition
    Pushdown. We may need to restore everything back if we first choose one
    index but then reconsider (see test_if_skip_sort_order() for such
    scenarios).
    NULL means no index condition pushdown was performed.
  */
  Item          *pre_idx_push_select_cond;
  /*
    Pointer to the associated ON expression. on_expr_ref=!NULL except for
    degenerate joins. 
    *on_expr_ref!=NULL for tables that are first inner tables within an outer
    join.
  */
  Item	       **on_expr_ref;
  COND_EQUAL    *cond_equal;    /**< multiple equalities for the on expression */
  st_join_table *first_inner;   /**< first inner table for including outerjoin */
  bool           found;         /**< true after all matches or null complement */
  bool           not_null_compl;/**< true before null complement is added      */
  st_join_table *last_inner;    /**< last table table for embedding outer join */
  st_join_table *first_upper;  /**< first inner table for embedding outer join */
  st_join_table *first_unmatched; /**< used for optimization purposes only     */

  /*
    For join tabs that are inside an SJM bush: root of the bush
  */
  st_join_table *bush_root_tab;

  /* TRUE <=> This join_tab is inside an SJM bush and is the last leaf tab here */
  bool          last_leaf_in_bush;
  
  /*
    ptr  - this is a bush, and ptr points to description of child join_tab
           range
    NULL - this join tab has no bush children
  */
  JOIN_TAB_RANGE *bush_children;
  
  /* Special content for EXPLAIN 'Extra' column or NULL if none */
  const char	*info;
  /* 
    Bitmap of TAB_INFO_* bits that encodes special line for EXPLAIN 'Extra'
    column, or 0 if there is no info.
  */
  uint          packed_info;

  READ_RECORD::Setup_func read_first_record;
  Next_select_func next_select;
  READ_RECORD	read_record;
  /* 
    Currently the following two fields are used only for a [NOT] IN subquery
    if it is executed by an alternative full table scan when the left operand of
    the subquery predicate is evaluated to NULL.
  */  
  READ_RECORD::Setup_func save_read_first_record;/* to save read_first_record */
  READ_RECORD::Read_func save_read_record;/* to save read_record.read_record */
  double	worst_seeks;
  key_map	const_keys;			/**< Keys with constant part */
  key_map	checked_keys;			/**< Keys checked in find_best */
  key_map	needed_reg;
  key_map       keys;                           /**< all keys with can be used */

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
  double        read_time;
  
  /* psergey-todo: make the below have type double, like POSITION::records_read? */
  ha_rows       records_read;
  
  /* Startup cost for execution */
  double        startup_cost;
    
  double        partial_join_cardinality;

  table_map	dependent,key_dependent;
  /*
     1 - use quick select
     2 - use "Range checked for each record"
  */
  uint		use_quick;
  /*
    Index to use. Note: this is valid only for 'index' access, but not range or
    ref access.
  */
  uint          index;
  uint		status;				///< Save status for cache
  uint		used_fields;
  ulong         used_fieldlength;
  ulong         max_used_fieldlength;
  uint          used_blobs;
  uint          used_null_fields;
  uint          used_uneven_bit_fields;
  enum join_type type;
  bool		cached_eq_ref_table,eq_ref_table,not_used_in_distinct;
  bool		sorted;
  /* 
    If it's not 0 the number stored this field indicates that the index
    scan has been chosen to access the table data and we expect to scan 
    this number of rows for the table.
  */ 
  ha_rows       limit; 
  TABLE_REF	ref;
  /* TRUE <=> condition pushdown supports other tables presence */
  bool          icp_other_tables_ok;
  /* 
    TRUE <=> condition pushed to the index has to be factored out of
    the condition pushed to the table
  */
  bool          idx_cond_fact_out;
  bool          use_join_cache;
  uint          used_join_cache_level;
  ulong         join_buffer_size_limit;
  JOIN_CACHE	*cache;
  /*
    Index condition for BKA access join
  */
  Item          *cache_idx_cond;
  SQL_SELECT    *cache_select;
  JOIN		*join;
  /*
    Embedding SJ-nest (may be not the direct parent), or NULL if none.
    This variable holds the result of table pullout.
  */
  TABLE_LIST    *emb_sj_nest;

  /* FirstMatch variables (final QEP) */
  struct st_join_table *first_sj_inner_tab;
  struct st_join_table *last_sj_inner_tab;

  /* Variables for semi-join duplicate elimination */
  SJ_TMP_TABLE  *flush_weedout_table;
  SJ_TMP_TABLE  *check_weed_out_table;
  /* for EXPLAIN only: */
  SJ_TMP_TABLE  *first_weedout_table;
  
  /*
    If set, means we should stop join enumeration after we've got the first
    match and return to the specified join tab. May point to
    join->join_tab[-1] which means stop join execution after the first
    match.
  */
  struct st_join_table  *do_firstmatch;
 
  /* 
     ptr  - We're doing a LooseScan, this join tab is the first (i.e. 
            "driving") join tab), and ptr points to the last join tab
            handled by the strategy. loosescan_match_tab->found_match
            should be checked to see if the current value group had a match.
     NULL - Not doing a loose scan on this join tab.
  */
  struct st_join_table *loosescan_match_tab;
  
  /* TRUE <=> we are inside LooseScan range */
  bool inside_loosescan_range;

  /* Buffer to save index tuple to be able to skip duplicates */
  uchar *loosescan_buf;
  
  /* 
    Index used by LooseScan (we store it here separately because ref access
    stores it in tab->ref.key, while range scan stores it in tab->index, etc)
  */
  uint loosescan_key;

  /* Length of key tuple (depends on #keyparts used) to store in the above */
  uint loosescan_key_len;

  /* Used by LooseScan. TRUE<=> there has been a matching record combination */
  bool found_match;
  
  /*
    Used by DuplicateElimination. tab->table->ref must have the rowid
    whenever we have a current record.
  */
  int  keep_current_rowid;

  /* NestedOuterJoins: Bitmap of nested joins this table is part of */
  nested_join_map embedding_map;

  /*
    Semi-join strategy to be used for this join table. This is a copy of
    POSITION::sj_strategy field. This field is set up by the
    fix_semijoin_strategies_for_picked_join_order.
  */
  enum sj_strategy_enum sj_strategy;

  uint n_sj_tables;

  bool preread_init_done;

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
  bool is_inner_table_of_semi_join_with_first_match()
  {
    return first_sj_inner_tab != NULL;
  }
  bool is_inner_table_of_semijoin()
  {
    return emb_sj_nest != NULL;
  }
  bool is_inner_table_of_outer_join()
  {
    return first_inner != NULL;
  }
  bool is_single_inner_of_semi_join_with_first_match()
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
  bool use_match_flag()
  {
    return is_first_inner_for_outer_join() || first_sj_inner_tab == this ; 
  }
  bool check_only_first_match()
  {
    return is_inner_table_of_semi_join_with_first_match() ||
           (is_inner_table_of_outer_join() &&
            table->reginfo.not_exists_optimize);
  }
  bool is_last_inner_table()
  {
    return (first_inner && first_inner->last_inner == this) ||
           last_sj_inner_tab == this;
  }
  /*
    Check whether the table belongs to a nest of inner tables of an
    outer join or to a nest of inner tables of a semi-join
  */
  bool is_nested_inner()
  {
    if (first_inner && 
        (first_inner != first_inner->last_inner || first_inner->first_upper))
      return TRUE;
    if (first_sj_inner_tab && first_sj_inner_tab != last_sj_inner_tab)
      return TRUE;
    return FALSE;
  }
  struct st_join_table *get_first_inner_table()
  {
    if (first_inner)
      return first_inner;
    return first_sj_inner_tab; 
  }
  void set_select_cond(COND *to, uint line)
  {
    DBUG_PRINT("info", ("select_cond changes %p -> %p at line %u tab %p",
                        select_cond, to, line, this));
    select_cond= to;
  }
  COND *set_cond(COND *new_cond)
  {
    COND *tmp_select_cond= select_cond;
    set_select_cond(new_cond, __LINE__);
    if (select)
      select->cond= new_cond;
    return tmp_select_cond;
  }
  void calc_used_field_length(bool max_fl);
  ulong get_used_fieldlength()
  {
    if (!used_fieldlength)
      calc_used_field_length(FALSE);
    return used_fieldlength;
  }
  ulong get_max_used_fieldlength()
  {
    if (!max_used_fieldlength)
      calc_used_field_length(TRUE);
    return max_used_fieldlength;
  }
  double get_partial_join_cardinality() { return partial_join_cardinality; }
  bool hash_join_is_possible();
  int make_scan_filter();
  bool is_ref_for_hash_join() { return is_hash_join_key_no(ref.key); }
  KEY *get_keyinfo_by_key_no(uint key) 
  {
    return (is_hash_join_key_no(key) ? hj_key : table->key_info+key);
  }
  double scan_time();
  ha_rows get_examined_rows();
  bool preread_init();

  bool is_sjm_nest() { return test(bush_children); }
} JOIN_TAB;


#include "sql_join_cache.h"

enum_nested_loop_state sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool
                                        end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JOIN_TAB *join_tab, bool
                                  end_of_records);
enum_nested_loop_state
end_send_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	       bool end_of_records);
enum_nested_loop_state
end_write_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		bool end_of_records);


struct st_position;

class Semi_join_strategy_picker
{
public:
  /* Called when starting to build a new join prefix */
  virtual void set_empty() = 0;

  /* 
    Update internal state after another table has been added to the join
    prefix
  */
  virtual void set_from_prev(struct st_position *prev) = 0;
  
  virtual bool check_qep(JOIN *join,
                         uint idx,
                         table_map remaining_tables, 
                         const JOIN_TAB *new_join_tab,
                         double *record_count,
                         double *read_time,
                         table_map *handled_fanout,
                         sj_strategy_enum *strategy,
                         struct st_position *loose_scan_pos) = 0;

  virtual void mark_used() = 0;

  virtual ~Semi_join_strategy_picker() {} 
};


/*
  Duplicate Weedout strategy optimization state
*/

class Duplicate_weedout_picker : public Semi_join_strategy_picker
{
  /* The first table that the strategy will need to handle */
  uint  first_dupsweedout_table;

  /*
    Tables that we will need to have in the prefix to do the weedout step
    (all inner and all outer that the involved semi-joins are correlated with)
  */
  table_map dupsweedout_tables;
  
  bool is_used;
public:
  void set_empty()
  {
    dupsweedout_tables= 0;
    first_dupsweedout_table= MAX_TABLES;
    is_used= FALSE;
  }
  void set_from_prev(struct st_position *prev);
  
  bool check_qep(JOIN *join,
                 uint idx,
                 table_map remaining_tables, 
                 const JOIN_TAB *new_join_tab,
                 double *record_count,
                 double *read_time,
                 table_map *handled_fanout,
                 sj_strategy_enum *stratey,
                 struct st_position *loose_scan_pos);

  void mark_used() { is_used= TRUE; }
  friend void fix_semijoin_strategies_for_picked_join_order(JOIN *join);
};


class Firstmatch_picker : public Semi_join_strategy_picker
{
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

  bool is_used;

  bool in_firstmatch_prefix() { return (first_firstmatch_table != MAX_TABLES); }
  void invalidate_firstmatch_prefix() { first_firstmatch_table= MAX_TABLES; }
public:
  void set_empty()
  {
    invalidate_firstmatch_prefix();
    is_used= FALSE;
  }

  void set_from_prev(struct st_position *prev);
  bool check_qep(JOIN *join,
                 uint idx,
                 table_map remaining_tables, 
                 const JOIN_TAB *new_join_tab,
                 double *record_count,
                 double *read_time,
                 table_map *handled_fanout,
                 sj_strategy_enum *strategy,
                 struct st_position *loose_scan_pos);

  void mark_used() { is_used= TRUE; }
  friend void fix_semijoin_strategies_for_picked_join_order(JOIN *join);
};


class LooseScan_picker : public Semi_join_strategy_picker
{
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
  
  bool is_used;
public:
  void set_empty()
  {
    first_loosescan_table= MAX_TABLES; 
    is_used= FALSE;
  }

  void set_from_prev(struct st_position *prev);
  bool check_qep(JOIN *join,
                 uint idx,
                 table_map remaining_tables, 
                 const JOIN_TAB *new_join_tab,
                 double *record_count,
                 double *read_time,
                 table_map *handled_fanout,
                 sj_strategy_enum *strategy,
                 struct st_position *loose_scan_pos);
  void mark_used() { is_used= TRUE; }

  friend class Loose_scan_opt;
  friend void best_access_path(JOIN      *join,
                               JOIN_TAB  *s,
                               table_map remaining_tables,
                               uint      idx,
                               bool      disable_jbuf,
                               double    record_count,
                               struct st_position *pos,
                               struct st_position *loose_scan_pos);
  friend bool get_best_combination(JOIN *join);
  friend int setup_semijoin_dups_elimination(JOIN *join, ulonglong options,
                                             uint no_jbuf_after);
  friend void fix_semijoin_strategies_for_picked_join_order(JOIN *join);
};


class Sj_materialization_picker : public Semi_join_strategy_picker
{
  bool is_used;

  /* The last inner table (valid once we're after it) */
  uint      sjm_scan_last_inner;
  /*
    Tables that we need to have in the prefix to calculate the correct cost.
    Basically, we need all inner tables and outer tables mentioned in the
    semi-join's ON expression so we can correctly account for fanout.
  */
  table_map sjm_scan_need_tables;

public:
  void set_empty()
  {
    sjm_scan_need_tables= 0;
    LINT_INIT(sjm_scan_last_inner);
    is_used= FALSE;
  }
  void set_from_prev(struct st_position *prev);
  bool check_qep(JOIN *join,
                 uint idx,
                 table_map remaining_tables, 
                 const JOIN_TAB *new_join_tab,
                 double *record_count,
                 double *read_time,
                 table_map *handled_fanout,
                 sj_strategy_enum *strategy,
                 struct st_position *loose_scan_pos);
  void mark_used() { is_used= TRUE; }

  friend void fix_semijoin_strategies_for_picked_join_order(JOIN *join);
};


/**
  Information about a position of table within a join order. Used in join
  optimization.
*/
typedef struct st_position :public Sql_alloc
{
  /* The table that's put into join order */
  JOIN_TAB *table;

  /*
    The "fanout": number of output rows that will be produced (after
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

  /* Cumulative cost and record count for the join prefix */
  COST_VECT prefix_cost;
  double    prefix_record_count;

  /*
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  KEYUSE *key;

  /* If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;
 
  /*
    TRUE <=> join buffering will be used. At the moment this is based on 
    *very* imprecise guesses made in best_access_path(). 
  */
  bool use_join_buffer;
 
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
  enum sj_strategy_enum sj_strategy;
  
  /*
    Valid only after fix_semijoin_strategies_for_picked_join_order() call:
    if sj_strategy!=SJ_OPT_NONE, this is the number of subsequent tables that
    are covered by the specified semi-join strategy
  */
  uint n_sj_tables;

  table_map prefix_dups_producing_tables;

  table_map inner_tables_handled_with_other_sjs;
   
  Duplicate_weedout_picker  dups_weedout_picker;
  Firstmatch_picker         firstmatch_picker;
  LooseScan_picker          loosescan_picker;
  Sj_materialization_picker sjmat_picker;
} POSITION;

typedef struct st_rollup
{
  enum State { STATE_NONE, STATE_INITED, STATE_READY };
  State state;
  Item_null_result **null_items;
  Item ***ref_pointer_arrays;
  List<Item> *fields;
} ROLLUP;


class JOIN_TAB_RANGE: public Sql_alloc
{
public:
  JOIN_TAB *start;
  JOIN_TAB *end;
};


class JOIN :public Sql_alloc
{
private:
  JOIN(const JOIN &rhs);                        /**< not implemented */
  JOIN& operator=(const JOIN &rhs);             /**< not implemented */

protected:

  /**
    The subset of the state of a JOIN that represents an optimized query
    execution plan. Allows saving/restoring different JOIN plans for the same
    query.
  */
  class Join_plan_state {
  public:
    DYNAMIC_ARRAY keyuse;        /* Copy of the JOIN::keyuse array. */
    POSITION *best_positions;    /* Copy of JOIN::best_positions */
    /* Copies of the JOIN_TAB::keyuse pointers for each JOIN_TAB. */
    KEYUSE **join_tab_keyuse;
    /* Copies of JOIN_TAB::checked_keys for each JOIN_TAB. */
    key_map *join_tab_checked_keys;
    SJ_MATERIALIZATION_INFO **sj_mat_info;
    my_bool error;
  public:
    Join_plan_state(uint tables) : error(0)
    {   
      keyuse.elements= 0;
      keyuse.buffer= NULL;
      best_positions= 0;                        /* To detect errors */
      error= my_multi_malloc(MYF(MY_WME),
                             &best_positions,
                             sizeof(*best_positions) * (tables + 1),
                             &join_tab_keyuse,
                             sizeof(*join_tab_keyuse) * tables,
                             &join_tab_checked_keys,
                             sizeof(*join_tab_checked_keys) * tables,
                             &sj_mat_info,
                             sizeof(sj_mat_info) * tables,
                             NullS) == 0;
    }
    Join_plan_state(JOIN *join);
    ~Join_plan_state()
    {
      delete_dynamic(&keyuse);
      my_free(best_positions);
    }
  };

  /* Results of reoptimizing a JOIN via JOIN::reoptimize(). */
  enum enum_reopt_result {
    REOPT_NEW_PLAN, /* there is a new reoptimized plan */
    REOPT_OLD_PLAN, /* no new improved plan can be found, use the old one */
    REOPT_ERROR,    /* an irrecovarable error occured during reoptimization */
    REOPT_NONE      /* not yet reoptimized */
  };

  /* Support for plan reoptimization with rewritten conditions. */
  enum_reopt_result reoptimize(Item *added_where, table_map join_tables,
                               Join_plan_state *save_to);
  void save_query_plan(Join_plan_state *save_to);
  void reset_query_plan();
  void restore_query_plan(Join_plan_state *restore_from);
  /* Choose a subquery plan for a table-less subquery. */
  bool choose_tableless_subquery_plan();

public:
  JOIN_TAB *join_tab, **best_ref;
  
  /* 
    Saved join_tab for pre_sorting. create_sort_index() will save here.. 
  */
  JOIN_TAB *pre_sort_join_tab;
  uint pre_sort_index;
  Item *pre_sort_idx_pushed_cond;
  void clean_pre_sort_join_tab();

  /*
    For "Using temporary+Using filesort" queries, JOIN::join_tab can point to
    either: 
    1. array of join tabs describing how to run the select, or
    2. array of single join tab describing read from the temporary table.

    SHOW EXPLAIN code needs to read/show #1. This is why two next members are
    there for saving it.
  */
  JOIN_TAB *table_access_tabs;
  uint     top_table_access_tabs_count;

  JOIN_TAB **map2table;    ///< mapping between table indexes and JOIN_TABs
  JOIN_TAB *join_tab_save; ///< saved join_tab for subquery reexecution

  List<JOIN_TAB_RANGE> join_tab_ranges;
  
  /*
    Base tables participating in the join. After join optimization is done, the
    tables are stored in the join order (but the only really important part is 
    that const tables are first).
  */
  TABLE    **table;
  /**
    The table which has an index that allows to produce the requried ordering.
    A special value of 0x1 means that the ordering will be produced by
    passing 1st non-const table to filesort(). NULL means no such table exists.
  */
  TABLE    *sort_by_table;
  /* 
    Number of tables in the join. 
    (In MySQL, it is named 'tables' and is also the number of elements in 
     join->join_tab array. In MariaDB, the latter is not true, so we've renamed
     the variable)
  */
  uint	   table_count;
  uint     outer_tables;  /**< Number of tables that are not inside semijoin */
  uint     const_tables;
  /* 
    Number of tables in the top join_tab array. Normally this matches
    (join_tab_ranges.head()->end - join_tab_ranges.head()->start). 
    
    We keep it here so that it is saved/restored with JOIN::restore_tmp.
  */
  uint     top_join_tab_count;
  uint	   send_group_parts;
  bool	   group;          /**< If query contains GROUP BY clause */
  bool     need_distinct;

  /**
    Indicates that grouping will be performed on the result set during
    query execution. This field belongs to query execution.

    @see make_group_fields, alloc_group_fields, JOIN::exec
  */
  bool     sort_and_group; 
  bool     first_record,full_join, no_field_update;
  bool     hash_join;
  bool	   do_send_rows;
  table_map const_table_map;
  /*
    Constant tables for which we have found a row (as opposed to those for
    which we didn't).
  */
  table_map found_const_table_map;
  
  /* Tables removed by table elimination. Set to 0 before the elimination. */
  table_map eliminated_tables;
  /*
     Bitmap of all inner tables from outer joins (set at start of
     make_join_statistics)
  */
  table_map outer_join;
  /* Bitmap of tables used in the select list items */
  table_map select_list_used_tables;
  ha_rows  send_records,found_records,examined_rows,row_limit, select_limit;
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
  /* Finally picked QEP. This is result of join optimization */
  POSITION *best_positions;

/******* Join optimization state members start *******/
  /*
    pointer - we're doing optimization for a semi-join materialization nest.
    NULL    - otherwise
  */
  TABLE_LIST *emb_sjm_nest;
  
  /* Current join optimization state */
  POSITION *positions;
  
  /*
    Bitmap of nested joins embedding the position at the end of the current 
    partial join (valid only during join optimizer run).
  */
  nested_join_map cur_embedding_map;
  
  /*
    Bitmap of inner tables of semi-join nests that have a proper subset of
    their tables in the current join prefix. That is, of those semi-join
    nests that have their tables both in and outside of the join prefix.
  */
  table_map cur_sj_inner_tables;
  
  /*
    Bitmap of semi-join inner tables that are in the join prefix and for
    which there's no provision for how to eliminate semi-join duplicates
    they produce.
  */
  table_map cur_dups_producing_tables;
  
  /* We also maintain a stack of join optimization states in * join->positions[] */
/******* Join optimization state members end *******/

  /*
    Tables within complex firstmatch ranges (i.e. those where inner tables are
    interleaved with outer tables). Join buffering cannot be used for these.
  */
  table_map complex_firstmatch_tables;

  /*
    The cost of best complete join plan found so far during optimization,
    after optimization phase - cost of picked join order (not taking into
    account the changes made by test_if_skip_sort_order()).
  */
  double   best_read;
  /*
    Estimated result rows (fanout) of the join operation. If this is a subquery
    that is reexecuted multiple times, this value includes the estiamted # of
    reexecutions. This value is equal to the multiplication of all
    join->positions[i].records_read of a JOIN.
  */
  double   record_count;
  List<Item> *fields;
  List<Cached_item> group_fields, group_fields_cache;
  TABLE    *tmp_table;
  /// used to store 2 possible tmp table of SELECT
  TABLE    *exec_tmp_table1, *exec_tmp_table2;
  THD	   *thd;
  Item_sum  **sum_funcs, ***sum_funcs_end;
  /** second copy of sumfuncs (for queries with 2 temporary tables */
  Item_sum  **sum_funcs2, ***sum_funcs_end2;
  Procedure *procedure;
  Item	    *having;
  Item      *tmp_having; ///< To store having when processed temporary table
  Item      *having_history; ///< Store having for explain
  ulonglong  select_options;
  /* 
    Bitmap of allowed types of the join caches that
    can be used for join operations
  */
  uint allowed_join_cache_types;
  bool allowed_semijoin_with_cache;
  bool allowed_outer_join_with_cache;
  /* Maximum level of the join caches that can be used for join operations */ 
  uint max_allowed_join_cache_level;
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
  /*
    This flag is set if we call no_rows_in_result() as par of end_group().
    This is used as a simple speed optimization to avoiding calling
    restore_no_rows_in_result() in ::reinit()
  */
  bool no_rows_in_result_called;
  
  /**
    Copy of this JOIN to be used with temporary tables.

    tmp_join is used when the JOIN needs to be "reusable" (e.g. in a
    subquery that gets re-executed several times) and we know will use
    temporary tables for materialization. The materialization to a
    temporary table overwrites the JOIN structure to point to the
    temporary table after the materialization is done. This is where
    tmp_join is used : it's a copy of the JOIN before the
    materialization and is used in restoring before re-execution by
    overwriting the current JOIN structure with the saved copy.
    Because of this we should pay extra care of not freeing up helper
    structures that are referenced by the original contents of the
    JOIN. We can check for this by making sure the "current" join is
    not the temporary copy, e.g.  !tmp_join || tmp_join != join
 
    We should free these sub-structures at JOIN::destroy() if the
    "current" join has a copy is not that copy.
  */
  JOIN *tmp_join;
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
  /**
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool no_order;
  /** Is set if we have a GROUP BY and we have ORDER BY on a constant. */
  bool          skip_sort_order;

  bool need_tmp, hidden_group_fields;
  /* TRUE if there was full cleunap of the JOIN */
  bool cleaned;
  DYNAMIC_ARRAY keyuse;
  Item::cond_result cond_value, having_value;
  List<Item> all_fields; ///< to store all fields that used in query
  ///Above list changed to use temporary table
  List<Item> tmp_all_fields1, tmp_all_fields2, tmp_all_fields3;
  ///Part, shared with list above, emulate following list
  List<Item> tmp_fields_list1, tmp_fields_list2, tmp_fields_list3;
  List<Item> &fields_list; ///< hold field list passed to mysql_select
  List<Item> procedure_fields_list;
  int error;

  ORDER *order, *group_list, *proc_param; //hold parameters of mysql_select
  COND *conds;                            // ---"---
  Item *conds_history;                    // store WHERE for explain
  COND *outer_ref_cond;       ///<part of conds containing only outer references
  COND *pseudo_bits_cond;     // part of conds containing special bita
  TABLE_LIST *tables_list;           ///<hold 'tables' parameter of mysql_select
  List<TABLE_LIST> *join_list;       ///< list of joined tables in reverse order
  COND_EQUAL *cond_equal;
  COND_EQUAL *having_equal;
  /*
    Constant codition computed during optimization, but evaluated during
    join execution. Typically expensive conditions that should not be
    evaluated at optimization time.
  */
  Item *exec_const_cond;
  /*
    Constant ORDER and/or GROUP expressions that contain subqueries. Such
    expressions need to evaluated to verify that the subquery indeed
    returns a single row. The evaluation of such expressions is delayed
    until query execution.
  */
  List<Item> exec_const_order_group_cond;
  SQL_SELECT *select;                ///<created in optimisation phase
  JOIN_TAB *return_tab;              ///<used only for outer joins
  Item **ref_pointer_array; ///<used pointer reference for this select
  // Copy of above to be used with different lists
  Item **items0, **items1, **items2, **items3, **current_ref_pointer_array;
  uint ref_pointer_array_size; ///< size of above in bytes
  const char *zero_result_cause; ///< not 0 if exec must return zero result
  
  bool union_part; ///< this subselect is part of union 
  bool optimized; ///< flag to avoid double optimization in EXPLAIN
  bool initialized; ///< flag to avoid double init_execution calls

  /*
    Additional WHERE and HAVING predicates to be considered for IN=>EXISTS
    subquery transformation of a JOIN object.
  */
  Item *in_to_exists_where;
  Item *in_to_exists_having;
  
  /* Temporary tables used to weed-out semi-join duplicates */
  List<TABLE> sj_tmp_tables;
  /* SJM nests that are executed with SJ-Materialization strategy */
  List<SJ_MATERIALIZATION_INFO> sjm_info_list;

  /* 
    storage for caching buffers allocated during query execution. 
    These buffers allocations need to be cached as the thread memory pool is
    cleared only at the end of the execution of the whole query and not caching
    allocations that occur in repetition at execution time will result in 
    excessive memory usage.
    Note: make_simple_join always creates an execution plan that accesses
    a single table, thus it is sufficient to have a one-element array for
    table_reexec.
  */  
  SORT_FIELD *sortorder;                        // make_unireg_sortorder()
  TABLE *table_reexec[1];                       // make_simple_join()
  JOIN_TAB *join_tab_reexec;                    // make_simple_join()
  /* end of allocation caching storage */

  JOIN(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
    :fields_list(fields_arg)
  {
    init(thd_arg, fields_arg, select_options_arg, result_arg);
  }

  void init(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
  {
    join_tab= join_tab_save= 0;
    table= 0;
    table_count= 0;
    top_join_tab_count= 0;
    const_tables= 0;
    eliminated_tables= 0;
    join_list= 0;
    implicit_grouping= FALSE;
    sort_and_group= 0;
    first_record= 0;
    do_send_rows= 1;
    send_records= 0;
    found_records= 0;
    fetch_limit= HA_POS_ERROR;
    examined_rows= 0;
    exec_tmp_table1= 0;
    exec_tmp_table2= 0;
    sortorder= 0;
    table_reexec[0]= 0;
    join_tab_reexec= 0;
    thd= thd_arg;
    sum_funcs= sum_funcs2= 0;
    procedure= 0;
    having= tmp_having= having_history= 0;
    select_options= select_options_arg;
    result= result_arg;
    lock= thd_arg->lock;
    select_lex= 0; //for safety
    tmp_join= 0;
    select_distinct= test(select_options & SELECT_DISTINCT);
    no_order= 0;
    simple_order= 0;
    simple_group= 0;
    need_distinct= 0;
    skip_sort_order= 0;
    need_tmp= 0;
    hidden_group_fields= 0; /*safety*/
    error= 0;
    select= 0;
    return_tab= 0;
    ref_pointer_array= items0= items1= items2= items3= 0;
    ref_pointer_array_size= 0;
    zero_result_cause= 0;
    optimized= 0;
    initialized= 0;
    cleaned= 0;
    cond_equal= 0;
    having_equal= 0;
    exec_const_cond= 0;
    group_optimized_away= 0;
    no_rows_in_result_called= 0;
    positions= best_positions= 0;

    all_fields= fields_arg;
    if (&fields_list != &fields_arg)      /* Avoid valgrind-warning */
      fields_list= fields_arg;
    bzero((char*) &keyuse,sizeof(keyuse));
    tmp_table_param.init();
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;

    no_const_tables= FALSE;
    outer_ref_cond= pseudo_bits_cond= NULL;
    in_to_exists_where= NULL;
    in_to_exists_having= NULL;

    pre_sort_join_tab= NULL;
  }

  int prepare(Item ***rref_pointer_array, TABLE_LIST *tables, uint wind_num,
	      COND *conds, uint og_num, ORDER *order, ORDER *group,
	      Item *having, ORDER *proc_param, SELECT_LEX *select,
	      SELECT_LEX_UNIT *unit);
  bool prepare_stage2();
  int optimize();
  int reinit();
  int init_execution();
  void exec();
  int destroy();
  void restore_tmp();
  bool alloc_func_list();
  bool flatten_subqueries();
  bool optimize_unflattened_subqueries();
  bool optimize_constant_subqueries();
  bool make_sum_func_list(List<Item> &all_fields, List<Item> &send_fields,
			  bool before_group_by, bool recompute= FALSE);

  inline void set_items_ref_array(Item **ptr)
  {
    memcpy((char*) ref_pointer_array, (char*) ptr, ref_pointer_array_size);
    current_ref_pointer_array= ptr;
  }
  inline void init_items_ref_array()
  {
    items0= ref_pointer_array + all_fields.elements;
    memcpy(items0, ref_pointer_array, ref_pointer_array_size);
    current_ref_pointer_array= items0;
  }

  bool rollup_init();
  bool rollup_process_const_fields();
  bool rollup_make_fields(List<Item> &all_fields, List<Item> &fields,
			  Item_sum ***func);
  int rollup_send_data(uint idx);
  int rollup_write_data(uint idx, TABLE *table);
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
  bool init_save_join_tab();
  bool send_row_on_empty_set()
  {
    return (do_send_rows && implicit_grouping && !group_optimized_away &&
            having_value != Item::COND_FALSE);
  }
  bool empty_result() { return (zero_result_cause && !implicit_grouping); }
  bool change_result(select_result *result);
  bool is_top_level_join() const
  {
    return (unit == &thd->lex->unit && (unit->fake_select_lex == 0 ||
                                        select_lex == unit->fake_select_lex));
  }
  void cache_const_exprs();
  inline table_map all_tables_map()
  {
    return (table_map(1) << table_count) - 1;
  }
  void drop_unused_derived_keys();
  inline void eval_select_list_used_tables();
  /* 
    Return the table for which an index scan can be used to satisfy 
    the sort order needed by the ORDER BY/(implicit) GROUP BY clause 
  */
  JOIN_TAB *get_sort_by_join_tab()
  {
    return (need_tmp || !sort_by_table || skip_sort_order ||
            ((group || tmp_table_param.sum_func_count) && !group_list)) ?
              NULL : join_tab+const_tables;
  }
  bool setup_subquery_caches();
  bool shrink_join_buffers(JOIN_TAB *jt, 
                           ulonglong curr_space,
                           ulonglong needed_space);
  void set_allowed_join_cache_types();
  bool is_allowed_hash_join_access()
  { 
    return test(allowed_join_cache_types & JOIN_CACHE_HASHED_BIT) &&
           max_allowed_join_cache_level > JOIN_CACHE_HASHED_BIT;
  }
  /*
    Check if we need to create a temporary table.
    This has to be done if all tables are not already read (const tables)
    and one of the following conditions holds:
    - We are using DISTINCT (simple distinct's are already optimized away)
    - We are using an ORDER BY or GROUP BY on fields not in the first table
    - We are using different ORDER BY and GROUP BY orders
    - The user wants us to buffer the result.
    When the WITH ROLLUP modifier is present, we cannot skip temporary table
    creation for the DISTINCT clause just because there are only const tables.
  */
  bool test_if_need_tmp_table()
  {
    return ((const_tables != table_count &&
	    ((select_distinct || !simple_order || !simple_group) ||
	     (group_list && order) ||
	     test(select_options & OPTION_BUFFER_RESULT))) ||
            (rollup.state != ROLLUP::STATE_NONE && select_distinct));
  }
  bool choose_subquery_plan(table_map join_tables);
  void get_partial_cost_and_fanout(int end_tab_idx,
                                   table_map filter_map,
                                   double *read_time_arg, 
                                   double *record_count_arg);
  void get_prefix_cost_and_fanout(uint n_tables, 
                                  double *read_time_arg,
                                  double *record_count_arg);
  double get_examined_rows();
  /* defined in opt_subselect.cc */
  bool transform_max_min_subquery();
  /* True if this JOIN is a subquery under an IN predicate. */
  bool is_in_subquery()
  {
    return (unit->item && unit->item->is_in_predicate());
  }
private:
  /**
    TRUE if the query contains an aggregate function but has no GROUP
    BY clause. 
  */
  bool implicit_grouping; 
  bool make_simple_join(JOIN *join, TABLE *tmp_table);
  void cleanup_item_list(List<Item> &items) const;
};

enum enum_with_bush_roots { WITH_BUSH_ROOTS, WITHOUT_BUSH_ROOTS};
enum enum_with_const_tables { WITH_CONST_TABLES, WITHOUT_CONST_TABLES};

JOIN_TAB *first_linear_tab(JOIN *join, enum enum_with_const_tables const_tbls);
JOIN_TAB *next_linear_tab(JOIN* join, JOIN_TAB* tab, 
                          enum enum_with_bush_roots include_bush_roots);

JOIN_TAB *first_top_level_tab(JOIN *join, enum enum_with_const_tables with_const);
JOIN_TAB *next_top_level_tab(JOIN *join, JOIN_TAB *tab);

typedef struct st_select_check {
  uint const_ref,reg_ref;
} SELECT_CHECK;

extern const char *join_type_str[];

/* Extern functions in sql_select.cc */
void count_field_types(SELECT_LEX *select_lex, TMP_TABLE_PARAM *param, 
                       List<Item> &fields, bool reset_with_sum_func);
bool setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param,
		       Item **ref_pointer_array,
		       List<Item> &new_list1, List<Item> &new_list2,
		       uint elements, List<Item> &fields);
void copy_fields(TMP_TABLE_PARAM *param);
bool copy_funcs(Item **func_ptr, const THD *thd);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);
Field* create_tmp_field_from_field(THD *thd, Field* org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item, uint convert_blob_length);
                                                                      
bool is_indexed_agg_distinct(JOIN *join, List<Item_field> *out_args);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(THD* thd,
                  List<TABLE_LIST> &tables, List<Item> &all_fields, COND *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b);

/** class to copying an field/item to a key struct */

class store_key :public Sql_alloc
{
public:
  bool null_key; /* TRUE <=> the value of the key has a null part */
  enum store_key_result { STORE_KEY_OK, STORE_KEY_FATAL, STORE_KEY_CONV };
  enum Type { FIELD_STORE_KEY, ITEM_STORE_KEY, CONST_ITEM_STORE_KEY };
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
  store_key(store_key &arg)
    :Sql_alloc(), null_key(arg.null_key), to_field(arg.to_field),
             null_ptr(arg.null_ptr), err(arg.err)

  {}
  virtual ~store_key() {}			/** Not actually needed */
  virtual enum Type type() const=0;
  virtual const char *name() const=0;
  virtual bool store_key_is_const() { return false; }

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
    ulonglong sql_mode= thd->variables.sql_mode;
    thd->variables.sql_mode&= ~(MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE);
    thd->variables.sql_mode|= MODE_INVALID_DATES;

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

  enum Type type() const { return FIELD_STORE_KEY; }
  const char *name() const { return field_name; }

  void change_source_field(Item_field *fld_item)
  {
    copy_field.set(to_field, fld_item->field, 0);
    field_name= fld_item->full_name();
  }

 protected: 
  enum store_key_result copy_inner()
  {
    TABLE *table= copy_field.to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);

    /* 
      It looks like the next statement is needed only for a simplified
      hash function over key values used now in BNLH join.
      When the implementation of this function will be replaced for a proper
      full version this statement probably should be removed.
    */  
    bzero(copy_field.to_ptr,copy_field.to_length);

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
  /*
    Flag that forces usage of save_val() method which save value of the
    item instead of save_in_field() method which saves result.
  */
  bool use_value;
public:
  store_key_item(THD *thd, Field *to_field_arg, uchar *ptr,
                 uchar *null_ptr_arg, uint length, Item *item_arg, bool val)
    :store_key(thd, to_field_arg, ptr,
	       null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
	       &err : (uchar*) 0, length), item(item_arg), use_value(val)
  {}
  store_key_item(store_key &arg, Item *new_item, bool val)
    :store_key(arg), item(new_item), use_value(val)
  {}


  enum Type type() const { return ITEM_STORE_KEY; }
  const char *name() const { return "func"; }

 protected:  
  enum store_key_result copy_inner()
  {
    TABLE *table= to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);
    int res= FALSE;

    /* 
      It looks like the next statement is needed only for a simplified
      hash function over key values used now in BNLH join.
      When the implementation of this function will be replaced for a proper
      full version this statement probably should be removed.
    */  
    to_field->reset();

    if (use_value)
      item->save_val(to_field);
    else
      res= item->save_in_field(to_field, 1);
    /*
     Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
     we need to check for errors executing it and react accordingly
    */
    if (!res && table->in_use->is_error())
      res= 1; /* STORE_KEY_FATAL */
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key= to_field->is_null() || item->null_value;
    return ((err != 0 || res < 0 || res > 2) ? STORE_KEY_FATAL : 
            (store_key_result) res);
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
		    null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
		    &err : (uchar*) 0, length, item_arg, FALSE), inited(0)
  {
  }
  store_key_const_item(store_key &arg, Item *new_item)
    :store_key_item(arg, new_item, FALSE), inited(0)
  {}

  enum Type type() const { return CONST_ITEM_STORE_KEY; }
  const char *name() const { return "const"; }
  bool store_key_is_const() { return true; }

protected:  
  enum store_key_result copy_inner()
  {
    int res;
    if (!inited)
    {
      inited=1;
      TABLE *table= to_field->table;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                       table->write_set);
      if ((res= item->save_in_field(to_field, 1)))
      {       
        if (!err)
          err= res < 0 ? 1 : res; /* 1=STORE_KEY_FATAL */
      }
      /*
        Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
        we need to check for errors executing it and react accordingly
        */
      if (!err && to_field->table->in_use->is_error())
        err= 1; /* STORE_KEY_FATAL */
      dbug_tmp_restore_column_map(table->write_set, old_map);
    }
    null_key= to_field->is_null() || item->null_value;
    return (err > 2 ? STORE_KEY_FATAL : (store_key_result) err);
  }
};

bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref);
bool error_if_full_join(JOIN *join);
int report_error(TABLE *table, int error);
int safe_index_read(JOIN_TAB *tab);
COND *remove_eq_conds(THD *thd, COND *cond, Item::cond_result *cond_value);
int get_quick_record(SQL_SELECT *select);
SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length,
                                  SORT_FIELD *sortorder);
int setup_order(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List <Item> &all_fields, ORDER *order);
int setup_group(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List<Item> &all_fields, ORDER *order,
		bool *hidden_group_fields);
bool fix_inner_refs(THD *thd, List<Item> &all_fields, SELECT_LEX *select,
                   Item **ref_pointer_array);
int join_read_key2(THD *thd, struct st_join_table *tab, TABLE *table,
                   struct st_table_ref *table_ref);

bool handle_select(THD *thd, LEX *lex, select_result *result,
                   ulong setup_tables_done_option);
bool mysql_select(THD *thd, Item ***rref_pointer_array,
                  TABLE_LIST *tables, uint wild_num,  List<Item> &list,
                  COND *conds, uint og_num, ORDER *order, ORDER *group,
                  Item *having, ORDER *proc_param, ulonglong select_type, 
                  select_result *result, SELECT_LEX_UNIT *unit, 
                  SELECT_LEX *select_lex);
void free_underlaid_joins(THD *thd, SELECT_LEX *select);
bool mysql_explain_union(THD *thd, SELECT_LEX_UNIT *unit,
                         select_result *result);
Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
			Item ***copy_func, Field **from_field,
                        Field **def_field,
			bool group, bool modify_item,
			bool table_cant_handle_bit_fields,
                        bool make_copy_field,
                        uint convert_blob_length);
bool create_internal_tmp_table(TABLE *table, KEY *keyinfo, 
                               ENGINE_COLUMNDEF *start_recinfo,
                               ENGINE_COLUMNDEF **recinfo, 
                               ulonglong options, my_bool big_tables);

/*
  General routine to change field->ptr of a NULL-terminated array of Field
  objects. Useful when needed to call val_int, val_str or similar and the
  field data is not in table->record[0] but in some other structure.
  set_key_field_ptr changes all fields of an index using a key_info object.
  All methods presume that there is at least one field to change.
*/

TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list);

int test_if_item_cache_changed(List<Cached_item> &list);
int join_init_read_record(JOIN_TAB *tab);
int join_read_record_no_init(JOIN_TAB *tab);
void set_position(JOIN *join,uint idx,JOIN_TAB *table,KEYUSE *key);
inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}
bool choose_plan(JOIN *join, table_map join_tables);
void optimize_wo_join_buffering(JOIN *join, uint first_tab, uint last_tab, 
                                table_map last_remaining_tables, 
                                bool first_alt, uint no_jbuf_before,
                                double *outer_rec_count, double *reopt_cost);
Item_equal *find_item_equal(COND_EQUAL *cond_equal, Field *field,
                            bool *inherited_fl);
extern bool test_if_ref(Item *, 
                 Item_field *left_item,Item *right_item);

inline bool optimizer_flag(THD *thd, uint flag)
{ 
  return (thd->variables.optimizer_switch & flag);
}

uint get_index_for_order(ORDER *order, TABLE *table, SQL_SELECT *select,
                         ha_rows limit, bool *need_sort, bool *reverse);
ORDER *simple_remove_const(ORDER *order, COND *where);
bool const_expression_in_where(COND *cond, Item *comp_item,
                               Field *comp_field= NULL,
                               Item **const_item= NULL);

/* Table elimination entry point function */
void eliminate_tables(JOIN *join);

/* Index Condition Pushdown entry point function */
void push_index_cond(JOIN_TAB *tab, uint keyno);

/****************************************************************************
  Temporary table support for SQL Runtime
 ***************************************************************************/

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2
#define MIN_STRING_LENGTH_TO_PACK_ROWS   10

TABLE *create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
			ORDER *group, bool distinct, bool save_sum_fields,
			ulonglong select_options, ha_rows rows_limit,
			const char* alias, bool do_not_open=FALSE);
void free_tmp_table(THD *thd, TABLE *entry);
bool create_internal_tmp_table_from_heap(THD *thd, TABLE *table,
                                         ENGINE_COLUMNDEF *start_recinfo,
                                         ENGINE_COLUMNDEF **recinfo, 
                                         int error, bool ignore_last_dupp_key_error);
bool create_internal_tmp_table(TABLE *table, KEY *keyinfo, 
                               ENGINE_COLUMNDEF *start_recinfo,
                               ENGINE_COLUMNDEF **recinfo, 
                               ulonglong options, my_bool big_tables);
bool open_tmp_table(TABLE *table);
void setup_tmp_table_column_bitmaps(TABLE *table, uchar *bitmaps);
double prev_record_reads(POSITION *positions, uint idx, table_map found_ref);
void fix_list_after_tbl_changes(SELECT_LEX *new_parent, List<TABLE_LIST> *tlist);

#endif /* SQL_SELECT_INCLUDED */

#ifndef ITEM_SUBSELECT_INCLUDED
#define ITEM_SUBSELECT_INCLUDED

/* Copyright (c) 2002, 2011, Oracle and/or its affiliates.

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

/* subselect Item */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <queues.h>

class st_select_lex;
class st_select_lex_unit;
class JOIN;
class select_result_interceptor;
class subselect_engine;
class subselect_hash_sj_engine;
class Item_bool_func2;
class Comp_creator;

typedef class st_select_lex SELECT_LEX;

/**
  Convenience typedef used in this file, and further used by any files
  including this file.
*/
typedef Comp_creator* (*chooser_compare_func_creator)(bool invert);
class Cached_item;

/* base class for subselects */

class Item_subselect :public Item_result_field
{
  bool value_assigned;   /* value already assigned to subselect */
  bool own_engine;  /* the engine was not taken from other Item_subselect */
protected:
  /* thread handler, will be assigned in fix_fields only */
  THD *thd;
  /* old engine if engine was changed */
  subselect_engine *old_engine;
  /* cache of used external tables */
  table_map used_tables_cache;
  /* allowed number of columns (1 for single value subqueries) */
  uint max_columns;
  /* where subquery is placed */
  enum_parsing_place parsing_place;
  /* work with 'substitution' */
  bool have_to_be_excluded;
  /* cache of constant state */
  bool const_item_cache;
  
  bool inside_first_fix_fields;
  bool done_first_fix_fields;
  Item *expr_cache;
  /*
    Set to TRUE if at optimization or execution time we determine that this
    item's value is a constant. We need this member because it is not possible
    to substitute 'this' with a constant item.
  */
  bool forced_const;
#ifndef DBUG_OFF
  /* Count the number of times this subquery predicate has been executed. */
  uint exec_counter;
#endif
public:
  /* 
    Used inside Item_subselect::fix_fields() according to this scenario:
      > Item_subselect::fix_fields
        > engine->prepare
          > child_join->prepare
            (Here we realize we need to do the rewrite and set
             substitution= some new Item, eg. Item_in_optimizer )
          < child_join->prepare
        < engine->prepare
        *ref= substitution;
        substitution= NULL;
      < Item_subselect::fix_fields
  */
  /* TODO make this protected member again. */
  Item *substitution;
  /* engine that perform execution of subselect (single select or union) */
  /* TODO make this protected member again. */
  subselect_engine *engine;
  /* unit of subquery */
  st_select_lex_unit *unit;
  /* A reference from inside subquery predicate to somewhere outside of it */
  class Ref_to_outside : public Sql_alloc
  {
  public:
    st_select_lex *select; /* Select where the reference is pointing to */
    /* 
      What is being referred. This may be NULL when we're referring to an
      aggregate function.
    */ 
    Item *item; 
  };
  /*
    References from within this subquery to somewhere outside of it (i.e. to
    parent select, grandparent select, etc)
  */
  List<Ref_to_outside> upper_refs;
  st_select_lex *parent_select;

  /*
   TRUE<=>Table Elimination has made it redundant to evaluate this select
          (and so it is not part of QEP, etc)
  */
  bool eliminated;
  
  /* subquery is transformed */
  bool changed;

  /* TRUE <=> The underlying SELECT is correlated w.r.t some ancestor select */
  bool is_correlated; 

  enum subs_type {UNKNOWN_SUBS, SINGLEROW_SUBS,
		  EXISTS_SUBS, IN_SUBS, ALL_SUBS, ANY_SUBS};

  Item_subselect();

  virtual subs_type substype() { return UNKNOWN_SUBS; }
  bool is_in_predicate()
  {
    return (substype() == Item_subselect::IN_SUBS ||
            substype() == Item_subselect::ALL_SUBS ||
            substype() == Item_subselect::ANY_SUBS);
  }

  /*
    We need this method, because some compilers do not allow 'this'
    pointer in constructor initialization list, but we need to pass a pointer
    to subselect Item class to select_result_interceptor's constructor.
  */
  virtual void init (st_select_lex *select_lex,
		     select_result_interceptor *result);

  ~Item_subselect();
  void cleanup();
  virtual void reset()
  {
    eliminated= FALSE;
    null_value= 1;
  }
  /**
    Set the subquery result to a default value consistent with the semantics of
    the result row produced for queries with implicit grouping.
  */
  void no_rows_in_result()= 0;
  virtual bool select_transformer(JOIN *join);
  bool assigned() { return value_assigned; }
  void assigned(bool a) { value_assigned= a; }
  enum Type type() const;
  bool is_null()
  {
    update_null_value();
    return null_value;
  }
  bool fix_fields(THD *thd, Item **ref);
  bool mark_as_dependent(THD *thd, st_select_lex *select, Item *item);
  void fix_after_pullout(st_select_lex *new_parent, Item **ref);
  void recalc_used_tables(st_select_lex *new_parent, bool after_pullout);
  virtual bool exec();
  /*
    If subquery optimization or execution determines that the subquery has
    an empty result, mark the subquery predicate as a constant value.
  */
  void make_const()
  { 
    used_tables_cache= 0;
    const_item_cache= 0;
    forced_const= TRUE; 
  }
  virtual void fix_length_and_dec();
  table_map used_tables() const;
  table_map not_null_tables() const { return 0; }
  bool const_item() const;
  inline table_map get_used_tables_cache() { return used_tables_cache; }
  Item *get_tmp_table_item(THD *thd);
  void update_used_tables();
  virtual void print(String *str, enum_query_type query_type);
  virtual bool have_guarded_conds() { return FALSE; }
  bool change_engine(subselect_engine *eng)
  {
    old_engine= engine;
    engine= eng;
    return eng == 0;
  }
  bool engine_changed(subselect_engine *eng) { return engine != eng; }
  /*
    True if this subquery has been already evaluated. Implemented only for
    single select and union subqueries only.
  */
  bool is_evaluated() const;
  bool is_uncacheable() const;
  bool is_expensive();

  /*
    Used by max/min subquery to initialize value presence registration
    mechanism. Engine call this method before rexecution query.
  */
  virtual void reset_value_registration() {}
  enum_parsing_place place() { return parsing_place; }
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg);
  bool mark_as_eliminated_processor(uchar *arg);
  bool eliminate_subselect_processor(uchar *arg);
  bool set_fake_select_as_master_processor(uchar *arg);
  bool enumerate_field_refs_processor(uchar *arg);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor("subselect");
  }
  /**
    Callback to test if an IN predicate is expensive.

    @notes
    The return value affects the behavior of make_cond_for_table().

    @retval TRUE  if the predicate is expensive
    @retval FALSE otherwise
  */
  bool is_expensive_processor(uchar *arg) { return is_expensive(); }

  /**
    Get the SELECT_LEX structure associated with this Item.
    @return the SELECT_LEX structure associated with this Item
  */
  st_select_lex* get_select_lex();
  const char *func_name() const { DBUG_ASSERT(0); return "subselect"; }
  virtual bool expr_cache_is_needed(THD *);
  virtual void get_cache_parameters(List<Item> &parameters);
  virtual bool is_subquery_processor (uchar *opt_arg) { return 1; }
  bool limit_index_condition_pushdown_processor(uchar *opt_arg) 
  {
    return TRUE;
   }

  friend class select_result_interceptor;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(THD *, Item **);
  friend int  Item_field::fix_outer_field(THD *, Field **, Item **);
  friend bool Item_ref::fix_fields(THD *, Item **);
  friend void mark_select_range_as_dependent(THD*,
                                             st_select_lex*, st_select_lex*,
                                             Field*, Item*, Item_ident*);
  friend bool convert_join_subqueries_to_semijoins(JOIN *join);
};

/* single value subselect */

class Item_cache;
class Item_singlerow_subselect :public Item_subselect
{
protected:
  Item_cache *value, **row;
public:
  Item_singlerow_subselect(st_select_lex *select_lex);
  Item_singlerow_subselect() :Item_subselect(), value(0), row (0)
  {}

  void cleanup();
  subs_type substype() { return SINGLEROW_SUBS; }

  void reset();
  void no_rows_in_result();
  bool select_transformer(JOIN *join);
  void store(uint i, Item* item);
  double val_real();
  longlong val_int ();
  String *val_str (String *);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool get_date(MYSQL_TIME *ltime, ulonglong fuzzydate);
  enum Item_result result_type() const;
  enum Item_result cmp_type() const;
  enum_field_types field_type() const;
  void fix_length_and_dec();

  uint cols();
  Item* element_index(uint i) { return reinterpret_cast<Item*>(row[i]); }
  Item** addr(uint i) { return (Item**)row + i; }
  bool check_cols(uint c);
  bool null_inside();
  void bring_value();

  /**
    This method is used to implement a special case of semantic tree
    rewriting, mandated by a SQL:2003 exception in the specification.
    The only caller of this method is handle_sql2003_note184_exception(),
    see the code there for more details.
    Note that this method breaks the object internal integrity, by
    removing it's association with the corresponding SELECT_LEX,
    making this object orphan from the parse tree.
    No other method, beside the destructor, should be called on this
    object, as it is now invalid.
    @return the SELECT_LEX structure that was given in the constructor.
  */
  st_select_lex* invalidate_and_restore_select_lex();

  Item* expr_cache_insert_transformer(uchar *thd_arg);

  friend class select_singlerow_subselect;
};

/* used in static ALL/ANY optimization */
class select_max_min_finder_subselect;
class Item_maxmin_subselect :public Item_singlerow_subselect
{
protected:
  bool max;
  bool was_values;  // Set if we have found at least one row
public:
  Item_maxmin_subselect(THD *thd, Item_subselect *parent,
			st_select_lex *select_lex, bool max);
  virtual void print(String *str, enum_query_type query_type);
  void cleanup();
  bool any_value() { return was_values; }
  void register_value() { was_values= TRUE; }
  void reset_value_registration() { was_values= FALSE; }
  void no_rows_in_result();
};

/* exists subselect */

class Item_exists_subselect :public Item_subselect
{
protected:
  bool value; /* value of this item (boolean: exists/not-exists) */

  void init_length_and_dec();

public:
  Item_exists_subselect(st_select_lex *select_lex);
  Item_exists_subselect(): Item_subselect() {}

  subs_type substype() { return EXISTS_SUBS; }
  void reset() 
  {
    eliminated= FALSE;
    value= 0;
  }
  void no_rows_in_result();

  enum Item_result result_type() const { return INT_RESULT;}
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);

  Item* expr_cache_insert_transformer(uchar *thd_arg);

  friend class select_exists_subselect;
  friend class subselect_uniquesubquery_engine;
  friend class subselect_indexsubquery_engine;
};


TABLE_LIST * const NO_JOIN_NEST=(TABLE_LIST*)0x1;

/*
  Possible methods to execute an IN predicate. These are set by the optimizer
  based on user-set optimizer switches, semantic analysis and cost comparison.
*/
#define SUBS_NOT_TRANSFORMED 0 /* No execution method was chosen for this IN. */
/* The Final decision about the strategy is made. */
#define SUBS_STRATEGY_CHOSEN 1
#define SUBS_SEMI_JOIN 2       /* IN was converted to semi-join. */
#define SUBS_IN_TO_EXISTS 4    /* IN was converted to correlated EXISTS. */
#define SUBS_MATERIALIZATION 8 /* Execute IN via subquery materialization. */
/* Partial matching substrategies of MATERIALIZATION. */
#define SUBS_PARTIAL_MATCH_ROWID_MERGE 16
#define SUBS_PARTIAL_MATCH_TABLE_SCAN 32
/* ALL/ANY will be transformed with max/min optimization */
/*   The subquery has not aggregates, transform it into a MAX/MIN query. */
#define SUBS_MAXMIN_INJECTED 64
/*   The subquery has aggregates, use a special max/min subselect engine. */
#define SUBS_MAXMIN_ENGINE 128


/**
  Representation of IN subquery predicates of the form
  "left_expr IN (SELECT ...)".

  @details
  This class has: 
   - A "subquery execution engine" (as a subclass of Item_subselect) that allows
     it to evaluate subqueries. (and this class participates in execution by
     having was_null variable where part of execution result is stored.
   - Transformation methods (todo: more on this).

  This class is not used directly, it is "wrapped" into Item_in_optimizer
  which provides some small bits of subquery evaluation.
*/

class Item_in_subselect :public Item_exists_subselect
{
protected:
  /*
    Cache of the left operand of the subquery predicate. Allocated in the
    runtime memory root, for each execution, thus need not be freed.
  */
  List<Cached_item> *left_expr_cache;
  bool first_execution;

  /*
    expr & optimizer used in subselect rewriting to store Item for
    all JOIN in UNION
  */
  Item *expr;
  bool was_null;
  bool abort_on_null;
  /* A bitmap of possible execution strategies for an IN predicate. */
  uchar in_strategy;
public:
  Item_in_optimizer *optimizer;
protected:
  /* Used to trigger on/off conditions that were pushed down to subselect */
  bool *pushed_cond_guards;
  Comp_creator *func;

protected:
  bool init_cond_guards();
  bool select_in_like_transformer(JOIN *join);
  bool single_value_transformer(JOIN *join);
  bool row_value_transformer(JOIN * join);
  bool fix_having(Item *having, st_select_lex *select_lex);
  bool create_single_in_to_exists_cond(JOIN * join,
                                       Item **where_item,
                                       Item **having_item);
  bool create_row_in_to_exists_cond(JOIN * join,
                                    Item **where_item,
                                    Item **having_item);
public:
  Item *left_expr;
  /* Priority of this predicate in the convert-to-semi-join-nest process. */
  int sj_convert_priority;
  /*
    Used by subquery optimizations to keep track about in which clause this
    subquery predicate is located: 
      NO_JOIN_NEST      - the predicate is an AND-part of the WHERE
      join nest pointer - the predicate is an AND-part of ON expression
                          of a join nest   
      NULL              - for all other locations
  */
  TABLE_LIST *emb_on_expr_nest;
  /*
    Types of left_expr and subquery's select list allow to perform subquery
    materialization. Currently, we set this to FALSE when it as well could
    be TRUE. This is to be properly addressed with fix for BUG#36752.
  */
  bool types_allow_materialization;

  /* 
    Same as above, but they also allow to scan the materialized table. 
  */
  bool sjm_scan_allowed;

  /* 
    JoinTaB Materialization (JTBM) members
  */
  
  /* 
    TRUE <=> This subselect has been converted into non-mergeable semi-join
    table.
  */
  bool is_jtbm_merged;
  
  /* (Applicable if is_jtbm_merged==TRUE) Time required to run the materialized join */
  double jtbm_read_time;

  /* (Applicable if is_jtbm_merged==TRUE) Number of output rows in materialized join */
  double jtbm_record_count;   
  
  /*
    (Applicable if is_jtbm_merged==TRUE) TRUE <=> The materialized subselect is
    a degenerate subselect which produces 0 or 1 rows, which we know at
    optimization phase.
    Examples:
    1. subquery has "Impossible WHERE": 

      SELECT * FROM ot WHERE ot.column IN (SELECT it.col FROM it WHERE 2 > 3)
    
    2. Subquery produces one row which opt_sum.cc is able to get with one lookup:

      SELECT * FROM ot WHERE ot.column IN (SELECT MAX(it.key_col) FROM it)
  */
  bool is_jtbm_const_tab;
  
  /* 
    (Applicable if is_jtbm_const_tab==TRUE) Whether the subquery has produced 
     the row (or not)
  */
  bool jtbm_const_row_found;
  
  /*
    TRUE<=>this is a flattenable semi-join, false overwise.
  */
  bool is_flattenable_semijoin;

  /*
    TRUE<=>registered in the list of semijoins in outer select
  */
  bool is_registered_semijoin;
  
  /*
    Used to determine how this subselect item is represented in the item tree,
    in case there is a need to locate it there and replace with something else.
    Two options are possible:
      1. This item is there 'as-is'.
      1. This item is wrapped within Item_in_optimizer.
  */
  Item *original_item()
  {
    return is_flattenable_semijoin ? (Item*)this : (Item*)optimizer;
  }
  
  bool *get_cond_guard(int i)
  {
    return pushed_cond_guards ? pushed_cond_guards + i : NULL;
  }
  void set_cond_guard_var(int i, bool v) 
  { 
    if ( pushed_cond_guards)
      pushed_cond_guards[i]= v;
  }
  bool have_guarded_conds() { return test(pushed_cond_guards); }

  Item_func_not_all *upper_item; // point on NOT/NOP before ALL/SOME subquery

  Item_in_subselect(Item * left_expr, st_select_lex *select_lex);
  Item_in_subselect()
    :Item_exists_subselect(), left_expr_cache(0), first_execution(TRUE),
     abort_on_null(0), in_strategy(SUBS_NOT_TRANSFORMED), optimizer(0),
    pushed_cond_guards(NULL), func(NULL), emb_on_expr_nest(NULL), 
    is_jtbm_merged(FALSE), is_jtbm_const_tab(FALSE),
    upper_item(0)
    {}
  void cleanup();
  subs_type substype() { return IN_SUBS; }
  void reset() 
  {
    eliminated= FALSE;
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  bool select_transformer(JOIN *join);
  bool create_in_to_exists_cond(JOIN *join_arg);
  bool inject_in_to_exists_cond(JOIN *join_arg);

  virtual bool exec();
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  void update_null_value () { (void) val_bool(); }
  bool val_bool();
  void top_level_item() { abort_on_null=1; }
  inline bool is_top_level_item() { return abort_on_null; }
  bool test_limit(st_select_lex_unit *unit);
  virtual void print(String *str, enum_query_type query_type);
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  void fix_after_pullout(st_select_lex *new_parent, Item **ref);
  bool const_item() const
  {
    return Item_subselect::const_item() && left_expr->const_item();
  }
  void update_used_tables();
  bool setup_mat_engine();
  bool init_left_expr_cache();
  /* Inform 'this' that it was computed, and contains a valid result. */
  void set_first_execution() { if (first_execution) first_execution= FALSE; }
  bool expr_cache_is_needed(THD *thd);
  inline bool left_expr_has_null();
  
  int optimize(double *out_rows, double *cost);
  /* 
    Return the identifier that we could use to identify the subquery for the
    user.
  */
  int get_identifier();

  void mark_as_condition_AND_part(TABLE_LIST *embedding)
  {
    emb_on_expr_nest= embedding;
  }

  bool test_strategy(uchar strategy)
  { return test(in_strategy & strategy); }

  /**
    Test that the IN strategy was chosen for execution. This is so
    when the CHOSEN flag is ON, and there is no other strategy.
  */
  bool test_set_strategy(uchar strategy)
  {
    DBUG_ASSERT(strategy == SUBS_SEMI_JOIN ||
                strategy == SUBS_IN_TO_EXISTS ||
                strategy == SUBS_MATERIALIZATION ||
                strategy == SUBS_PARTIAL_MATCH_ROWID_MERGE ||
                strategy == SUBS_PARTIAL_MATCH_TABLE_SCAN ||
                strategy == SUBS_MAXMIN_INJECTED ||
                strategy == SUBS_MAXMIN_ENGINE);
    return ((in_strategy & SUBS_STRATEGY_CHOSEN) &&
            (in_strategy & ~SUBS_STRATEGY_CHOSEN) == strategy);
  }

  bool is_set_strategy()
  { return test(in_strategy & SUBS_STRATEGY_CHOSEN); }

  bool has_strategy()
  { return in_strategy != SUBS_NOT_TRANSFORMED; }

  void add_strategy (uchar strategy)
  {
    DBUG_ASSERT(strategy != SUBS_NOT_TRANSFORMED);
    DBUG_ASSERT(!(strategy & SUBS_STRATEGY_CHOSEN));
    /*
      TODO: PS re-execution breaks this condition, because
      check_and_do_in_subquery_rewrites() is called for each reexecution
      and re-adds the same strategies.
      DBUG_ASSERT(!(in_strategy & SUBS_STRATEGY_CHOSEN));
    */
    in_strategy|= strategy;
  }

  void reset_strategy(uchar strategy)
  {
    DBUG_ASSERT(strategy != SUBS_NOT_TRANSFORMED);
    in_strategy= strategy;
  }

  void set_strategy(uchar strategy)
  {
    /* Check that only one strategy is set for execution. */
    DBUG_ASSERT(strategy == SUBS_SEMI_JOIN ||
                strategy == SUBS_IN_TO_EXISTS ||
                strategy == SUBS_MATERIALIZATION ||
                strategy == SUBS_PARTIAL_MATCH_ROWID_MERGE ||
                strategy == SUBS_PARTIAL_MATCH_TABLE_SCAN ||
                strategy == SUBS_MAXMIN_INJECTED ||
                strategy == SUBS_MAXMIN_ENGINE);
    in_strategy= (SUBS_STRATEGY_CHOSEN | strategy);
  }

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class Item_in_optimizer;
  friend class subselect_indexsubquery_engine;
  friend class subselect_hash_sj_engine;
  friend class subselect_partial_match_engine;
};


/* ALL/ANY/SOME subselect */
class Item_allany_subselect :public Item_in_subselect
{
public:
  chooser_compare_func_creator func_creator;
  bool all;

  Item_allany_subselect(Item * left_expr, chooser_compare_func_creator fc,
                        st_select_lex *select_lex, bool all);

  void cleanup();
  // only ALL subquery has upper not
  subs_type substype() { return all?ALL_SUBS:ANY_SUBS; }
  bool select_transformer(JOIN *join);
  void create_comp_func(bool invert) { func= func_creator(invert); }
  virtual void print(String *str, enum_query_type query_type);
  bool is_maxmin_applicable(JOIN *join);
  bool transform_into_max_min(JOIN *join);
  void no_rows_in_result();
};


class subselect_engine: public Sql_alloc
{
protected:
  select_result_interceptor *result; /* results storage class */
  THD *thd; /* pointer to current THD */
  Item_subselect *item; /* item, that use this engine */
  enum Item_result res_type; /* type of results */
  enum Item_result cmp_type; /* how to compare the results */
  enum_field_types res_field_type; /* column type of the results */
  bool maybe_null; /* may be null (first item in select) */
public:

  enum enum_engine_type {ABSTRACT_ENGINE, SINGLE_SELECT_ENGINE,
                         UNION_ENGINE, UNIQUESUBQUERY_ENGINE,
                         INDEXSUBQUERY_ENGINE, HASH_SJ_ENGINE,
                         ROWID_MERGE_ENGINE, TABLE_SCAN_ENGINE};

  subselect_engine(THD *thd_arg, Item_subselect *si,
                   select_result_interceptor *res)
  {
    result= res;
    item= si;
    cmp_type= res_type= STRING_RESULT;
    res_field_type= MYSQL_TYPE_VAR_STRING;
    maybe_null= 0;
    set_thd(thd_arg);
  }
  virtual ~subselect_engine() {}; // to satisfy compiler
  virtual void cleanup()= 0;

  /*
    Also sets "thd" for subselect_engine::result.
    Should be called before prepare().
  */
  void set_thd(THD *thd_arg);
  THD * get_thd() { return thd; }
  virtual int prepare()= 0;
  virtual void fix_length_and_dec(Item_cache** row)= 0;
  /*
    Execute the engine

    SYNOPSIS
      exec()

    DESCRIPTION
      Execute the engine. The result of execution is subquery value that is
      either captured by previously set up select_result-based 'sink' or
      stored somewhere by the exec() method itself.

      A required side effect: If at least one pushed-down predicate is
      disabled, subselect_engine->no_rows() must return correct result after 
      the exec() call.

    RETURN
      0 - OK
      1 - Either an execution error, or the engine was "changed", and the
          caller should call exec() again for the new engine.
  */
  virtual int exec()= 0;
  virtual uint cols()= 0; /* return number of columns in select */
  virtual uint8 uncacheable()= 0; /* query is uncacheable */
  enum Item_result type() { return res_type; }
  enum Item_result cmptype() { return cmp_type; }
  enum_field_types field_type() { return res_field_type; }
  virtual void exclude()= 0;
  virtual bool may_be_null() { return maybe_null; };
  virtual table_map upper_select_const_tables()= 0;
  static table_map calc_const_tables(TABLE_LIST *);
  static table_map calc_const_tables(List<TABLE_LIST> &list);
  virtual void print(String *str, enum_query_type query_type)= 0;
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result,
                             bool temp= FALSE)= 0;
  virtual bool no_tables()= 0;
  virtual bool is_executed() const { return FALSE; }
  /* Check if subquery produced any rows during last query execution */
  virtual bool no_rows() = 0;
  virtual enum_engine_type engine_type() { return ABSTRACT_ENGINE; }
  virtual int get_identifier() { DBUG_ASSERT(0); return 0; }
protected:
  void set_row(List<Item> &item_list, Item_cache **row);
};


class subselect_single_select_engine: public subselect_engine
{
  bool prepared;       /* simple subselect is prepared */
  bool executed;       /* simple subselect is executed */
  bool optimize_error; /* simple subselect optimization failed */
  st_select_lex *select_lex; /* corresponding select_lex */
  JOIN * join; /* corresponding JOIN structure */
public:
  subselect_single_select_engine(THD *thd_arg, st_select_lex *select,
				 select_result_interceptor *result,
				 Item_subselect *item);
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols();
  uint8 uncacheable();
  void exclude();
  table_map upper_select_const_tables();
  virtual void print (String *str, enum_query_type query_type);
  bool change_result(Item_subselect *si,
                     select_result_interceptor *result,
                     bool temp);
  bool no_tables();
  bool may_be_null();
  bool is_executed() const { return executed; }
  bool no_rows();
  virtual enum_engine_type engine_type() { return SINGLE_SELECT_ENGINE; }
  int get_identifier();

  friend class subselect_hash_sj_engine;
  friend class Item_in_subselect;
  friend bool setup_jtbm_semi_joins(JOIN *join, List<TABLE_LIST> *join_list,
                                    Item **join_where);

};


class subselect_union_engine: public subselect_engine
{
  st_select_lex_unit *unit;  /* corresponding unit structure */
public:
  subselect_union_engine(THD *thd_arg, st_select_lex_unit *u,
			 select_result_interceptor *result,
			 Item_subselect *item);
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols();
  uint8 uncacheable();
  void exclude();
  table_map upper_select_const_tables();
  virtual void print (String *str, enum_query_type query_type);
  bool change_result(Item_subselect *si,
                     select_result_interceptor *result,
                     bool temp= FALSE);
  bool no_tables();
  bool is_executed() const;
  bool no_rows();
  virtual enum_engine_type engine_type() { return UNION_ENGINE; }
};


struct st_join_table;


/*
  A subquery execution engine that evaluates the subquery by doing one index
  lookup in a unique index.

  This engine is used to resolve subqueries in forms
  
    outer_expr IN (SELECT tbl.unique_key FROM tbl WHERE subq_where) 
    
  or, tuple-based:
  
    (oe1, .. oeN) IN (SELECT uniq_key_part1, ... uniq_key_partK
                      FROM tbl WHERE subqwhere) 
  
  i.e. the subquery is a single table SELECT without GROUP BY, aggregate
  functions, etc.
*/

class subselect_uniquesubquery_engine: public subselect_engine
{
protected:
  st_join_table *tab;
  Item *cond; /* The WHERE condition of subselect */
  /* 
    TRUE<=> last execution produced empty set. Valid only when left
    expression is NULL.
  */
  bool empty_result_set;
public:

  // constructor can assign THD because it will be called after JOIN::prepare
  subselect_uniquesubquery_engine(THD *thd_arg, st_join_table *tab_arg,
				  Item_subselect *subs, Item *where)
    :subselect_engine(thd_arg, subs, 0), tab(tab_arg), cond(where)
  {}
  ~subselect_uniquesubquery_engine();
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols() { return 1; }
  uint8 uncacheable() { return UNCACHEABLE_DEPENDENT_INJECTED; }
  void exclude();
  table_map upper_select_const_tables() { return 0; }
  virtual void print (String *str, enum_query_type query_type);
  bool change_result(Item_subselect *si,
                     select_result_interceptor *result,
                     bool temp= FALSE);
  bool no_tables();
  int index_lookup(); /* TIMOUR: this method needs refactoring. */
  int scan_table();
  bool copy_ref_key(bool skip_constants);
  bool no_rows() { return empty_result_set; }
  virtual enum_engine_type engine_type() { return UNIQUESUBQUERY_ENGINE; }
};


class subselect_indexsubquery_engine: public subselect_uniquesubquery_engine
{
  /* FALSE for 'ref', TRUE for 'ref-or-null'. */
  bool check_null;
  /* 
    The "having" clause. This clause (further reffered to as "artificial
    having") was inserted by subquery transformation code. It contains 
    Item(s) that have a side-effect: they record whether the subquery has 
    produced a row with NULL certain components. We need to use it for cases
    like
      (oe1, oe2) IN (SELECT t.key, t.no_key FROM t1)
    where we do index lookup on t.key=oe1 but need also to check if there
    was a row such that t.no_key IS NULL.
    
    NOTE: This is currently here and not in the uniquesubquery_engine. Ideally
    it should have been in uniquesubquery_engine in order to allow execution of
    subqueries like
    
      (oe1, oe2) IN (SELECT primary_key, non_key_maybe_null_field FROM tbl)

    We could use uniquesubquery_engine for the first component and let
    Item_is_not_null_test( non_key_maybe_null_field) to handle the second.

    However, subqueries like the above are currently not handled by index
    lookup-based subquery engines, the engine applicability check misses
    them: it doesn't switch the engine for case of artificial having and
    [eq_]ref access (only for artifical having + ref_or_null or no having).
    The above example subquery is handled as a full-blown SELECT with eq_ref
    access to one table.

    Due to this limitation, the "artificial having" currently needs to be 
    checked by only in indexsubquery_engine.
  */
  Item *having;
public:

  // constructor can assign THD because it will be called after JOIN::prepare
  subselect_indexsubquery_engine(THD *thd_arg, st_join_table *tab_arg,
				 Item_subselect *subs, Item *where,
                                 Item *having_arg, bool chk_null)
    :subselect_uniquesubquery_engine(thd_arg, tab_arg, subs, where),
     check_null(chk_null),
     having(having_arg)
  {}
  int exec();
  virtual void print (String *str, enum_query_type query_type);
  virtual enum_engine_type engine_type() { return INDEXSUBQUERY_ENGINE; }
};

/*
  This function is actually defined in sql_parse.cc, but it depends on
  chooser_compare_func_creator defined in this file.
 */
Item * all_any_subquery_creator(Item *left_expr,
                                chooser_compare_func_creator cmp,
                                bool all,
                                SELECT_LEX *select_lex);


inline bool Item_subselect::is_evaluated() const
{
  return engine->is_executed();
}


inline bool Item_subselect::is_uncacheable() const
{
  return engine->uncacheable();
}

/**
  Compute an IN predicate via a hash semi-join. This class is responsible for
  the materialization of the subquery, and the selection of the correct and
  optimal execution method (e.g. direct index lookup, or partial matching) for
  the IN predicate.
*/

class subselect_hash_sj_engine : public subselect_engine
{
public:
  /* The table into which the subquery is materialized. */
  TABLE *tmp_table;
  /* TRUE if the subquery was materialized into a temp table. */
  bool is_materialized;
  /*
    The old engine already chosen at parse time and stored in permanent memory.
    Through this member we can re-create and re-prepare materialize_join for
    each execution of a prepared statement. We also reuse the functionality
    of subselect_single_select_engine::[prepare | cols].
  */
  subselect_single_select_engine *materialize_engine;
  /*
    QEP to execute the subquery and materialize its result into a
    temporary table. Created during the first call to exec().
  */
  JOIN *materialize_join;
  /*
    A conjunction of all the equality condtions between all pairs of expressions
    that are arguments of an IN predicate. We need these to post-filter some
    IN results because index lookups sometimes match values that are actually
    not equal to the search key in SQL terms.
  */
  Item_cond_and *semi_join_conds;
  Name_resolution_context *semi_join_conds_context;


  subselect_hash_sj_engine(THD *thd, Item_subselect *in_predicate,
                           subselect_single_select_engine *old_engine)
    : subselect_engine(thd, in_predicate, NULL), 
      tmp_table(NULL), is_materialized(FALSE), materialize_engine(old_engine),
      materialize_join(NULL),  semi_join_conds(NULL), lookup_engine(NULL),
      count_partial_match_columns(0), count_null_only_columns(0),
      count_columns_with_nulls(0), strategy(UNDEFINED)
  {}
  ~subselect_hash_sj_engine();

  bool init(List<Item> *tmp_columns, uint subquery_id);
  void cleanup();
  int prepare();
  int exec();
  virtual void print(String *str, enum_query_type query_type);
  uint cols()
  {
    return materialize_engine->cols();
  }
  uint8 uncacheable() { return materialize_engine->uncacheable(); }
  table_map upper_select_const_tables() { return 0; }
  bool no_rows() { return !tmp_table->file->stats.records; }
  virtual enum_engine_type engine_type() { return HASH_SJ_ENGINE; }
  /*
    TODO: factor out all these methods in a base subselect_index_engine class
    because all of them have dummy implementations and should never be called.
  */
  void fix_length_and_dec(Item_cache** row);//=>base class
  void exclude(); //=>base class
  //=>base class
  bool change_result(Item_subselect *si,
                     select_result_interceptor *result,
                     bool temp= FALSE);
  bool no_tables();//=>base class

protected:
  /* The engine used to compute the IN predicate. */
  subselect_engine *lookup_engine;
  /* Keyparts of the only non-NULL composite index in a rowid merge. */
  MY_BITMAP non_null_key_parts;
  /* Keyparts of the single column indexes with NULL, one keypart per index. */
  MY_BITMAP partial_match_key_parts;
  uint count_partial_match_columns;
  uint count_null_only_columns;
  uint count_columns_with_nulls;
  /* Possible execution strategies that can be used to compute hash semi-join.*/
  enum exec_strategy {
    UNDEFINED,
    COMPLETE_MATCH, /* Use regular index lookups. */
    PARTIAL_MATCH,  /* Use some partial matching strategy. */
    PARTIAL_MATCH_MERGE, /* Use partial matching through index merging. */
    PARTIAL_MATCH_SCAN,  /* Use partial matching through table scan. */
    IMPOSSIBLE      /* Subquery materialization is not applicable. */
  };
  /* The chosen execution strategy. Computed after materialization. */
  exec_strategy strategy;
  exec_strategy get_strategy_using_schema();
  exec_strategy get_strategy_using_data();
  ulonglong rowid_merge_buff_size(bool has_non_null_key,
                                  bool has_covering_null_row,
                                  MY_BITMAP *partial_match_key_parts);
  void choose_partial_match_strategy(bool has_non_null_key,
                                     bool has_covering_null_row,
                                     MY_BITMAP *partial_match_key_parts);
  bool make_semi_join_conds();
  subselect_uniquesubquery_engine* make_unique_engine();

};


/*
  Distinguish the type of (0-based) row numbers from the type of the index into
  an array of row numbers.
*/
typedef ha_rows rownum_t;


/*
  An Ordered_key is an in-memory table index that allows O(log(N)) time
  lookups of a multi-part key.

  If the index is over a single column, then this column may contain NULLs, and
  the NULLs are stored and tested separately for NULL in O(1) via is_null().
  Multi-part indexes assume that the indexed columns do not contain NULLs.

  TODO:
  = Due to the unnatural assymetry between single and multi-part indexes, it
    makes sense to somehow refactor or extend the class.

  = This class can be refactored into a base abstract interface, and two
    subclasses:
    - one to represent single-column indexes, and
    - another to represent multi-column indexes.
    Such separation would allow slightly more efficient implementation of
    the single-column indexes.
  = The current design requires such indexes to be fully recreated for each
    PS (re)execution, however most of the comprising objects can be reused.
*/

class Ordered_key : public Sql_alloc
{
protected:
  /*
    Index of the key in an array of keys. This index allows to
    construct (sub)sets of keys represented by bitmaps.
  */
  uint keyid;
  /* The table being indexed. */
  TABLE *tbl;
  /* The columns being indexed. */
  Item_field **key_columns;
  /* Number of elements in 'key_columns' (number of key parts). */
  uint key_column_count;
  /*
    An expression, or sequence of expressions that forms the search key.
    The search key is a sequence when it is Item_row. Each element of the
    sequence is accessible via Item::element_index(int i).
  */
  Item *search_key;

/* Value index related members. */
  /*
    The actual value index, consists of a sorted sequence of row numbers.
  */
  rownum_t *key_buff;
  /* Number of elements in key_buff. */
  ha_rows key_buff_elements;
  /* Current element in 'key_buff'. */
  ha_rows cur_key_idx;
  /*
    Mapping from row numbers to row ids. The element row_num_to_rowid[i]
    contains a buffer with the rowid for the row numbered 'i'.
    The memory for this member is not maintanined by this class because
    all Ordered_key indexes of the same table share the same mapping.
  */
  uchar *row_num_to_rowid;
  /*
    A sequence of predicates to compare the search key with the corresponding
    columns of a table row from the index.
  */
  Item_func_lt **compare_pred;

/* Null index related members. */
  MY_BITMAP null_key;
  /* Count of NULLs per column. */
  ha_rows null_count;
  /* The row number that contains the first NULL in a column. */
  rownum_t min_null_row;
  /* The row number that contains the last NULL in a column. */
  rownum_t max_null_row;

protected:
  bool alloc_keys_buffers();
  /*
    Quick sort comparison function that compares two rows of the same table
    indentfied with their row numbers.
  */
  int cmp_keys_by_row_data(rownum_t a, rownum_t b);
  static int cmp_keys_by_row_data_and_rownum(Ordered_key *key,
                                             rownum_t* a, rownum_t* b);

  int cmp_key_with_search_key(rownum_t row_num);

public:
  Ordered_key(uint keyid_arg, TABLE *tbl_arg,
              Item *search_key_arg, ha_rows null_count_arg,
              ha_rows min_null_row_arg, ha_rows max_null_row_arg,
              uchar *row_num_to_rowid_arg);
  ~Ordered_key();
  void cleanup();
  /* Initialize a multi-column index. */
  bool init(MY_BITMAP *columns_to_index);
  /* Initialize a single-column index. */
  bool init(int col_idx);

  uint get_column_count() { return key_column_count; }
  uint get_keyid() { return keyid; }
  uint get_field_idx(uint i)
  {
    DBUG_ASSERT(i < key_column_count);
    return key_columns[i]->field->field_index;
  }
  rownum_t get_min_null_row() { return min_null_row; }
  rownum_t get_max_null_row() { return max_null_row; }
  MY_BITMAP * get_null_key() { return &null_key; }
  ha_rows get_null_count() { return null_count; }
  /*
    Get the search key element that corresponds to the i-th key part of this
    index.
  */
  Item *get_search_key(uint i)
  {
    return search_key->element_index(key_columns[i]->field->field_index);
  }
  void add_key(rownum_t row_num)
  {
    /* The caller must know how many elements to add. */
    DBUG_ASSERT(key_buff_elements && cur_key_idx < key_buff_elements);
    key_buff[cur_key_idx]= row_num;
    ++cur_key_idx;
  }

  void sort_keys();
  double null_selectivity();

  /*
    Position the current element at the first row that matches the key.
    The key itself is propagated by evaluating the current value(s) of
    this->search_key.
  */
  bool lookup();
  /* Move the current index cursor to the first key. */
  void first()
  {
    DBUG_ASSERT(key_buff_elements);
    cur_key_idx= 0;
  }
  /* TODO */
  bool next_same();
  /* Move the current index cursor to the next key. */
  bool next()
  {
    DBUG_ASSERT(key_buff_elements);
    if (cur_key_idx < key_buff_elements - 1)
    {
      ++cur_key_idx;
      return TRUE;
    }
    return FALSE;
  };
  /* Return the current index element. */
  rownum_t current()
  {
    DBUG_ASSERT(key_buff_elements && cur_key_idx < key_buff_elements);
    return key_buff[cur_key_idx];
  }

  void set_null(rownum_t row_num)
  {
    bitmap_set_bit(&null_key, (uint)row_num);
  }
  bool is_null(rownum_t row_num)
  {
    /*
      Indexes consisting of only NULLs do not have a bitmap buffer at all.
      Their only initialized member is 'n_bits', which is equal to the number
      of temp table rows.
    */
    if (null_count == tbl->file->stats.records)
    {
      DBUG_ASSERT(tbl->file->stats.records == null_key.n_bits);
      return TRUE;
    }
    if (row_num > max_null_row || row_num < min_null_row)
      return FALSE;
    return bitmap_is_set(&null_key, (uint)row_num);
  }
  void print(String *str);
};


class subselect_partial_match_engine : public subselect_engine
{
protected:
  /* The temporary table that contains a materialized subquery. */
  TABLE *tmp_table;
  /*
    The engine used to check whether an IN predicate is TRUE or not. If not
    TRUE, then subselect_rowid_merge_engine further distinguishes between
    FALSE and UNKNOWN.
  */
  subselect_uniquesubquery_engine *lookup_engine;
  /* A list of equalities between each pair of IN operands. */
  List<Item> *equi_join_conds;
  /*
    True if there is an all NULL row in tmp_table. If so, then if there is
    no complete match, there is a guaranteed partial match.
  */
  bool has_covering_null_row;

  /*
    True if all nullable columns of tmp_table consist of only NULL values.
    If so, then if there is a match in the non-null columns, there is a
    guaranteed partial match.
  */
  bool has_covering_null_columns;
  uint count_columns_with_nulls;

protected:
  virtual bool partial_match()= 0;
public:
  subselect_partial_match_engine(THD *thd_arg,
                                 subselect_uniquesubquery_engine *engine_arg,
                                 TABLE *tmp_table_arg, Item_subselect *item_arg,
                                 select_result_interceptor *result_arg,
                                 List<Item> *equi_join_conds_arg,
                                 bool has_covering_null_row_arg,
                                 bool has_covering_null_columns_arg,
                                 uint count_columns_with_nulls_arg);
  int prepare() { return 0; }
  int exec();
  void fix_length_and_dec(Item_cache**) {}
  uint cols() { /* TODO: what is the correct value? */ return 1; }
  uint8 uncacheable() { return UNCACHEABLE_DEPENDENT; }
  void exclude() {}
  table_map upper_select_const_tables() { return 0; }
  bool change_result(Item_subselect*,
                     select_result_interceptor*,
                     bool temp= FALSE)
  { DBUG_ASSERT(FALSE); return false; }
  bool no_tables() { return false; }
  bool no_rows()
  {
    /*
      TODO: It is completely unclear what is the semantics of this
      method. The current result is computed so that the call to no_rows()
      from Item_in_optimizer::val_int() sets Item_in_optimizer::null_value
      correctly.
    */
    return !(((Item_in_subselect *) item)->null_value);
  }
  void print(String*, enum_query_type);

  friend void subselect_hash_sj_engine::cleanup();
};


class subselect_rowid_merge_engine: public subselect_partial_match_engine
{
protected:
  /*
    Mapping from row numbers to row ids. The rowids are stored sequentially
    in the array - rowid[i] is located in row_num_to_rowid + i * rowid_length.
  */
  uchar *row_num_to_rowid;
  /*
    A subset of all the keys for which there is a match for the same row.
    Used during execution. Computed for each outer reference
  */
  MY_BITMAP matching_keys;
  /*
    The columns of the outer reference that are NULL. Computed for each
    outer reference.
  */
  MY_BITMAP matching_outer_cols;
  /*
    Indexes of row numbers, sorted by <column_value, row_number>. If an
    index may contain NULLs, the NULLs are stored efficiently in a bitmap.

    The indexes are sorted by the selectivity of their NULL sub-indexes, the
    one with the fewer NULLs is first. Thus, if there is any index on
    non-NULL columns, it is contained in keys[0].
  */
  Ordered_key **merge_keys;
  /* The number of elements in merge_keys. */
  uint merge_keys_count;
  /* The NULL bitmaps of merge keys.*/
  MY_BITMAP   **null_bitmaps;
  /*
    An index on all non-NULL columns of 'tmp_table'. The index has the
    logical form: <[v_i1 | ... | v_ik], rownum>. It allows to find the row
    number where the columns c_i1,...,c1_k contain the values v_i1,...,v_ik.
    If such an index exists, it is always the first element of 'merge_keys'.
  */
  Ordered_key *non_null_key;
  /*
    Priority queue of Ordered_key indexes, one per NULLable column.
    This queue is used by the partial match algorithm in method exec().
  */
  QUEUE pq;
protected:
  /*
    Comparison function to compare keys in order of decreasing bitmap
    selectivity.
  */
  static int cmp_keys_by_null_selectivity(Ordered_key **k1, Ordered_key **k2);
  /*
    Comparison function used by the priority queue pq, the 'smaller' key
    is the one with the smaller current row number.
  */
  static int cmp_keys_by_cur_rownum(void *arg, uchar *k1, uchar *k2);

  bool test_null_row(rownum_t row_num);
  bool exists_complementing_null_row(MY_BITMAP *keys_to_complement);
  bool partial_match();
public:
  subselect_rowid_merge_engine(THD *thd_arg,
                               subselect_uniquesubquery_engine *engine_arg,
                               TABLE *tmp_table_arg, uint merge_keys_count_arg,
                               bool has_covering_null_row_arg,
                               bool has_covering_null_columns_arg,
                               uint count_columns_with_nulls_arg,
                               Item_subselect *item_arg,
                               select_result_interceptor *result_arg,
                               List<Item> *equi_join_conds_arg)
    :subselect_partial_match_engine(thd_arg, engine_arg, tmp_table_arg,
                                    item_arg, result_arg, equi_join_conds_arg,
                                    has_covering_null_row_arg,
                                    has_covering_null_columns_arg,
                                    count_columns_with_nulls_arg),
    merge_keys_count(merge_keys_count_arg), non_null_key(NULL)
  {}
  ~subselect_rowid_merge_engine();
  bool init(MY_BITMAP *non_null_key_parts, MY_BITMAP *partial_match_key_parts);
  void cleanup();
  virtual enum_engine_type engine_type() { return ROWID_MERGE_ENGINE; }
};


class subselect_table_scan_engine: public subselect_partial_match_engine
{
protected:
  bool partial_match();
public:
  subselect_table_scan_engine(THD *thd_arg,
                              subselect_uniquesubquery_engine *engine_arg,
                              TABLE *tmp_table_arg, Item_subselect *item_arg,
                              select_result_interceptor *result_arg,
                              List<Item> *equi_join_conds_arg,
                              bool has_covering_null_row_arg,
                              bool has_covering_null_columns_arg,
                              uint count_columns_with_nulls_arg);
  void cleanup();
  virtual enum_engine_type engine_type() { return TABLE_SCAN_ENGINE; }
};
#endif /* ITEM_SUBSELECT_INCLUDED */

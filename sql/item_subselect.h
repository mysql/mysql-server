#ifndef ITEM_SUBSELECT_INCLUDED
#define ITEM_SUBSELECT_INCLUDED

/* Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/* subselect Item */

class st_select_lex;
class st_select_lex_unit;
class JOIN;
class select_result_interceptor;
class subselect_engine;
class subselect_hash_sj_engine;
class Item_bool_func2;
class Cached_item;
class Comp_creator;

typedef class st_select_lex SELECT_LEX;

/**
  Convenience typedef used in this file, and further used by any files
  including this file.
*/
typedef Comp_creator* (*chooser_compare_func_creator)(bool invert);

/* base class for subselects */

class Item_subselect :public Item_result_field
{
private:
  bool value_assigned; /* value already assigned to subselect */
  /**
      Whether or not execution of this subselect has been traced by
      optimizer tracing already. If optimizer trace option
      REPEATED_SUBSELECT is disabled, this is used to disable tracing
      after the first one.
  */
  bool traced_before;
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
      < Item_subselect::fix_fields
  */
  Item *substitution;
public:
  /* unit of subquery */
  st_select_lex_unit *unit;
  /**
     If !=INT_MIN: this Item is in the condition attached to the JOIN_TAB
     having this index in the parent JOIN.
  */
  int in_cond_of_tab;

  /// EXPLAIN needs read-only access to the engine
  const subselect_engine *get_engine_for_explain() const { return engine; }

protected:
  /* engine that perform execution of subselect (single select or union) */
  subselect_engine *engine;
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

public:
  /* changed engine indicator */
  bool engine_changed;
  /* subquery is transformed */
  bool changed;

  enum trans_res {RES_OK, RES_REDUCE, RES_ERROR};
  enum subs_type {UNKNOWN_SUBS, SINGLEROW_SUBS,
		  EXISTS_SUBS, IN_SUBS, ALL_SUBS, ANY_SUBS};

  Item_subselect();

  virtual subs_type substype() { return UNKNOWN_SUBS; }

  /*
    We need this method, because some compilers do not allow 'this'
    pointer in constructor initialization list, but we need to pass a pointer
    to subselect Item class to select_result_interceptor's constructor.
  */
  virtual void init (st_select_lex *select_lex,
		     select_result_interceptor *result);

  ~Item_subselect();
  virtual void cleanup();
  virtual void reset()
  {
    null_value= 1;
  }
  virtual trans_res select_transformer(JOIN *join);
  bool assigned() const { return value_assigned; }
  void assigned(bool a) { value_assigned= a; }
  enum Type type() const;
  bool is_null()
  {
    update_null_value();
    return null_value;
  }
  bool fix_fields(THD *thd, Item **ref);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  virtual bool exec();
  virtual void fix_length_and_dec();
  table_map used_tables() const;
  table_map not_null_tables() const { return 0; }
  bool const_item() const;
  inline table_map get_used_tables_cache() { return used_tables_cache; }
  inline bool get_const_item_cache() { return const_item_cache; }
  Item *get_tmp_table_item(THD *thd);
  void update_used_tables();
  virtual void print(String *str, enum_query_type query_type);
  virtual bool have_guarded_conds() { return FALSE; }
  bool change_engine(subselect_engine *eng)
  {
    old_engine= engine;
    engine= eng;
    engine_changed= 1;
    return eng == 0;
  }

  /*
    True if this subquery has been already evaluated. Implemented only for
    single select and union subqueries only.
  */
  bool is_evaluated() const;
  bool is_uncacheable() const;

  /*
    Used by max/min subquery to initialize value presence registration
    mechanism. Engine call this method before rexecution query.
  */
  virtual void reset_value_registration() {}
  enum_parsing_place place() { return parsing_place; }
  bool walk_join_condition(List<TABLE_LIST> *tables, Item_processor processor,
                           bool walk_subquery, uchar *argument);
  bool walk_body(Item_processor processor, bool walk_subquery, uchar *arg);
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg);
  virtual bool explain_subquery_checker(uchar **arg);
  bool inform_item_in_cond_of_tab(uchar *join_tab_index);
  virtual bool clean_up_after_removal(uchar *arg);

  const char *func_name() const { DBUG_ASSERT(0); return "subselect"; }

  friend class select_result_interceptor;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(THD *, Item **);
  friend int  Item_field::fix_outer_field(THD *, Field **, Item **);
  friend bool Item_ref::fix_fields(THD *, Item **);
  friend void Item_ident::fix_after_pullout(st_select_lex *parent_select,
                                            st_select_lex *removed_selec);
  friend void mark_select_range_as_dependent(THD*,
                                             st_select_lex*, st_select_lex*,
                                             Field*, Item*, Item_ident*);
};

/* single value subselect */

class Item_cache;
class Item_singlerow_subselect :public Item_subselect
{
protected:
  Item_cache *value, **row;
  bool no_rows;                              ///< @c no_rows_in_result
public:
  Item_singlerow_subselect(st_select_lex *select_lex);
  Item_singlerow_subselect() :
    Item_subselect(), value(0), row (0), no_rows(false) {}

  virtual void cleanup();
  subs_type substype() { return SINGLEROW_SUBS; }

  virtual void reset();
  trans_res select_transformer(JOIN *join);
  void store(uint i, Item* item);
  double val_real();
  longlong val_int ();
  String *val_str (String *);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool val_bool();
  enum Item_result result_type() const;
  enum_field_types field_type() const;
  void fix_length_and_dec();

  /*
    Mark the subquery as having no rows.
    If there are aggregate functions (in the outer query),
    we need to generate a NULL row. @c return_zero_rows().
  */
  virtual void no_rows_in_result();

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
			st_select_lex *select_lex, bool max, bool ignore_nulls);
  virtual void print(String *str, enum_query_type query_type);
  virtual void cleanup();
  bool any_value() { return was_values; }
  void register_value() { was_values= TRUE; }
  void reset_value_registration() { was_values= FALSE; }
};

/* exists subselect */

class Item_exists_subselect :public Item_subselect
{
protected:
  bool value; /* value of this item (boolean: exists/not-exists) */

public:
  /**
    The method chosen to execute the predicate, currently used for IN, =ANY
    and EXISTS predicates.
  */
  enum enum_exec_method {
    EXEC_UNSPECIFIED, ///< No execution method specified yet.
    EXEC_SEMI_JOIN,   ///< Predicate is converted to semi-join nest.
    /// IN was converted to correlated EXISTS, and this is a final decision.
    EXEC_EXISTS,
    /**
       Decision between EXEC_EXISTS and EXEC_MATERIALIZATION is not yet taken.
       IN was temporarily converted to correlated EXISTS.
       All descendants of Item_in_subselect must go through this method
       before they can reach EXEC_EXISTS.
    */
    EXEC_EXISTS_OR_MAT,
    /// Predicate executed via materialization, and this is a final decision.
    EXEC_MATERIALIZATION
  };
  enum_exec_method exec_method;
  /// Priority of this predicate in the convert-to-semi-join-nest process.
  int sj_convert_priority;
  /// True if this predicate is chosen for semi-join transformation
  bool sj_chosen;
  /**
    Used by subquery optimizations to keep track about where this subquery
    predicate is located, and whether it is a candidate for transformation.
      (TABLE_LIST*) 1   - the predicate is an AND-part of the WHERE
      join nest pointer - the predicate is an AND-part of ON expression
                          of a join nest
      NULL              - for all other locations. It also means that the
                          predicate is not a candidate for transformation.
    See also THD::emb_on_expr_nest.
  */
  TABLE_LIST *embedding_join_nest;

  Item_exists_subselect(st_select_lex *select_lex);
  Item_exists_subselect()
    :Item_subselect(), value(false), exec_method(EXEC_UNSPECIFIED),
     sj_convert_priority(0), sj_chosen(false), embedding_join_nest(NULL)
  {}
  virtual trans_res select_transformer(JOIN *join)
  {
    exec_method= EXEC_EXISTS;
    return RES_OK;
  }
  subs_type substype() { return EXISTS_SUBS; }
  virtual void reset() 
  {
    value= 0;
  }

  enum Item_result result_type() const { return INT_RESULT;}
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);

  friend class select_exists_subselect;
  friend class subselect_indexsubquery_engine;
};


/**
  Representation of IN subquery predicates of the form
  "left_expr IN (SELECT ...)".

  @detail
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
public:
  Item *left_expr;
protected:
  /*
    Cache of the left operand of the subquery predicate. Allocated in the
    runtime memory root, for each execution, thus need not be freed.
  */
  List<Cached_item> *left_expr_cache;
  bool left_expr_cache_filled; ///< Whether left_expr_cache holds a value
  /** The need for expr cache may be optimized away, @sa init_left_expr_cache. */
  bool need_expr_cache;

  /*
    expr & optimizer used in subselect rewriting to store Item for
    all JOIN in UNION
  */
  Item *expr;
  Item_in_optimizer *optimizer;
  bool was_null;
  bool abort_on_null;
private:
  /**
     This bundles several pieces of information useful when doing the
     IN->EXISTS transform. If this transform has not been done, pointer is
     NULL.
  */
  struct In2exists_info: public Sql_alloc
  {
    /**
       True: if IN->EXISTS has been done and has added a condition to the
       subquery's WHERE clause.
    */
    bool added_to_where;
    /**
       True: if original subquery was dependent (correlated) before IN->EXISTS
       was done.
    */
    bool originally_dependent;
  } *in2exists_info;

  Item *remove_in2exists_conds(Item* conds);

public:
  /* Used to trigger on/off conditions that were pushed down to subselect */
  bool *pushed_cond_guards;
  
  Item_func_not_all *upper_item; // point on NOT/NOP before ALL/SOME subquery

  /* 
    Location of the subquery predicate. It is either
     - pointer to join nest if the subquery predicate is in the ON expression
     - (TABLE_LIST*)1 if the predicate is in the WHERE.
  */
  TABLE_LIST *expr_join_nest;

  bool in2exists_added_to_where() const
  { return in2exists_info && in2exists_info->added_to_where; }

  /// Is reliable only if IN->EXISTS has been done.
  bool originally_dependent() const
  { return in2exists_info->originally_dependent; }

  bool *get_cond_guard(int i)
  {
    return pushed_cond_guards ? pushed_cond_guards + i : NULL;
  }
  void set_cond_guard_var(int i, bool v) 
  { 
    if ( pushed_cond_guards)
      pushed_cond_guards[i]= v;
  }
  bool have_guarded_conds() { return MY_TEST(pushed_cond_guards); }

  Item_in_subselect(Item * left_expr, st_select_lex *select_lex);
  Item_in_subselect()
    :Item_exists_subselect(), left_expr(NULL), left_expr_cache(NULL),
    left_expr_cache_filled(false), need_expr_cache(TRUE), expr(NULL),
    optimizer(NULL), was_null(FALSE), abort_on_null(FALSE),
    in2exists_info(NULL), pushed_cond_guards(NULL), upper_item(NULL)
  {}
  virtual void cleanup();
  subs_type substype() { return IN_SUBS; }
  virtual void reset() 
  {
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  trans_res select_transformer(JOIN *join);
  trans_res select_in_like_transformer(JOIN *join, Comp_creator *func);
  trans_res single_value_transformer(JOIN *join, Comp_creator *func);
  trans_res row_value_transformer(JOIN * join);
  trans_res single_value_in_to_exists_transformer(JOIN * join,
                                                  Comp_creator *func);
  trans_res row_value_in_to_exists_transformer(JOIN * join);
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg);
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
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  bool init_left_expr_cache();

  /**
     Once the decision to use IN->EXISTS has been taken, performs some last
     steps of this transformation.
  */
  bool finalize_exists_transform(SELECT_LEX *select_lex);
  /**
     Once the decision to use materialization has been taken, performs some
     last steps of this transformation.
  */
  bool finalize_materialization_transform(JOIN *join);

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class Item_in_optimizer;
  friend class subselect_indexsubquery_engine;
  friend class subselect_hash_sj_engine;
};


/* ALL/ANY/SOME subselect */
class Item_allany_subselect :public Item_in_subselect
{
public:
  chooser_compare_func_creator func_creator;
  Comp_creator *func;
  bool all;

  Item_allany_subselect(Item * left_expr, chooser_compare_func_creator fc,
                        st_select_lex *select_lex, bool all);

  // only ALL subquery has upper not
  subs_type substype() { return all?ALL_SUBS:ANY_SUBS; }
  trans_res select_transformer(JOIN *join);
  virtual void print(String *str, enum_query_type query_type);
};


class subselect_engine: public Sql_alloc
{
protected:
  select_result_interceptor *result; /* results storage class */
  Item_subselect *item; /* item, that use this engine */
  enum Item_result res_type; /* type of results */
  enum_field_types res_field_type; /* column type of the results */
  bool maybe_null; /* may be null (first item in select) */
public:

  enum enum_engine_type {ABSTRACT_ENGINE, SINGLE_SELECT_ENGINE,
                         UNION_ENGINE, UNIQUESUBQUERY_ENGINE,
                         INDEXSUBQUERY_ENGINE, HASH_SJ_ENGINE};

  subselect_engine(Item_subselect *si, select_result_interceptor *res)
    :result(res), item(si), res_type(STRING_RESULT),
    res_field_type(MYSQL_TYPE_VAR_STRING), maybe_null(false)
  {}
  virtual ~subselect_engine() {}; // to satisfy compiler
  /**
    Cleanup engine after complete query execution, free all resources.
  */
  virtual void cleanup()= 0;

  /// Sets "thd" for 'result'. Should be called before prepare()
  void set_thd_for_result();
  virtual bool prepare()= 0;
  virtual void fix_length_and_dec(Item_cache** row)= 0;
  /*
    Execute the engine

    SYNOPSIS
      exec()

    DESCRIPTION
      Execute the engine. The result of execution is subquery value that is
      either captured by previously set up select_result-based 'sink' or
      stored somewhere by the exec() method itself.

    RETURN
      0 - OK
      1 - Either an execution error, or the engine was "changed", and the
          caller should call exec() again for the new engine.
  */
  virtual bool exec()= 0;
  virtual uint cols() const = 0; /* return number of columns in select */
  virtual uint8 uncacheable() const = 0; /* query is uncacheable */
  virtual enum Item_result type() const { return res_type; }
  virtual enum_field_types field_type() const { return res_field_type; }
  virtual void exclude()= 0;
  virtual bool may_be_null() const { return maybe_null; };
  virtual table_map upper_select_const_tables() const = 0;
  static table_map calc_const_tables(TABLE_LIST *);
  virtual void print(String *str, enum_query_type query_type)= 0;
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result)= 0;
  virtual bool no_tables() const = 0;
  virtual bool is_executed() const { return FALSE; }
  virtual enum_engine_type engine_type() const { return ABSTRACT_ENGINE; }
#ifndef DBUG_OFF
  /**
     @returns the internal Item. Defined only in debug builds, because should
     be used only for debug asserts.
  */
  const Item_subselect *get_item() const { return item; }
#endif

protected:
  void set_row(List<Item> &item_list, Item_cache **row);
};


class subselect_single_select_engine: public subselect_engine
{
private:
  bool prepared; /* simple subselect is prepared */
  bool executed; /* simple subselect is executed */
  bool optimize_error; ///< simple subselect optimization failed
  st_select_lex *select_lex; /* corresponding select_lex */
  JOIN * join; /* corresponding JOIN structure */
public:
  subselect_single_select_engine(st_select_lex *select,
				 select_result_interceptor *result,
				 Item_subselect *item);
  virtual void cleanup();
  virtual bool prepare();
  virtual void fix_length_and_dec(Item_cache** row);
  virtual bool exec();
  virtual uint cols() const;
  virtual uint8 uncacheable() const;
  virtual void exclude();
  virtual table_map upper_select_const_tables() const;
  virtual void print (String *str, enum_query_type query_type);
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result);
  virtual bool no_tables() const;
  virtual bool may_be_null() const;
  virtual bool is_executed() const { return executed; }
  virtual enum_engine_type engine_type() const { return SINGLE_SELECT_ENGINE; }

  friend class subselect_hash_sj_engine;
  friend class Item_in_subselect;
};


class subselect_union_engine: public subselect_engine
{
public:
  subselect_union_engine(st_select_lex_unit *u,
			 select_result_interceptor *result,
			 Item_subselect *item);
  virtual void cleanup();
  virtual bool prepare();
  virtual void fix_length_and_dec(Item_cache** row);
  virtual bool exec();
  virtual uint cols() const;
  virtual uint8 uncacheable() const;
  virtual void exclude();
  virtual table_map upper_select_const_tables() const;
  virtual void print (String *str, enum_query_type query_type);
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result);
  virtual bool no_tables() const;
  virtual bool is_executed() const;
  virtual enum_engine_type engine_type() const { return UNION_ENGINE; }

private:
  st_select_lex_unit *unit;  /* corresponding unit structure */
};


struct st_join_table;


/**
  A subquery execution engine that evaluates the subquery by doing index
  lookups in a single table's index.

  This engine is used to resolve subqueries in forms

    outer_expr IN (SELECT tbl.key FROM tbl WHERE subq_where)

  or, row-based:

    (oe1, .. oeN) IN (SELECT key_part1, ... key_partK
                      FROM tbl WHERE subqwhere)

  i.e. the subquery is a single table SELECT without GROUP BY, aggregate
  functions, etc.
*/
class subselect_indexsubquery_engine : public subselect_engine
{
protected:
  st_join_table *tab;
  Item *cond; /* The WHERE condition of subselect */
private:
  /* FALSE for 'ref', TRUE for 'ref-or-null'. */
  bool check_null;
  /* 
    The "having" clause. This clause (further referred to as "artificial
    having") was inserted by subquery transformation code. It contains 
    Item(s) that have a side-effect: they record whether the subquery has 
    produced a row with NULL certain components. We need to use it for cases
    like
      (oe1, oe2) IN (SELECT t.key, t.no_key FROM t1)
    where we do index lookup on t.key=oe1 but need also to check if there
    was a row such that t.no_key IS NULL.
  */
  Item *having;
  /**
     Whether a lookup for key value (x0,y0) (x0 and/or y0 being NULL or not
     NULL) will find at most one row.
  */
  bool unique;
public:

  // constructor can assign THD because it will be called after JOIN::prepare
  subselect_indexsubquery_engine(THD *thd_arg, st_join_table *tab_arg,
				 Item_subselect *subs, Item *where,
                                 Item *having_arg, bool chk_null,
                                 bool unique_arg)
    :subselect_engine(subs, 0), tab(tab_arg), cond(where),
    check_null(chk_null), having(having_arg), unique(unique_arg)
  {};
  virtual bool exec();
  virtual void print (String *str, enum_query_type query_type);
  virtual enum_engine_type engine_type() const
  { return unique ? UNIQUESUBQUERY_ENGINE : INDEXSUBQUERY_ENGINE; }
  virtual void cleanup() {}
  virtual bool prepare();
  virtual void fix_length_and_dec(Item_cache** row);
  virtual uint cols() const { return 1; }
  virtual uint8 uncacheable() const { return UNCACHEABLE_DEPENDENT; }
  virtual void exclude();
  virtual table_map upper_select_const_tables() const { return 0; }
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result);
  virtual bool no_tables() const;
  bool scan_table();
  void copy_ref_key(bool *require_scan, bool *convert_error);
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
  Compute an IN predicate via a hash semi-join. The subquery is materialized
  during the first evaluation of the IN predicate. The IN predicate is executed
  via the functionality inherited from subselect_indexsubquery_engine.
*/

class subselect_hash_sj_engine: public subselect_indexsubquery_engine
{
private:
  /* TRUE if the subquery was materialized into a temp table. */
  bool is_materialized;
  /**
     Existence of inner NULLs in materialized table:
     By design, other values than IRRELEVANT_OR_FALSE are possible only if the
     subquery has only one inner expression.
  */
  enum nulls_exist
  {
    /// none, or they don't matter
    NEX_IRRELEVANT_OR_FALSE= 0,
    /// they matter, and we don't know yet if they exists
    NEX_UNKNOWN= 1,
    /// they matter, and we know there exists at least one.
    NEX_TRUE= 2
  };
  enum nulls_exist mat_table_has_nulls;
  /*
    The old engine already chosen at parse time and stored in permanent memory.
    Through this member we can re-create and re-prepare the join object
    used to materialize the subquery for each execution of a prepared
    statement. We also reuse the functionality of
    subselect_single_select_engine::[prepare | cols].
  */
  subselect_single_select_engine *materialize_engine;
  /* Temp table context of the outer select's JOIN. */
  TMP_TABLE_PARAM *tmp_param;

public:
  subselect_hash_sj_engine(THD *thd, Item_subselect *in_predicate,
                           subselect_single_select_engine *old_engine)
    :subselect_indexsubquery_engine(thd, NULL, in_predicate, NULL,
                                    NULL, false, true),
    is_materialized(false), materialize_engine(old_engine), tmp_param(NULL)
  {}
  ~subselect_hash_sj_engine();

  bool setup(List<Item> *tmp_columns);
  virtual void cleanup();
  virtual bool prepare() 
  { 
    return materialize_engine->prepare();
  }
  virtual bool exec();
  virtual void print (String *str, enum_query_type query_type);
  virtual uint cols() const
  {
    return materialize_engine->cols();
  }
  virtual enum_engine_type engine_type() const { return HASH_SJ_ENGINE; }
  
  const st_join_table *get_join_tab() const { return tab; }
  Item *get_cond_for_explain() const { return cond; }
};
#endif /* ITEM_SUBSELECT_INCLUDED */

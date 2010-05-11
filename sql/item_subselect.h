/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* subselect Item */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

class st_select_lex;
class st_select_lex_unit;
class JOIN;
class select_subselect;
class subselect_engine;
class Item_bool_func2;

/* base class for subselects */

class Item_subselect :public Item_result_field
{
  my_bool value_assigned; /* value already assigned to subselect */
protected:
  /* thread handler, will be assigned in fix_fields only */
  THD *thd;
  /* substitution instead of subselect in case of optimization */
  Item *substitution;
  /* unit of subquery */
  st_select_lex_unit *unit;
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

  /* TRUE <=> The underlying SELECT is correlated w.r.t some ancestor select */
  bool is_correlated; 

  enum trans_res {RES_OK, RES_REDUCE, RES_ERROR};
  enum subs_type {UNKNOWN_SUBS, SINGLEROW_SUBS,
		  EXISTS_SUBS, IN_SUBS, ALL_SUBS, ANY_SUBS};

  Item_subselect();

  virtual subs_type substype() { return UNKNOWN_SUBS; }

  /*
     We need this method, because some compilers do not allow 'this'
     pointer in constructor initialization list, but we need pass pointer
     to subselect Item class to select_subselect classes constructor.
  */
  virtual void init (st_select_lex *select_lex,
		     select_subselect *result);

  ~Item_subselect();
  void cleanup();
  virtual void reset()
  {
    null_value= 1;
  }
  virtual trans_res select_transformer(JOIN *join);
  bool assigned() { return value_assigned; }
  void assigned(bool a) { value_assigned= a; }
  enum Type type() const;
  bool is_null()
  {
    update_null_value();
    return null_value;
  }
  bool fix_fields(THD *thd, Item **ref);
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
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg);
  Item *safe_charset_converter(CHARSET_INFO *tocs);

  /**
    Get the SELECT_LEX structure associated with this Item.
    @return the SELECT_LEX structure associated with this Item
  */
  st_select_lex* get_select_lex();
  const char *func_name() const { DBUG_ASSERT(0); return "subselect"; }

  friend class select_subselect;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(THD *, Item **);
  friend int  Item_field::fix_outer_field(THD *, Field **, Item **);
  friend bool Item_ref::fix_fields(THD *, Item **);
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
public:
  Item_singlerow_subselect(st_select_lex *select_lex);
  Item_singlerow_subselect() :Item_subselect(), value(0), row (0) {}

  void cleanup();
  subs_type substype() { return SINGLEROW_SUBS; }

  void reset();
  trans_res select_transformer(JOIN *join);
  void store(uint i, Item* item);
  double val_real();
  longlong val_int ();
  String *val_str (String *);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  enum Item_result result_type() const;
  enum_field_types field_type() const;
  void fix_length_and_dec();

  uint cols();
  Item* element_index(uint i) { return my_reinterpret_cast(Item*)(row[i]); }
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
			st_select_lex *select_lex, bool max);
  virtual void print(String *str, enum_query_type query_type);
  void cleanup();
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
  Item_exists_subselect(st_select_lex *select_lex);
  Item_exists_subselect(): Item_subselect() {}

  subs_type substype() { return EXISTS_SUBS; }
  void reset() 
  {
    value= 0;
  }

  enum Item_result result_type() const { return INT_RESULT;}
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);

  friend class select_exists_subselect;
  friend class subselect_uniquesubquery_engine;
  friend class subselect_indexsubquery_engine;
};


/*
  IN subselect: this represents "left_exr IN (SELECT ...)"

  This class has: 
   - (as a descendant of Item_subselect) a "subquery execution engine" which 
      allows it to evaluate subqueries. (and this class participates in
      execution by having was_null variable where part of execution result
      is stored.
   - Transformation methods (todo: more on this).

  This class is not used directly, it is "wrapped" into Item_in_optimizer
  which provides some small bits of subquery evaluation.
*/

class Item_in_subselect :public Item_exists_subselect
{
protected:
  Item *left_expr;
  /*
    expr & optimizer used in subselect rewriting to store Item for
    all JOIN in UNION
  */
  Item *expr;
  Item_in_optimizer *optimizer;
  bool was_null;
  bool abort_on_null;
  bool transformed;
public:
  /* Used to trigger on/off conditions that were pushed down to subselect */
  bool *pushed_cond_guards;

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
    :Item_exists_subselect(), optimizer(0), abort_on_null(0), transformed(0),
     pushed_cond_guards(NULL), upper_item(0)
  {}

  subs_type substype() { return IN_SUBS; }
  void reset() 
  {
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  trans_res select_transformer(JOIN *join);
  trans_res select_in_like_transformer(JOIN *join, Comp_creator *func);
  trans_res single_value_transformer(JOIN *join, Comp_creator *func);
  trans_res row_value_transformer(JOIN * join);
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

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class subselect_indexsubquery_engine;
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
  select_subselect *result; /* results storage class */
  THD *thd; /* pointer to current THD */
  Item_subselect *item; /* item, that use this engine */
  enum Item_result res_type; /* type of results */
  enum_field_types res_field_type; /* column type of the results */
  bool maybe_null; /* may be null (first item in select) */
public:

  subselect_engine(Item_subselect *si, select_subselect *res)
    :thd(0)
  {
    result= res;
    item= si;
    res_type= STRING_RESULT;
    res_field_type= MYSQL_TYPE_VAR_STRING;
    maybe_null= 0;
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
  enum_field_types field_type() { return res_field_type; }
  virtual void exclude()= 0;
  virtual bool may_be_null() { return maybe_null; };
  virtual table_map upper_select_const_tables()= 0;
  static table_map calc_const_tables(TABLE_LIST *);
  virtual void print(String *str, enum_query_type query_type)= 0;
  virtual bool change_result(Item_subselect *si, select_subselect *result)= 0;
  virtual bool no_tables()= 0;
  virtual bool is_executed() const { return FALSE; }
  /* Check if subquery produced any rows during last query execution */
  virtual bool no_rows() = 0;

protected:
  void set_row(List<Item> &item_list, Item_cache **row);
};


class subselect_single_select_engine: public subselect_engine
{
  my_bool prepared; /* simple subselect is prepared */
  my_bool optimized; /* simple subselect is optimized */
  my_bool executed; /* simple subselect is executed */
  st_select_lex *select_lex; /* corresponding select_lex */
  JOIN * join; /* corresponding JOIN structure */
public:
  subselect_single_select_engine(st_select_lex *select,
				 select_subselect *result,
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
  bool change_result(Item_subselect *si, select_subselect *result);
  bool no_tables();
  bool may_be_null();
  bool is_executed() const { return executed; }
  bool no_rows();
};


class subselect_union_engine: public subselect_engine
{
  st_select_lex_unit *unit;  /* corresponding unit structure */
public:
  subselect_union_engine(st_select_lex_unit *u,
			 select_subselect *result,
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
  bool change_result(Item_subselect *si, select_subselect *result);
  bool no_tables();
  bool is_executed() const;
  bool no_rows();
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
  bool null_keypart; /* TRUE <=> constructed search tuple has a NULL */
public:

  // constructor can assign THD because it will be called after JOIN::prepare
  subselect_uniquesubquery_engine(THD *thd_arg, st_join_table *tab_arg,
				  Item_subselect *subs, Item *where)
    :subselect_engine(subs, 0), tab(tab_arg), cond(where)
  {
    set_thd(thd_arg);
  }
  ~subselect_uniquesubquery_engine();
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols() { return 1; }
  uint8 uncacheable() { return UNCACHEABLE_DEPENDENT; }
  void exclude();
  table_map upper_select_const_tables() { return 0; }
  virtual void print (String *str, enum_query_type query_type);
  bool change_result(Item_subselect *si, select_subselect *result);
  bool no_tables();
  int scan_table();
  bool copy_ref_key();
  bool no_rows() { return empty_result_set; }
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
};


inline bool Item_subselect::is_evaluated() const
{
  return engine->is_executed();
}

inline bool Item_subselect::is_uncacheable() const
{
  return engine->uncacheable();
}



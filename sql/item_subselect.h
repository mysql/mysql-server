/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* subselect Item */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class st_select_lex;
class st_select_lex_unit;
class JOIN;
class select_subselect;
class subselect_engine;
class Item_bool_func2;
class Item_arena;

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
    val_int();
    return null_value;
  }
  bool fix_fields(THD *thd, TABLE_LIST *tables, Item **ref);
  bool exec();
  virtual void fix_length_and_dec();
  table_map used_tables() const;
  bool const_item() const;
  inline table_map get_used_tables_cache() { return used_tables_cache; }
  inline bool get_const_item_cache() { return const_item_cache; }
  Item *get_tmp_table_item(THD *thd);
  void update_used_tables();
  void print(String *str);
  bool change_engine(subselect_engine *eng)
  {
    old_engine= engine;
    engine= eng;
    engine_changed= 1;
    return eng == 0;
  }
  enum_parsing_place place() { return parsing_place; }

  friend class select_subselect;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(THD *, TABLE_LIST *, Item **);
  friend bool Item_ref::fix_fields(THD *, TABLE_LIST *, Item **);
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
  double val();
  longlong val_int ();
  String *val_str (String *);
  enum Item_result result_type() const;
  void fix_length_and_dec();

  uint cols();
  Item* el(uint i) { return my_reinterpret_cast(Item*)(row[i]); }
  Item** addr(uint i) { return (Item**)row + i; }
  bool check_cols(uint c);
  bool null_inside();
  void bring_value();

  friend class select_singlerow_subselect;
};

/* used in static ALL/ANY optimisation */
class Item_maxmin_subselect :public Item_singlerow_subselect
{
  bool max;
public:
  Item_maxmin_subselect(THD *thd, Item_subselect *parent,
			st_select_lex *select_lex, bool max);
  void print(String *str);
};

/* exists subselect */

class Item_exists_subselect :public Item_subselect
{
protected:
  longlong value; /* value of this item (boolean: exists/not-exists) */

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
  double val();
  String *val_str(String*);
  void fix_length_and_dec();
  void print(String *str);

  friend class select_exists_subselect;
  friend class subselect_uniquesubquery_engine;
  friend class subselect_indexsubquery_engine;
};

/* IN subselect */

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
  Item_func_not_all *upper_not; // point on NOT before ALL subquery

  Item_in_subselect(Item * left_expr, st_select_lex *select_lex);
  Item_in_subselect()
    :Item_exists_subselect(), abort_on_null(0), transformed(0), upper_not(0)
  {}

  subs_type substype() { return IN_SUBS; }
  void reset() 
  {
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  trans_res select_transformer(JOIN *join);
  trans_res single_value_transformer(JOIN *join,
				     Comp_creator *func);
  trans_res row_value_transformer(JOIN * join);
  longlong val_int();
  double val();
  String *val_str(String*);
  void top_level_item() { abort_on_null=1; }
  bool test_limit(st_select_lex_unit *unit);
  void print(String *str);

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class subselect_indexsubquery_engine;
};


/* ALL/ANY/SOME subselect */
class Item_allany_subselect :public Item_in_subselect
{
protected:
  Comp_creator *func;

public:
  bool all;

  Item_allany_subselect(Item * left_expr, Comp_creator *f,
		     st_select_lex *select_lex, bool all);

  // only ALL subquery has upper not
  subs_type substype() { return upper_not?ALL_SUBS:ANY_SUBS; }
  trans_res select_transformer(JOIN *join);
  void print(String *str);
};


class subselect_engine: public Sql_alloc
{
protected:
  select_subselect *result; /* results storage class */
  THD *thd; /* pointer to current THD */
  Item_subselect *item; /* item, that use this engine */
  enum Item_result res_type; /* type of results */
  bool maybe_null; /* may be null (first item in select) */
public:

  subselect_engine(Item_subselect *si, select_subselect *res)
    :thd(0)
  {
    result= res;
    item= si;
    res_type= STRING_RESULT;
    maybe_null= 0;
  }
  virtual ~subselect_engine() {}; // to satisfy compiler
  virtual void cleanup()= 0;

  // set_thd should be called before prepare()
  void set_thd(THD *thd_arg) { thd= thd_arg; }
  THD * get_thd() { return thd; }
  virtual int prepare()= 0;
  virtual void fix_length_and_dec(Item_cache** row)= 0;
  virtual int exec()= 0;
  virtual uint cols()= 0; /* return number of columnss in select */
  virtual uint8 uncacheable()= 0; /* query is uncacheable */
  enum Item_result type() { return res_type; }
  virtual void exclude()= 0;
  bool may_be_null() { return maybe_null; };
  virtual table_map upper_select_const_tables()= 0;
  static table_map calc_const_tables(TABLE_LIST *);
  virtual void print(String *str)= 0;
  virtual int change_item(Item_subselect *si, select_subselect *result)= 0;
  virtual bool no_tables()= 0;
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
  void print (String *str);
  int change_item(Item_subselect *si, select_subselect *result);
  bool no_tables();
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
  void print (String *str);
  int change_item(Item_subselect *si, select_subselect *result);
  bool no_tables();
};


struct st_join_table;
class subselect_uniquesubquery_engine: public subselect_engine
{
protected:
  st_join_table *tab;
  Item *cond;
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
  void print (String *str);
  int change_item(Item_subselect *si, select_subselect *result);
  bool no_tables();
};


class subselect_indexsubquery_engine: public subselect_uniquesubquery_engine
{
  bool check_null;
public:

  // constructor can assign THD because it will be called after JOIN::prepare
  subselect_indexsubquery_engine(THD *thd, st_join_table *tab_arg,
				 Item_subselect *subs, Item *where,
				 bool chk_null)
    :subselect_uniquesubquery_engine(thd, tab_arg, subs, where),
     check_null(chk_null)
  {}
  int exec();
  void print (String *str);
};

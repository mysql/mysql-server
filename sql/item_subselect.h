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

typedef Item_bool_func2* (*compare_func_creator)(Item*, Item*);

/* base class for subselects */

class Item_subselect :public Item_result_field
{
  my_bool engine_owner; /* Is this item owner of engine */
  my_bool value_assigned; /* value already assigned to subselect */
protected:
  /* thread handler, will be assigned in fix_fields only */
  THD *thd;
  /* substitution instead of subselect in case of optimization */
  Item *substitution;
  /* engine that perform execution of subselect (single select or union) */
  subselect_engine *engine; 
  /* allowed number of columns (1 for single value subqueries) */
  uint max_columns;
  /* work with 'substitution' */
  bool have_to_be_excluded;

public:
  enum trans_res {OK, REDUCE, ERROR};

  Item_subselect();
  Item_subselect(Item_subselect *item)
  {
    substitution= item->substitution;
    null_value= item->null_value;
    decimals= item->decimals;
    max_columns= item->max_columns;
    engine= item->engine;
    engine_owner= 0;
    name= item->name;
  }

  /* 
     We need this method, because some compilers do not allow 'this'
     pointer in constructor initialization list, but we need pass pointer
     to subselect Item class to select_subselect classes constructor.
  */
  virtual void init (THD *thd, st_select_lex *select_lex, 
		     select_subselect *result);

  ~Item_subselect();
  virtual void reset() 
  {
    null_value= 1;
  }
  virtual trans_res select_transformer(THD *thd, JOIN *join);
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
  void print(String *str)
  {
    if (name)
      str->append(name);
    else
      str->append("-subselect-");
  }

  friend class select_subselect;
  friend class Item_in_optimizer;
};

/* single value subselect */

class Item_cache;
class Item_singlerow_subselect :public Item_subselect
{
protected:
  Item_cache *value, **row;
public:
  Item_singlerow_subselect(THD *thd, st_select_lex *select_lex);
  Item_singlerow_subselect(Item_singlerow_subselect *item):
    Item_subselect(item)
  {
    value= item->value;
    max_length= item->max_length;
    decimals= item->decimals;
  }
  void reset();
  trans_res select_transformer(THD *thd, JOIN *join);
  void store(uint i, Item* item);
  double val();
  longlong val_int ();
  String *val_str (String *);
  Item *new_item() { return new Item_singlerow_subselect(this); }
  enum Item_result result_type() const;
  void fix_length_and_dec();

  uint cols();
  Item* el(uint i) { return (Item*)row[i]; }
  Item** addr(uint i) { return (Item**)row + i; }
  bool check_cols(uint c);
  bool null_inside();
  void bring_value();

  friend class select_singlerow_subselect;
};

/* exists subselect */

class Item_exists_subselect :public Item_subselect
{
protected:
  longlong value; /* value of this item (boolean: exists/not-exists) */

public:
  Item_exists_subselect(THD *thd, st_select_lex *select_lex);
  Item_exists_subselect(Item_exists_subselect *item):
    Item_subselect(item)
  {
    value= item->value;
  }
  Item_exists_subselect(): Item_subselect() {}

  void reset() 
  {
    value= 0;
  }

  Item *new_item() { return new Item_exists_subselect(this); }
  enum Item_result result_type() const { return INT_RESULT;}
  longlong val_int();
  double val();
  String *val_str(String*);
  void fix_length_and_dec();

  friend class select_exists_subselect;
};

/* IN subselect */

class Item_in_subselect :public Item_exists_subselect
{
protected:
  Item * left_expr;
  /*
    expr & optinizer used in subselect rewriting to store Item for
    all JOIN in UNION
  */
  Item *expr;
  Item_in_optimizer *optimizer;
  bool was_null;
  bool abort_on_null;
public:
  Item_in_subselect(THD *thd, Item * left_expr, st_select_lex *select_lex);
  Item_in_subselect(Item_in_subselect *item);
  Item_in_subselect(): Item_exists_subselect(),  abort_on_null(0)  {}
  void reset() 
  {
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  trans_res select_transformer(THD *thd, JOIN *join);
  trans_res single_value_transformer(THD *thd, JOIN *join,
				     Item *left_expr,
				     compare_func_creator func);
  trans_res row_value_transformer(THD *thd, JOIN * join,
				  Item *left_expr);
  longlong val_int();
  double val();
  String *val_str(String*);
  void top_level_item() { abort_on_null=1; }
  bool test_limit(st_select_lex_unit *unit);

  friend class Item_asterisk_remover;
  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
};

/* ALL/ANY/SOME subselect */
class Item_allany_subselect :public Item_in_subselect
{
protected:
  compare_func_creator func;

public:
  Item_allany_subselect(THD *thd, Item * left_expr, compare_func_creator f,
		     st_select_lex *select_lex);
  Item_allany_subselect(Item_allany_subselect *item);
  trans_res select_transformer(THD *thd, JOIN *join);
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

  subselect_engine(THD *thd, Item_subselect *si, select_subselect *res) 
  {
    result= res;
    item= si;
    this->thd= thd;
    res_type= STRING_RESULT;
    maybe_null= 0;
  }
  virtual ~subselect_engine() {}; // to satisfy compiler

  virtual int prepare()= 0;
  virtual void fix_length_and_dec(Item_cache** row)= 0;
  virtual int exec()= 0;
  virtual uint cols()= 0; /* return number of columnss in select */
  virtual bool dependent()= 0; /* depended from outer select */
  virtual bool uncacheable()= 0; /* query is uncacheable */
  enum Item_result type() { return res_type; }
  virtual void exclude()= 0;
  bool may_be_null() { return maybe_null; };
};

class subselect_single_select_engine: public subselect_engine
{
  my_bool prepared; /* simple subselect is prepared */
  my_bool optimized; /* simple subselect is optimized */
  my_bool executed; /* simple subselect is executed */
  st_select_lex *select_lex; /* corresponding select_lex */
  JOIN * join; /* corresponding JOIN structure */
public:
  subselect_single_select_engine(THD *thd, st_select_lex *select,
				 select_subselect *result,
				 Item_subselect *item);
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols();
  bool dependent();
  bool uncacheable();
  void exclude();
};

class subselect_union_engine: public subselect_engine
{
  st_select_lex_unit *unit;  /* corresponding unit structure */
public:
  subselect_union_engine(THD *thd,
			 st_select_lex_unit *u,
			 select_subselect *result,
			 Item_subselect *item);
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint cols();
  bool dependent();
  bool uncacheable();
  void exclude();
};

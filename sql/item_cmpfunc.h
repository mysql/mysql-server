/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/* compare and test functions */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class Item_bool_func :public Item_int_func
{
public:
  Item_bool_func() :Item_int_func() {}
  Item_bool_func(Item *a) :Item_int_func(a) {}
  Item_bool_func(Item *a,Item *b) :Item_int_func(a,b) {}
  void fix_length_and_dec() { decimals=0; max_length=1; }
};

class Item_bool_func2 :public Item_int_func
{						/* Bool with 2 string args */
protected:
  String tmp_value1,tmp_value2;
public:
  Item_bool_func2(Item *a,Item *b) :Item_int_func(a,b) {}
  void fix_length_and_dec();
  void set_cmp_func(Item_result type);
  int (Item_bool_func2::*cmp_func)();
  int compare_string();				/* compare arg[0] & arg[1] */
  int compare_real();				/* compare arg[0] & arg[1] */
  int compare_int();				/* compare arg[0] & arg[1] */
  optimize_type select_optimize() const { return OPTIMIZE_OP; }
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const { return rev_functype() != UNKNOWN_FUNC; }
  void print(String *str) { Item_func::print_op(str); }
};


class Item_func_not :public Item_bool_func
{
public:
  Item_func_not(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  const char *func_name() const { return "not"; }
};

class Item_func_eq :public Item_bool_func2
{
public:
  Item_func_eq(Item *a,Item *b) :Item_bool_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return EQ_FUNC; }
  enum Functype rev_functype() const { return EQ_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "="; }
};

class Item_func_equal :public Item_bool_func2
{
  Item_result cmp_result_type;
public:
  Item_func_equal(Item *a,Item *b) :Item_bool_func2(a,b) { };
  longlong val_int();
  void fix_length_and_dec();
  enum Functype functype() const { return EQUAL_FUNC; }
  enum Functype rev_functype() const { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<=>"; }
};


class Item_func_ge :public Item_bool_func2
{
public:
  Item_func_ge(Item *a,Item *b) :Item_bool_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return GE_FUNC; }
  enum Functype rev_functype() const { return LE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return ">="; }
};


class Item_func_gt :public Item_bool_func2
{
public:
  Item_func_gt(Item *a,Item *b) :Item_bool_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return GT_FUNC; }
  enum Functype rev_functype() const { return LT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return ">"; }
};


class Item_func_le :public Item_bool_func2
{
public:
  Item_func_le(Item *a,Item *b) :Item_bool_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return LE_FUNC; }
  enum Functype rev_functype() const { return GE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<="; }
};


class Item_func_lt :public Item_bool_func2
{
public:
  Item_func_lt(Item *a,Item *b) :Item_bool_func2(a,b) { }
  longlong val_int();
  enum Functype functype() const { return LT_FUNC; }
  enum Functype rev_functype() const { return GT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return "<"; }
};


class Item_func_ne :public Item_bool_func2
{
public:
  Item_func_ne(Item *a,Item *b) :Item_bool_func2(a,b) { }
  longlong val_int();
  enum Functype functype() const { return NE_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "<>"; }
};


class Item_func_between :public Item_int_func
{
  int (*string_compare)(const String *x,const String *y);
public:
  Item_result cmp_type;
  String value0,value1,value2;
  Item_func_between(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_KEY; }
  enum Functype functype() const   { return BETWEEN; }
  const char *func_name() const { return "between"; }
  void fix_length_and_dec();
};


class Item_func_strcmp :public Item_bool_func2
{
public:
  Item_func_strcmp(Item *a,Item *b) :Item_bool_func2(a,b) {}
  longlong val_int();
  void fix_length_and_dec() { max_length=2; }
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "strcmp"; }
};


class Item_func_interval :public Item_int_func
{
  Item *item;
  double *intervals;
public:
  Item_func_interval(Item *a,List<Item> &list)
    :Item_int_func(list),item(a),intervals(0) {}
  longlong val_int();
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void fix_length_and_dec();
  ~Item_func_interval() { delete item; }
  const char *func_name() const { return "interval"; }
  void update_used_tables();
};


class Item_func_ifnull :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_ifnull(Item *a,Item *b) :Item_func(a,b) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  const char *func_name() const { return "ifnull"; }
};


class Item_func_if :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_if(Item *a,Item *b,Item *c) :Item_func(a,b,c) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  const char *func_name() const { return "if"; }
};


class Item_func_nullif :public Item_bool_func2
{
  enum Item_result cached_result_type;
public:
  Item_func_nullif(Item *a,Item *b) :Item_bool_func2(a,b) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  const char *func_name() const { return "nullif"; }
};


class Item_func_coalesce :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_coalesce(List<Item> &list) :Item_func(list) {}
  double val();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cached_result_type; }
  const char *func_name() const { return "coalesce"; }
};

class Item_func_case :public Item_func
{
  Item * first_expr, *else_expr;
  enum Item_result cached_result_type;
  String tmp_value;
public:
  Item_func_case(List<Item> &list, Item *first_expr_, Item *else_expr_)
    :Item_func(list), first_expr(first_expr_), else_expr(else_expr_) {}
  double val();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  void update_used_tables();
  enum Item_result result_type () const { return cached_result_type; }
  const char *func_name() const { return "case"; }
  void print(String *str);
  bool fix_fields(THD *thd,struct st_table_list *tlist);
  Item *find_item(String *str);
};


/* Functions to handle the optimized IN */

class in_vector :public Sql_alloc
{
 protected:
  char *base;
  uint size;
  qsort_cmp compare;
  uint count;
public:
  uint used_count;
  in_vector(uint elements,uint element_length,qsort_cmp cmp_func)
    :base((char*) sql_calloc(elements*element_length)),
     size(element_length), compare(cmp_func), count(elements),
     used_count(elements) {}
  virtual ~in_vector() {}
  virtual void set(uint pos,Item *item)=0;
  virtual byte *get_value(Item *item)=0;
  void sort()
    {
      qsort(base,used_count,size,compare);
    }
  int find(Item *item);
};


class in_string :public in_vector
{
  char buff[80];
  String tmp;
public:
  in_string(uint elements,qsort_cmp cmp_func);
  ~in_string();
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};


class in_longlong :public in_vector
{
  longlong tmp;
public:
  in_longlong(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};


class in_double :public in_vector
{
  double tmp;
public:
  in_double(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};


/*
** Classes for easy comparing of non const items
*/

class cmp_item :public Sql_alloc
{
public:
  cmp_item() {}
  virtual ~cmp_item() {}
  virtual void store_value(Item *item)=0;
  virtual int cmp(Item *item)=0;
};


class cmp_item_sort_string :public cmp_item {
 protected:
  char value_buff[80];
  String value,*value_res;
public:
  cmp_item_sort_string() :value(value_buff,sizeof(value_buff)) {}
  void store_value(Item *item)
    {
      value_res=item->val_str(&value);
    }
  int cmp(Item *arg)
    {
      char buff[80];
      String tmp(buff,sizeof(buff)),*res;
      if (!(res=arg->val_str(&tmp)))
	return 1;				/* Can't be right */
      return sortcmp(value_res,res);
    }
};

class cmp_item_binary_string :public cmp_item_sort_string {
public:
  cmp_item_binary_string() {}
  int cmp(Item *arg)
    {
      char buff[80];
      String tmp(buff,sizeof(buff)),*res;
      if (!(res=arg->val_str(&tmp)))
	return 1;				/* Can't be right */
      return stringcmp(value_res,res);
    }
};


class cmp_item_int :public cmp_item
{
  longlong value;
public:
  void store_value(Item *item)
    {
      value=item->val_int();
    }
  int cmp(Item *arg)
    {
      return value != arg->val_int();
    }
};


class cmp_item_real :public cmp_item
{
  double value;
public:
  void store_value(Item *item)
    {
      value= item->val();
    }
  int cmp(Item *arg)
    {
      return value != arg->val();
    }
};


class Item_func_in :public Item_int_func
{
  Item *item;
  in_vector *array;
  cmp_item *in_item;
 public:
  Item_func_in(Item *a,List<Item> &list)
    :Item_int_func(list),item(a),array(0),in_item(0) {}
  longlong val_int();
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void fix_length_and_dec();
  ~Item_func_in() { delete item; delete array; delete in_item; }
  optimize_type select_optimize() const
    { return array ? OPTIMIZE_KEY : OPTIMIZE_NONE; }
  Item *key_item() const { return item; }
  void print(String *str);
  enum Functype functype() const { return IN_FUNC; }
  const char *func_name() const { return " IN "; }
  void update_used_tables();
};



/* Functions used by where clause */

class Item_func_isnull :public Item_bool_func
{
public:
  Item_func_isnull(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return ISNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
    Item_func_isnull::update_used_tables();
  }
  const char *func_name() const { return "isnull"; }
  /* Optimize case of not_null_column IS NULL */
  void update_used_tables()
  {
    if (!args[0]->maybe_null)
      used_tables_cache=0;			/* is always false */
    else
    {
      args[0]->update_used_tables();
      used_tables_cache=args[0]->used_tables();
    }
  }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
};

class Item_func_isnotnull :public Item_bool_func
{
public:
  Item_func_isnotnull(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return ISNOTNULL_FUNC; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=0; }
  const char *func_name() const { return "isnotnull"; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
};

class Item_func_like :public Item_bool_func2
{
  char escape;
public:
  Item_func_like(Item *a,Item *b, char* escape_arg) :Item_bool_func2(a,b),escape(*escape_arg)
  {}
  longlong val_int();
  enum Functype functype() const { return LIKE_FUNC; }
  optimize_type select_optimize() const;
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "like"; }
  void fix_length_and_dec();
};

#ifdef USE_REGEX

#include <regex.h>

class Item_func_regex :public Item_bool_func
{
  regex_t preg;
  bool regex_compiled;
  bool regex_is_const;
  String prev_regexp;
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b),
    regex_compiled(0),regex_is_const(0) {}
  ~Item_func_regex();
  longlong val_int();
  bool fix_fields(THD *thd,struct st_table_list *tlist);
  const char *func_name() const { return "regex"; }
};

#else

class Item_func_regex :public Item_bool_func
{
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b) {}
  longlong val_int() { return 0;}
  const char *func_name() const { return "regex"; }
};

#endif /* USE_REGEX */


typedef class Item COND;

class Item_cond :public Item_bool_func
{
protected:
  List<Item> list;
public:
  Item_cond() : Item_bool_func() { const_item_cache=0; }
  Item_cond(Item *i1,Item *i2) :Item_bool_func()
    { list.push_back(i1); list.push_back(i2); }
  ~Item_cond() { list.delete_elements(); }
  bool add(Item *item) { return list.push_back(item); }
  bool fix_fields(THD *,struct st_table_list *);

  enum Type type() const { return COND_ITEM; }
  List<Item>* argument_list() { return &list; }
  table_map used_tables() const;
  void update_used_tables();
  void print(String *str);
  void split_sum_func(List<Item> &fields);
  friend int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds);
};


class Item_cond_and :public Item_cond
{
public:
  Item_cond_and() :Item_cond() {}
  Item_cond_and(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_AND_FUNC; }
  longlong val_int();
  const char *func_name() const { return "and"; }
};

class Item_cond_or :public Item_cond
{
public:
  Item_cond_or() :Item_cond() {}
  Item_cond_or(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_OR_FUNC; }
  longlong val_int();
  const char *func_name() const { return "or"; }
};


/* Some usefull inline functions */

inline Item *and_conds(Item *a,Item *b)
{
  if (!b) return a;
  if (!a) return b;
  Item *cond=new Item_cond_and(a,b);
  if (cond)
    cond->update_used_tables();
  return cond;
}

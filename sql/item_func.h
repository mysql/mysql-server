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


/* Function items used by mysql */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#ifdef HAVE_IEEEFP_H
extern "C"				/* Bug in BSDI include file */
{
#include <ieeefp.h>
}
#endif

class Item_func :public Item_result_field
{
protected:
  Item **args,*tmp_arg[2];
public:
  uint arg_count;
  table_map used_tables_cache;
  bool const_item_cache;
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,EQUAL_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,
		  GE_FUNC,GT_FUNC,FT_FUNC,
		  LIKE_FUNC,NOTLIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
		  COND_AND_FUNC,COND_OR_FUNC,BETWEEN,IN_FUNC,INTERVAL_FUNC};
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP, OPTIMIZE_NULL };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  Item_func(void)
  {
    arg_count=0; with_sum_func=0;
  }
  Item_func(Item *a)
  {
    arg_count=1;
    args=tmp_arg;
    args[0]=a;
    with_sum_func=a->with_sum_func;
  }
  Item_func(Item *a,Item *b)
  {
    arg_count=2;
    args=tmp_arg;
    args[0]=a; args[1]=b;
    with_sum_func=a->with_sum_func || b->with_sum_func;
  }
  Item_func(Item *a,Item *b,Item *c)
  {
    arg_count=0;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*3)))
    {
      arg_count=3;
      args[0]=a; args[1]=b; args[2]=c;
      with_sum_func=a->with_sum_func || b->with_sum_func || c->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c,Item *d)
  {
    arg_count=0;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*4)))
    {
      arg_count=4;
      args[0]=a; args[1]=b; args[2]=c; args[3]=d;
      with_sum_func=a->with_sum_func || b->with_sum_func || c->with_sum_func ||
	d->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c,Item *d,Item* e)
  {
    arg_count=5;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*5)))
    {
      args[0]=a; args[1]=b; args[2]=c; args[3]=d; args[4]=e;
      with_sum_func=a->with_sum_func || b->with_sum_func || c->with_sum_func ||
	d->with_sum_func || e->with_sum_func ;
    }
  }
  Item_func(List<Item> &list);
  ~Item_func() {} /* Nothing to do; Items are freed automaticly */
  bool fix_fields(THD *,struct st_table_list *);
  void make_field(Send_field *field);
  table_map used_tables() const;
  void update_used_tables();
  bool eq(const Item *item) const;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual bool have_rev_func() const { return 0; }
  virtual Item *key_item() const { return args[0]; }
  virtual const char *func_name() const { return "?"; }
  virtual bool const_item() const { return const_item_cache; }
  inline Item **arguments() const { return args; }
  inline uint argument_count() const { return arg_count; }
  inline void remove_arguments() { arg_count=0; }
  virtual void split_sum_func(List<Item> &fields);
  void print(String *str);
  void print_op(String *str);
  void fix_num_length_and_dec();
  inline bool get_arg0_date(TIME *ltime,bool fuzzy_date)
  {
    return (null_value=args[0]->get_date(ltime,fuzzy_date));
  }
  inline bool get_arg0_time(TIME *ltime)
  {
    return (null_value=args[0]->get_time(ltime));
  }
  friend class udf_handler;
};


class Item_real_func :public Item_func
{
public:
  Item_real_func() :Item_func() {}
  Item_real_func(Item *a) :Item_func(a) {}
  Item_real_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_real_func(List<Item> &list) :Item_func(list) {}
  String *val_str(String*str);
  longlong val_int() { return (longlong) val(); }
  enum Item_result result_type () const { return REAL_RESULT; }
  void fix_length_and_dec() { decimals=NOT_FIXED_DEC; max_length=float_length(decimals); }
};

class Item_num_func :public Item_func
{
 protected:
  Item_result hybrid_type;
public:
  Item_num_func(Item *a) :Item_func(a),hybrid_type(REAL_RESULT) {}
  Item_num_func(Item *a,Item *b) :Item_func(a,b),hybrid_type(REAL_RESULT) {}
  String *val_str(String*str);
  longlong val_int() { return (longlong) val(); }
  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_num_op :public Item_func
{
 protected:
  Item_result hybrid_type;
 public:
  Item_num_op(Item *a,Item *b) :Item_func(a,b),hybrid_type(REAL_RESULT) {}
  String *val_str(String*str);
  void print(String *str) { print_op(str); }
  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec() { fix_num_length_and_dec(); find_num_type(); }
  void find_num_type(void);
};


class Item_int_func :public Item_func
{
public:
  Item_int_func() :Item_func() {}
  Item_int_func(Item *a) :Item_func(a) {}
  Item_int_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_int_func(Item *a,Item *b,Item *c) :Item_func(a,b,c) {}
  Item_int_func(List<Item> &list) :Item_func(list) {}
  double val() { return (double) val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_plus :public Item_num_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "+"; }
  double val();
  longlong val_int();
};

class Item_func_minus :public Item_num_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "-"; }
  double val();
  longlong val_int();
};

class Item_func_mul :public Item_num_op
{
public:
  Item_func_mul(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "*"; }
  double val();
  longlong val_int();
};


class Item_func_div :public Item_num_op
{
public:
  Item_func_div(Item *a,Item *b) :Item_num_op(a,b) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "/"; }
  void fix_length_and_dec();
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "%"; }
  void fix_length_and_dec();
};


class Item_func_neg :public Item_num_func
{
public:
  Item_func_neg(Item *a) :Item_num_func(a) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "-"; }
  void fix_length_and_dec();
};


class Item_func_abs :public Item_num_func
{
public:
  Item_func_abs(Item *a) :Item_num_func(a) {}
  const char *func_name() const { return "abs"; }
  double val();
  longlong val_int();
  enum Item_result result_type () const
  { return args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT; }
  void fix_length_and_dec();
};

// A class to handle logaritmic and trigometric functions

class Item_dec_func :public Item_real_func
{
 public:
  Item_dec_func(Item *a) :Item_real_func(a) {}
  Item_dec_func(Item *a,Item *b) :Item_real_func(a,b) {}
  void fix_length_and_dec()
  {
    decimals=6; max_length=float_length(decimals);
    maybe_null=1;
  }
  inline double fix_result(double value)
  {
#ifndef HAVE_FINITE
    return value;
#else
    if (finite(value) && value != POSTFIX_ERROR)
      return value;
    null_value=1;
    return 0.0;
#endif
  }
};

class Item_func_exp :public Item_dec_func
{
public:
  Item_func_exp(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "exp"; }
};

class Item_func_log :public Item_dec_func
{
public:
  Item_func_log(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "log"; }
};


class Item_func_log10 :public Item_dec_func
{
public:
  Item_func_log10(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "log10"; }
};


class Item_func_sqrt :public Item_dec_func
{
public:
  Item_func_sqrt(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "sqrt"; }
};


class Item_func_pow :public Item_dec_func
{
public:
  Item_func_pow(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val();
  const char *func_name() const { return "pow"; }
};


class Item_func_acos :public Item_dec_func
{
 public:
  Item_func_acos(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "acos"; }
};

class Item_func_asin :public Item_dec_func
{
 public:
  Item_func_asin(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "asin"; }
};

class Item_func_atan :public Item_dec_func
{
 public:
  Item_func_atan(Item *a) :Item_dec_func(a) {}
  Item_func_atan(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val();
  const char *func_name() const { return "atan"; }
};

class Item_func_cos :public Item_dec_func
{
 public:
  Item_func_cos(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "cos"; }
};

class Item_func_sin :public Item_dec_func
{
 public:
  Item_func_sin(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "sin"; }
};

class Item_func_tan :public Item_dec_func
{
 public:
  Item_func_tan(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "tan"; }
};

class Item_func_integer :public Item_int_func
{
public:
  inline Item_func_integer(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec();
};


class Item_func_ceiling :public Item_func_integer
{
  Item_func_ceiling();				/* Never called */
public:
  Item_func_ceiling(Item *a) :Item_func_integer(a) {}
  const char *func_name() const { return "ceiling"; }
  longlong val_int();
};

class Item_func_floor :public Item_func_integer
{
public:
  Item_func_floor(Item *a) :Item_func_integer(a) {}
  const char *func_name() const { return "floor"; }
  longlong val_int();
};

/* This handles round and truncate */

class Item_func_round :public Item_real_func
{
  bool truncate;
public:
  Item_func_round(Item *a,Item *b,bool trunc_arg)
    :Item_real_func(a,b),truncate(trunc_arg) {}
  const char *func_name() const { return truncate ? "truncate" : "round"; }
  double val();
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
public:
  Item_func_rand(Item *a) :Item_real_func(a) {}
  Item_func_rand()	  :Item_real_func()  {}
  double val();
  const char *func_name() const { return "rand"; }
  void fix_length_and_dec() { decimals=NOT_FIXED_DEC; max_length=float_length(decimals); }
  bool const_item() const { return 0; }
  table_map used_tables() const { return RAND_TABLE_BIT; }
};


class Item_func_sign :public Item_int_func
{
public:
  Item_func_sign(Item *a) :Item_int_func(a) {}
  const char *func_name() const { return "sign"; }
  longlong val_int();
};


class Item_func_units :public Item_real_func
{
  char *name;
  double mul,add;
 public:
  Item_func_units(char *name_arg,Item *a,double mul_arg,double add_arg)
    :Item_real_func(a),name(name_arg),mul(mul_arg),add(add_arg) {}
  double val();
  const char *func_name() const { return name; }
  void fix_length_and_dec() { decimals=NOT_FIXED_DEC; max_length=float_length(decimals); }
};


class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
public:
  Item_func_min_max(List<Item> &list,int cmp_sign_arg) :Item_func(list),
    cmp_sign(cmp_sign_arg) {}
  double val();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
};

class Item_func_min :public Item_func_min_max
{
public:
  Item_func_min(List<Item> &list) :Item_func_min_max(list,1) {}
  const char *func_name() const { return "least"; }
};

class Item_func_max :public Item_func_min_max
{
public:
  Item_func_max(List<Item> &list) :Item_func_min_max(list,-1) {}
  const char *func_name() const { return "greatest"; }
};


class Item_func_length :public Item_int_func
{
  String value;
public:
  Item_func_length(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "length"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_char_length :public Item_int_func
{
  String value;
public:
  Item_func_char_length(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "char_length"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_locate :public Item_int_func
{
  String value1,value2;
public:
  Item_func_locate(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_func_locate(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  const char *func_name() const { return "locate"; }
  longlong val_int();
  void fix_length_and_dec() { maybe_null=0; max_length=11; }
};


class Item_func_field :public Item_int_func
{
  Item *item;
  String value,tmp;
public:
  Item_func_field(Item *a,List<Item> &list) :Item_int_func(list),item(a) {}
  ~Item_func_field() { delete item; }
  longlong val_int();
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void update_used_tables()
  {
    item->update_used_tables() ; Item_func::update_used_tables();
    used_tables_cache|=item->used_tables();
  }
  const char *func_name() const { return "field"; }
  void fix_length_and_dec()
  { maybe_null=0; max_length=2; used_tables_cache|=item->used_tables();}
};


class Item_func_ascii :public Item_int_func
{
  String value;
public:
  Item_func_ascii(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "ascii"; }
  void fix_length_and_dec() { max_length=3; }
};

class Item_func_ord :public Item_int_func
{
  String value;
public:
  Item_func_ord(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "ord"; }
  void fix_length_and_dec() { max_length=21; }
};

class Item_func_find_in_set :public Item_int_func
{
  String value,value2;
  uint enum_value;
  ulonglong enum_bit;
public:
  Item_func_find_in_set(Item *a,Item *b) :Item_int_func(a,b),enum_value(0) {}
  longlong val_int();
  const char *func_name() const { return "find_in_set"; }
  void fix_length_and_dec();
};


class Item_func_bit_or :public Item_int_func
{
public:
  Item_func_bit_or(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "|"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_bit_and :public Item_int_func
{
public:
  Item_func_bit_and(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "&"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_bit_count :public Item_int_func
{
public:
  Item_func_bit_count(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "bit_count"; }
  void fix_length_and_dec() { decimals=0; max_length=2; }
};

class Item_func_shift_left :public Item_int_func
{
public:
  Item_func_shift_left(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "<<"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_shift_right :public Item_int_func
{
public:
  Item_func_shift_right(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return ">>"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_bit_neg :public Item_int_func
{
public:
  Item_func_bit_neg(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "~"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_set_last_insert_id :public Item_int_func
{
public:
  Item_func_set_last_insert_id(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec() { decimals=0; max_length=args[0]->max_length; }
};

class Item_func_benchmark :public Item_int_func
{
  ulong loop_count;
 public:
  Item_func_benchmark(ulong loop_count_arg,Item *expr)
    :Item_int_func(expr), loop_count(loop_count_arg)
  {}
  longlong val_int();
  const char *func_name() const { return "benchmark"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=0; }
};


#ifdef HAVE_DLOPEN

class Item_udf_func :public Item_func
{
 protected:
  udf_handler udf;

public:
  Item_udf_func(udf_func *udf_arg) :Item_func(), udf(udf_arg) {}
  Item_udf_func(udf_func *udf_arg, List<Item> &list)
    :Item_func(list), udf(udf_arg) {}
  ~Item_udf_func() {}
  const char *func_name() const { return udf.name(); }
  bool fix_fields(THD *thd,struct st_table_list *tables)
  {
    bool res=udf.fix_fields(thd,tables,this,arg_count,args);
    used_tables_cache=udf.used_tables_cache;
    const_item_cache=udf.const_item_cache;
    return res;
  }
  Item_result result_type () const { return udf.result_type(); }
};


class Item_func_udf_float :public Item_udf_func
{
 public:
  Item_func_udf_float(udf_func *udf_arg) :Item_udf_func(udf_arg) {}
  Item_func_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg,list) {}
  ~Item_func_udf_float() {}
  longlong val_int() { return (longlong) Item_func_udf_float::val(); }
  double val();
  String *val_str(String *str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_int :public Item_udf_func
{
public:
  Item_func_udf_int(udf_func *udf_arg) :Item_udf_func(udf_arg) {}
  Item_func_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg,list) {}
  ~Item_func_udf_int() {}
  longlong val_int();
  double val() { return (double) Item_func_udf_int::val_int(); }
  String *val_str(String *str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};


class Item_func_udf_str :public Item_udf_func
{
public:
  Item_func_udf_str(udf_func *udf_arg) :Item_udf_func(udf_arg) {}
  Item_func_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg,list) {}
  ~Item_func_udf_str() {}
  String *val_str(String *);
  double val()
  {
    String *res;  res=val_str(&str_value);
    return res ? atof(res->c_ptr()) : 0.0;
  }
  longlong val_int()
  {
    String *res;  res=val_str(&str_value);
    return res ? strtoll(res->c_ptr(),(char**) 0,10) : (longlong) 0;
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_func_udf_float :public Item_real_func
{
 public:
  Item_func_udf_float(udf_func *udf_arg) :Item_real_func() {}
  Item_func_udf_float(udf_func *udf_arg, List<Item> &list) :Item_real_func(list) {}
  ~Item_func_udf_float() {}
  double val() { return 0.0; }
};


class Item_func_udf_int :public Item_int_func
{
public:
  Item_func_udf_int(udf_func *udf_arg) :Item_int_func() {}
  Item_func_udf_int(udf_func *udf_arg, List<Item> &list) :Item_int_func(list) {}
  ~Item_func_udf_int() {}
  longlong val_int() { return 0; }
};


class Item_func_udf_str :public Item_func
{
public:
  Item_func_udf_str(udf_func *udf_arg) :Item_func() {}
  Item_func_udf_str(udf_func *udf_arg, List<Item> &list)  :Item_func(list) {}
  ~Item_func_udf_str() {}
  String *val_str(String *) { null_value=1; return 0; }
  double val() { null_value=1; return 0.0; }
  longlong val_int() { null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
};

#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

class ULL;
void item_user_lock_init(void);
void item_user_lock_release(ULL *ull);
void item_user_lock_free(void);

class Item_func_get_lock :public Item_int_func
{
  String value;
 public:
  Item_func_get_lock(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "get_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
};

class Item_func_release_lock :public Item_int_func
{
  String value;
 public:
  Item_func_release_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "release_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
};


/* Handling of user definiable variables */

class user_var_entry;

class Item_func_set_user_var :public Item_func
{
  enum Item_result cached_result_type;
  LEX_STRING name;
  user_var_entry *entry;

public:
  Item_func_set_user_var(LEX_STRING a,Item *b): Item_func(b), name(a) {}
  double val();
  longlong val_int();
  String *val_str(String *str);
  void update_hash(void *ptr, uint length, enum Item_result type);
  bool update();
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *thd,struct st_table_list *tables);
  void fix_length_and_dec();
  const char *func_name() const { return "set_user_var"; }
};


class Item_func_get_user_var :public Item_func
{
  LEX_STRING name;
  user_var_entry *entry;

public:
  Item_func_get_user_var(LEX_STRING a): Item_func(), name(a) {}
  user_var_entry *get_entry();
  double val();
  longlong val_int();
  String *val_str(String* str);
  void fix_length_and_dec();
  enum Item_result result_type() const;
  const char *func_name() const { return "get_user_var"; }
};

class Item_func_inet_aton : public Item_int_func
{
public:
   Item_func_inet_aton(Item *a) :Item_int_func(a) {}
   longlong val_int();
   const char *func_name() const { return "inet_aton"; }
   void fix_length_and_dec() { decimals = 0; max_length = 21; maybe_null=1;}
};


/* for fulltext search */
#include <ft_global.h>

class Item_func_match :public Item_real_func
{
public:
  List<Item> fields;
  TABLE *table;
  uint key;
  bool first_call, join_key;
  Item_func_match *master;
  FT_DOCLIST *ft_handler;

  Item_func_match(List<Item> &a, Item *b): Item_real_func(b),
  fields(a), table(0), ft_handler(0), master(0)  {}
  ~Item_func_match()
  {
    if (!master)
    {
      if (ft_handler)
	ft_close_search(ft_handler);
      if(join_key)
        table->file->ft_handler=0;
    }
  }
  const char *func_name() const { return "match"; }
  enum Functype functype() const { return FT_FUNC; }
  void update_used_tables() {}
  bool fix_fields(THD *thd,struct st_table_list *tlist);
  bool eq(const Item *) const;
  double val();
  longlong val_int() { return val()!=0.0; }

  bool fix_index();
  void init_search();
};

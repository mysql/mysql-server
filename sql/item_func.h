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
  Item **args, *tmp_arg[2];
  uint allowed_arg_cols;
public:
  uint arg_count;
  table_map used_tables_cache, not_null_tables_cache;
  bool const_item_cache;
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,EQUAL_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,
		  GE_FUNC,GT_FUNC,FT_FUNC,
		  LIKE_FUNC,NOTLIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
		  COND_AND_FUNC, COND_OR_FUNC, COND_XOR_FUNC,
                  BETWEEN, IN_FUNC, MULT_EQUAL_FUNC,
		  INTERVAL_FUNC, ISNOTNULLTEST_FUNC,
		  SP_EQUALS_FUNC, SP_DISJOINT_FUNC,SP_INTERSECTS_FUNC,
		  SP_TOUCHES_FUNC,SP_CROSSES_FUNC,SP_WITHIN_FUNC,
		  SP_CONTAINS_FUNC,SP_OVERLAPS_FUNC,
		  SP_STARTPOINT,SP_ENDPOINT,SP_EXTERIORRING,
		  SP_POINTN,SP_GEOMETRYN,SP_INTERIORRINGN,
                  NOT_FUNC, NOT_ALL_FUNC,
                  NOW_FUNC, TRIG_COND_FUNC,
                  GUSERVAR_FUNC};
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP, OPTIMIZE_NULL,
                       OPTIMIZE_EQUAL };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  Item_func(void):
    allowed_arg_cols(1), arg_count(0)
  {
    with_sum_func= 0;
  }
  Item_func(Item *a):
    allowed_arg_cols(1), arg_count(1)
  {
    args= tmp_arg;
    args[0]= a;
    with_sum_func= a->with_sum_func;
  }
  Item_func(Item *a,Item *b):
    allowed_arg_cols(1), arg_count(2)
  {
    args= tmp_arg;
    args[0]= a; args[1]= b;
    with_sum_func= a->with_sum_func || b->with_sum_func;
  }
  Item_func(Item *a,Item *b,Item *c):
    allowed_arg_cols(1)
  {
    arg_count= 0;
    if ((args= (Item**) sql_alloc(sizeof(Item*)*3)))
    {
      arg_count= 3;
      args[0]= a; args[1]= b; args[2]= c;
      with_sum_func= a->with_sum_func || b->with_sum_func || c->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c,Item *d):
    allowed_arg_cols(1)
  {
    arg_count= 0;
    if ((args= (Item**) sql_alloc(sizeof(Item*)*4)))
    {
      arg_count= 4;
      args[0]= a; args[1]= b; args[2]= c; args[3]= d;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
	c->with_sum_func || d->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c,Item *d,Item* e):
    allowed_arg_cols(1)
  {
    arg_count= 5;
    if ((args= (Item**) sql_alloc(sizeof(Item*)*5)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d; args[4]= e;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
	c->with_sum_func || d->with_sum_func || e->with_sum_func ;
    }
  }
  Item_func(List<Item> &list);
  // Constructor used for Item_cond_and/or (see Item comment)
  Item_func(THD *thd, Item_func *item);
  bool fix_fields(THD *,struct st_table_list *, Item **ref);
  table_map used_tables() const;
  table_map not_null_tables() const;
  void update_used_tables();
  bool eq(const Item *item, bool binary_cmp) const;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual bool have_rev_func() const { return 0; }
  virtual Item *key_item() const { return args[0]; }
  virtual const char *func_name() const { return "?"; }
  virtual bool const_item() const { return const_item_cache; }
  inline Item **arguments() const { return args; }
  void set_arguments(List<Item> &list);
  inline uint argument_count() const { return arg_count; }
  inline void remove_arguments() { arg_count=0; }
  void split_sum_func(THD *thd, Item **ref_pointer_array, List<Item> &fields);
  void print(String *str);
  void print_op(String *str);
  void print_args(String *str, uint from);
  void fix_num_length_and_dec();
  inline bool get_arg0_date(TIME *ltime, uint fuzzy_date)
  {
    return (null_value=args[0]->get_date(ltime, fuzzy_date));
  }
  inline bool get_arg0_time(TIME *ltime)
  {
    return (null_value=args[0]->get_time(ltime));
  }
  bool is_null() { (void) val_int(); return null_value; }
  void signal_divide_by_null();
  friend class udf_handler;
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg);
  Item *get_tmp_table_item(THD *thd);
  
  bool agg_arg_collations(DTCollation &c, Item **items, uint nitems,
                          uint flags= 0);
  bool agg_arg_collations_for_comparison(DTCollation &c,
                                         Item **items, uint nitems,
                                         uint flags= 0);
  bool agg_arg_charsets(DTCollation &c, Item **items, uint nitems,
                        uint flags= 0);
  bool walk(Item_processor processor, byte *arg);
  Item *transform(Item_transformer transformer, byte *arg);
};


class Item_real_func :public Item_func
{
public:
  Item_real_func() :Item_func() {}
  Item_real_func(Item *a) :Item_func(a) {}
  Item_real_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_real_func(List<Item> &list) :Item_func(list) {}
  String *val_str(String*str);
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return (longlong) val_real(); }
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
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return (longlong) val_real(); }
  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
  bool is_null() { (void) val_real(); return null_value; }
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
  bool is_null() { (void) val_real(); return null_value; }
};


class Item_int_func :public Item_func
{
public:
  Item_int_func() :Item_func() { max_length=21; }
  Item_int_func(Item *a) :Item_func(a) { max_length=21; }
  Item_int_func(Item *a,Item *b) :Item_func(a,b) { max_length=21; }
  Item_int_func(Item *a,Item *b,Item *c) :Item_func(a,b,c) { max_length=21; }
  Item_int_func(List<Item> &list) :Item_func(list) { max_length=21; }
  Item_int_func(THD *thd, Item_int_func *item) :Item_func(thd, item) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() {}
};


class Item_func_signed :public Item_int_func
{
public:
  Item_func_signed(Item *a) :Item_int_func(a) {}
  const char *func_name() const { return "cast_as_signed"; }
  double val_real()
  {
    double tmp= args[0]->val_real();
    null_value= args[0]->null_value;
    return tmp;
  }
  longlong val_int()
  {
    longlong tmp= args[0]->val_int();
    null_value= args[0]->null_value; 
    return tmp;
  }
  void fix_length_and_dec()
  { max_length=args[0]->max_length; unsigned_flag=0; }
  void print(String *str);
};


class Item_func_unsigned :public Item_func_signed
{
public:
  Item_func_unsigned(Item *a) :Item_func_signed(a) {}
  const char *func_name() const { return "cast_as_unsigned"; }
  void fix_length_and_dec()
  { max_length=args[0]->max_length; unsigned_flag=1; }
  void print(String *str);
};


class Item_func_plus :public Item_num_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "+"; }
  double val_real();
  longlong val_int();
};

class Item_func_minus :public Item_num_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "-"; }
  double val_real();
  longlong val_int();
  void fix_length_and_dec();
};


class Item_func_mul :public Item_num_op
{
public:
  Item_func_mul(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "*"; }
  double val_real();
  longlong val_int();
};


class Item_func_div :public Item_num_op
{
public:
  Item_func_div(Item *a,Item *b) :Item_num_op(a,b) {}
  double val_real();
  longlong val_int();
  const char *func_name() const { return "/"; }
  void fix_length_and_dec();
};


class Item_func_int_div :public Item_num_op
{
public:
  Item_func_int_div(Item *a,Item *b) :Item_num_op(a,b)
  { hybrid_type=INT_RESULT; }
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  longlong val_int();
  const char *func_name() const { return "DIV"; }
  void fix_length_and_dec();
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  double val_real();
  longlong val_int();
  const char *func_name() const { return "%"; }
  void fix_length_and_dec();
};


class Item_func_neg :public Item_num_func
{
public:
  Item_func_neg(Item *a) :Item_num_func(a) {}
  double val_real();
  longlong val_int();
  const char *func_name() const { return "-"; }
  void fix_length_and_dec();
};


class Item_func_abs :public Item_num_func
{
public:
  Item_func_abs(Item *a) :Item_num_func(a) {}
  const char *func_name() const { return "abs"; }
  double val_real();
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
    decimals=NOT_FIXED_DEC; max_length=float_length(decimals);
    maybe_null=1;
  }
  inline double fix_result(double value)
  {
#ifndef HAVE_FINITE
    return value;
#else
    /* The following should be safe, even if we compare doubles */
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
  double val_real();
  const char *func_name() const { return "exp"; }
};


class Item_func_ln :public Item_dec_func
{
public:
  Item_func_ln(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "ln"; }
};


class Item_func_log :public Item_dec_func
{
public:
  Item_func_log(Item *a) :Item_dec_func(a) {}
  Item_func_log(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "log"; }
};


class Item_func_log2 :public Item_dec_func
{
public:
  Item_func_log2(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "log2"; }
};


class Item_func_log10 :public Item_dec_func
{
public:
  Item_func_log10(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "log10"; }
};


class Item_func_sqrt :public Item_dec_func
{
public:
  Item_func_sqrt(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "sqrt"; }
};


class Item_func_pow :public Item_dec_func
{
public:
  Item_func_pow(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "pow"; }
};


class Item_func_acos :public Item_dec_func
{
 public:
  Item_func_acos(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "acos"; }
};

class Item_func_asin :public Item_dec_func
{
 public:
  Item_func_asin(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "asin"; }
};

class Item_func_atan :public Item_dec_func
{
 public:
  Item_func_atan(Item *a) :Item_dec_func(a) {}
  Item_func_atan(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "atan"; }
};

class Item_func_cos :public Item_dec_func
{
 public:
  Item_func_cos(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "cos"; }
};

class Item_func_sin :public Item_dec_func
{
 public:
  Item_func_sin(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "sin"; }
};

class Item_func_tan :public Item_dec_func
{
 public:
  Item_func_tan(Item *a) :Item_dec_func(a) {}
  double val_real();
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
  double val_real();
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
  struct rand_struct *rand;
public:
  Item_func_rand(Item *a) :Item_real_func(a), rand(0) {}
  Item_func_rand()	  :Item_real_func() {}
  double val_real();
  const char *func_name() const { return "rand"; }
  bool const_item() const { return 0; }
  void update_used_tables();
  bool fix_fields(THD *thd, struct st_table_list *tables, Item **ref);
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
  double val_real();
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
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg) {}
  double val_real();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
  table_map not_null_tables() const { return 0; }
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

class Item_func_bit_length :public Item_func_length
{
public:
  Item_func_bit_length(Item *a) :Item_func_length(a) {}
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return Item_func_length::val_int()*8; }
  const char *func_name() const { return "bit_length"; }
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

class Item_func_coercibility :public Item_int_func
{
public:
  Item_func_coercibility(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "coercibility"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_locate :public Item_int_func
{
  String value1,value2;
  DTCollation cmp_collation;
public:
  Item_func_locate(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_func_locate(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  const char *func_name() const { return "locate"; }
  longlong val_int();
  void fix_length_and_dec();
  void print(String *str);
};


class Item_func_field :public Item_int_func
{
  String value,tmp;
  Item_result cmp_type;
  DTCollation cmp_collation;
public:
  Item_func_field(List<Item> &list) :Item_int_func(list) {}
  longlong val_int();
  const char *func_name() const { return "field"; }
  void fix_length_and_dec();
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
};

class Item_func_find_in_set :public Item_int_func
{
  String value,value2;
  uint enum_value;
  ulonglong enum_bit;
  DTCollation cmp_collation;
public:
  Item_func_find_in_set(Item *a,Item *b) :Item_int_func(a,b),enum_value(0) {}
  longlong val_int();
  const char *func_name() const { return "find_in_set"; }
  void fix_length_and_dec();
};

/* Base class for all bit functions: '~', '|', '^', '&', '>>', '<<' */

class Item_func_bit: public Item_int_func
{
public:
  Item_func_bit(Item *a, Item *b) :Item_int_func(a, b) {}
  Item_func_bit(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec() { unsigned_flag= 1; }
  void print(String *str) { print_op(str); }
};

class Item_func_bit_or :public Item_func_bit
{
public:
  Item_func_bit_or(Item *a, Item *b) :Item_func_bit(a, b) {}
  longlong val_int();
  const char *func_name() const { return "|"; }
};

class Item_func_bit_and :public Item_func_bit
{
public:
  Item_func_bit_and(Item *a, Item *b) :Item_func_bit(a, b) {}
  longlong val_int();
  const char *func_name() const { return "&"; }
};

class Item_func_bit_count :public Item_int_func
{
public:
  Item_func_bit_count(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "bit_count"; }
  void fix_length_and_dec() { max_length=2; }
};

class Item_func_shift_left :public Item_func_bit
{
public:
  Item_func_shift_left(Item *a, Item *b) :Item_func_bit(a, b) {}
  longlong val_int();
  const char *func_name() const { return "<<"; }
};

class Item_func_shift_right :public Item_func_bit
{
public:
  Item_func_shift_right(Item *a, Item *b) :Item_func_bit(a, b) {}
  longlong val_int();
  const char *func_name() const { return ">>"; }
};

class Item_func_bit_neg :public Item_func_bit
{
public:
  Item_func_bit_neg(Item *a) :Item_func_bit(a) {}
  longlong val_int();
  const char *func_name() const { return "~"; }
  void print(String *str) { Item_func::print(str); }
};


class Item_func_last_insert_id :public Item_int_func
{
public:
  Item_func_last_insert_id() :Item_int_func() {}
  Item_func_last_insert_id(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec() { if (arg_count) max_length= args[0]->max_length; }
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
  void fix_length_and_dec() { max_length=1; maybe_null=0; }
  void print(String *str);
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
  const char *func_name() const { return udf.name(); }
  bool fix_fields(THD *thd, struct st_table_list *tables, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    bool res= udf.fix_fields(thd, tables, this, arg_count, args);
    used_tables_cache= udf.used_tables_cache;
    const_item_cache= udf.const_item_cache;
    fixed= 1;
    return res;
  }
  Item_result result_type () const { return udf.result_type(); }
  table_map not_null_tables() const { return 0; }
};


class Item_func_udf_float :public Item_udf_func
{
 public:
  Item_func_udf_float(udf_func *udf_arg) :Item_udf_func(udf_arg) {}
  Item_func_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg,list) {}
  longlong val_int()
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) Item_func_udf_float::val_real();
  }
  double val_real();
  String *val_str(String *str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_int :public Item_udf_func
{
public:
  Item_func_udf_int(udf_func *udf_arg) :Item_udf_func(udf_arg) {}
  Item_func_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg,list) {}
  longlong val_int();
  double val_real() { return (double) Item_func_udf_int::val_int(); }
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
  String *val_str(String *);
  double val_real()
  {
    int err;
    String *res;  res=val_str(&str_value);
    return res ? my_strntod(res->charset(),(char*) res->ptr(),res->length(),0,&err) : 0.0;
  }
  longlong val_int()
  {
    int err;
    String *res;  res=val_str(&str_value);
    return res ? my_strntoll(res->charset(),res->ptr(),res->length(),10,(char**) 0,&err) : (longlong) 0;
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
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
};


class Item_func_udf_int :public Item_int_func
{
public:
  Item_func_udf_int(udf_func *udf_arg) :Item_int_func() {}
  Item_func_udf_int(udf_func *udf_arg, List<Item> &list) :Item_int_func(list) {}
  longlong val_int() { DBUG_ASSERT(fixed == 1); return 0; }
};


class Item_func_udf_str :public Item_func
{
public:
  Item_func_udf_str(udf_func *udf_arg) :Item_func() {}
  Item_func_udf_str(udf_func *udf_arg, List<Item> &list)  :Item_func(list) {}
  String *val_str(String *)
    { DBUG_ASSERT(fixed == 1); null_value=1; return 0; }
  double val_real() { DBUG_ASSERT(fixed == 1); null_value= 1; return 0.0; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
};

#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

class User_level_lock;
void item_user_lock_init(void);
void item_user_lock_release(User_level_lock *ull);
void item_user_lock_free(void);

class Item_func_get_lock :public Item_int_func
{
  String value;
 public:
  Item_func_get_lock(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "get_lock"; }
  void fix_length_and_dec() { max_length=1; maybe_null=1;}
};

class Item_func_release_lock :public Item_int_func
{
  String value;
 public:
  Item_func_release_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "release_lock"; }
  void fix_length_and_dec() { max_length=1; maybe_null=1;}
};

/* replication functions */

class Item_master_pos_wait :public Item_int_func
{
  String value;
 public:
  Item_master_pos_wait(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_master_pos_wait(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  longlong val_int();
  const char *func_name() const { return "master_pos_wait"; }
  void fix_length_and_dec() { max_length=21; maybe_null=1;}
};


/* Handling of user definiable variables */

class user_var_entry;

class Item_func_set_user_var :public Item_func
{
  enum Item_result cached_result_type;
  LEX_STRING name;
  user_var_entry *entry;
  char buffer[MAX_FIELD_WIDTH];
  String value;
  union
  {
    longlong vint;
    double vreal;
    String *vstr;
  } save_result;
  String save_buff;
  

public:
  Item_func_set_user_var(LEX_STRING a,Item *b)
    :Item_func(b), cached_result_type(INT_RESULT), name(a)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  bool update_hash(void *ptr, uint length, enum Item_result type, 
  		   CHARSET_INFO *cs, Derivation dv);
  bool check();
  bool update();
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *thd, struct st_table_list *tables, Item **ref);
  void fix_length_and_dec();
  void print(String *str);
  void print_as_stmt(String *str);
  const char *func_name() const { return "set_user_var"; }
};


class Item_func_get_user_var :public Item_func
{
  LEX_STRING name;
  user_var_entry *var_entry;

public:
  Item_func_get_user_var(LEX_STRING a):
    Item_func(), name(a) {}
  enum Functype functype() const { return GUSERVAR_FUNC; }
  LEX_STRING get_name() { return name; }
  double val_real();
  longlong val_int();
  String *val_str(String* str);
  void fix_length_and_dec();
  void print(String *str);
  enum Item_result result_type() const;
  /*
    We must always return variables as strings to guard against selects of type
    select @t1:=1,@t1,@t:="hello",@t from foo where (@t1:= t2.b)
  */
  enum_field_types field_type() const  { return MYSQL_TYPE_STRING; }
  const char *func_name() const { return "get_user_var"; }
  bool const_item() const;
  table_map used_tables() const
  { return const_item() ? 0 : RAND_TABLE_BIT; }
  bool eq(const Item *item, bool binary_cmp) const;
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
  uint key, flags;
  bool join_key;
  DTCollation cmp_collation;
  FT_INFO *ft_handler;
  TABLE *table;
  Item_func_match *master;   // for master-slave optimization
  Item *concat;              // Item_func_concat_ws
  String value;              // value of concat
  String search_value;       // key_item()'s value converted to cmp_collation

  Item_func_match(List<Item> &a, uint b): Item_real_func(a), key(0), flags(b),
       join_key(0), ft_handler(0), table(0), master(0), concat(0) { }
  void cleanup()
  {
    DBUG_ENTER("Item_func_match");
    Item_real_func::cleanup();
    if (!master && ft_handler)
    {
      ft_handler->please->close_search(ft_handler);
      ft_handler=0;
      if (join_key)
	table->file->ft_handler=0;
      table->fulltext_searched=0;
    }
    concat= 0;
    DBUG_VOID_RETURN;
  }
  enum Functype functype() const { return FT_FUNC; }
  const char *func_name() const { return "match"; }
  void update_used_tables() {}
  table_map not_null_tables() const { return 0; }
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref);
  bool eq(const Item *, bool binary_cmp) const;
  /* The following should be safe, even if we compare doubles */
  longlong val_int() { DBUG_ASSERT(fixed == 1); return val_real() != 0.0; }
  double val_real();
  void print(String *str);

  bool fix_index();
  void init_search(bool no_order);
};


class Item_func_bit_xor : public Item_func_bit
{
public:
  Item_func_bit_xor(Item *a, Item *b) :Item_func_bit(a, b) {}
  longlong val_int();
  const char *func_name() const { return "^"; }
};

class Item_func_is_free_lock :public Item_int_func
{
  String value;
public:
  Item_func_is_free_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "is_free_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
};

class Item_func_is_used_lock :public Item_int_func
{
  String value;
public:
  Item_func_is_used_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "is_used_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=10; maybe_null=1;}
};

/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR
};


class Item_func_row_count :public Item_int_func
{
public:
  Item_func_row_count() :Item_int_func() {}
  longlong val_int();
  const char *func_name() const { return "row_count"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
};


/*
 *
 * Stored FUNCTIONs
 *
 */

class sp_head;
class sp_name;

class Item_func_sp :public Item_func
{
private:
  sp_name *m_name;
  mutable sp_head *m_sp;

  int execute(Item **itp);

public:

  Item_func_sp(sp_name *name);

  Item_func_sp(sp_name *name, List<Item> &list);

  virtual ~Item_func_sp()
  {}

  const char *func_name() const;

  enum enum_field_types field_type() const;

  Item_result result_type() const;

  longlong val_int()
  {
    return (longlong)Item_func_sp::val_real();
  }

  double val_real()
  {
    Item *it;
    double d;

    if (execute(&it))
    {
      null_value= 1;
      return 0.0;
    }
    d= it->val_real();
    null_value= it->null_value;
    return d;
  }

  String *val_str(String *str)
  {
    Item *it;
    String *s;

    if (execute(&it))
    {
      null_value= 1;
      return NULL;
    }
    s= it->val_str(str);
    null_value= it->null_value;
    return s;
  }

  void fix_length_and_dec();

};


class Item_func_found_rows :public Item_int_func
{
public:
  Item_func_found_rows() :Item_int_func() {}
  longlong val_int();
  const char *func_name() const { return "found_rows"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
};

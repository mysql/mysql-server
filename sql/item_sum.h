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


/* classes for sum functions */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <my_tree.h>

class Item_sum :public Item_result_field
{
public:
  enum Sumfunctype
  { COUNT_FUNC, COUNT_DISTINCT_FUNC, SUM_FUNC, SUM_DISTINCT_FUNC, AVG_FUNC,
    AVG_DISTINCT_FUNC, MIN_FUNC, MAX_FUNC, UNIQUE_USERS_FUNC, STD_FUNC,
    VARIANCE_FUNC, SUM_BIT_FUNC, UDF_SUM_FUNC, GROUP_CONCAT_FUNC
  };

  Item **args, *tmp_args[2];
  uint arg_count;
  bool quick_group;			/* If incremental update of fields */

  void mark_as_sum_func();
  Item_sum() :arg_count(0), quick_group(1) 
  {
    mark_as_sum_func();
  }
  Item_sum(Item *a)
    :args(tmp_args), arg_count(1), quick_group(1)
  {
    args[0]=a;
    mark_as_sum_func();
  }
  Item_sum( Item *a, Item *b )
    :args(tmp_args), arg_count(2), quick_group(1)
  {
    args[0]=a; args[1]=b;
    mark_as_sum_func();
  }
  Item_sum(List<Item> &list);
  //Copy constructor, need to perform subselects with temporary tables
  Item_sum(THD *thd, Item_sum *item);
  enum Type type() const { return SUM_FUNC_ITEM; }
  virtual enum Sumfunctype sum_func () const=0;
  inline bool reset() { clear(); return add(); };
  virtual void clear()= 0;
  virtual bool add()=0;
  /*
    Called when new group is started and results are being saved in
    a temporary table. Similar to reset(), but must also store value in
    result_field. Like reset() it is supposed to reset start value to
    default.
    This set of methods (reult_field(), reset_field, update_field()) of
    Item_sum is used only if quick_group is not null. Otherwise
    copy_or_same() is used to obtain a copy of this item.
  */
  virtual void reset_field()=0;
  /*
    Called for each new value in the group, when temporary table is in use.
    Similar to add(), but uses temporary table field to obtain current value,
    Updated value is then saved in the field.
  */
  virtual void update_field()=0;
  virtual bool keep_field_type(void) const { return 0; }
  virtual void fix_length_and_dec() { maybe_null=1; null_value=1; }
  /*
    This method is used for debug purposes to print the name of an
    item to the debug log. The second use of this method is as
    a helper function of print(), where it is applicable.
    To suit both goals it should return a meaningful,
    distinguishable and sintactically correct string.  This method
    should not be used for runtime type identification, use enum
    {Sum}Functype and Item_func::functype()/Item_sum::sum_func()
    instead.

    NOTE: for Items inherited from Item_sum, func_name() return part of
    function name till first argument (including '(') to make difference in
    names for functions with 'distinct' clause and without 'distinct' and
    also to make printing of items inherited from Item_sum uniform.
  */
  virtual const char *func_name() const= 0;
  virtual Item *result_item(Field *field)
    { return new Item_field(field); }
  table_map used_tables() const { return ~(table_map) 0; } /* Not used */
  bool const_item() const { return 0; }
  bool is_null() { return null_value; }
  void update_used_tables() { }
  void make_field(Send_field *field);
  void print(String *str);
  void fix_num_length_and_dec();
  void no_rows_in_result() { reset(); }
  virtual bool setup(THD *thd) {return 0;}
  virtual void make_unique() {}
  Item *get_tmp_table_item(THD *thd);
  virtual Field *create_tmp_field(bool group, TABLE *table,
                                  uint convert_blob_length);
  bool walk (Item_processor processor, byte *argument);
};


class Item_sum_num :public Item_sum
{
public:
  Item_sum_num() :Item_sum() {}
  Item_sum_num(Item *item_par) :Item_sum(item_par) {}
  Item_sum_num(Item *a, Item* b) :Item_sum(a,b) {}
  Item_sum_num(List<Item> &list) :Item_sum(list) {}
  Item_sum_num(THD *thd, Item_sum_num *item) :Item_sum(thd, item) {}
  bool fix_fields(THD *, Item **);
  longlong val_int()
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) val_real();             /* Real as default */
  }
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *);
  void reset_field();
};


class Item_sum_int :public Item_sum_num
{
public:
  Item_sum_int(Item *item_par) :Item_sum_num(item_par) {}
  Item_sum_int(List<Item> &list) :Item_sum_num(list) {}
  Item_sum_int(THD *thd, Item_sum_int *item) :Item_sum_num(thd, item) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec()
  { decimals=0; max_length=21; maybe_null=null_value=0; }
};


class Item_sum_sum :public Item_sum_num
{
protected:
  Item_result hybrid_type;
  double sum;
  my_decimal dec_buffs[2];
  uint curr_dec_buff;
  void fix_length_and_dec();

public:
  Item_sum_sum(Item *item_par) :Item_sum_num(item_par) {}
  Item_sum_sum(THD *thd, Item_sum_sum *item);
  enum Sumfunctype sum_func () const {return SUM_FUNC;}
  void clear();
  bool add();
  double val_real();
  longlong val_int();
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return hybrid_type; }
  void reset_field();
  void update_field();
  void no_rows_in_result() {}
  const char *func_name() const { return "sum("; }
  Item *copy_or_same(THD* thd);
};



/* Common class for SUM(DISTINCT), AVG(DISTINCT) */

class Unique;

class Item_sum_distinct :public Item_sum_num
{
protected:
  /* storage for the summation result */
  ulonglong count;
  Hybrid_type val;
  /* storage for unique elements */
  Unique *tree;
  TABLE *table;
  enum enum_field_types table_field_type;
  uint tree_key_length;
protected:
  Item_sum_distinct(THD *thd, Item_sum_distinct *item);
public:
  Item_sum_distinct(Item *item_par);
  ~Item_sum_distinct();

  bool setup(THD *thd);
  void clear();
  void cleanup();
  bool add();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  longlong val_int();
  String *val_str(String *str);

  /* XXX: does it need make_unique? */

  enum Sumfunctype sum_func () const { return SUM_DISTINCT_FUNC; }
  void reset_field() {} // not used
  void update_field() {} // not used
  virtual void no_rows_in_result() {}
  void fix_length_and_dec();
  enum Item_result result_type () const { return val.traits->type(); }
  virtual void calculate_val_and_count();
  virtual bool unique_walk_function(void *elem);
};


/*
  Item_sum_sum_distinct - implementation of SUM(DISTINCT expr).
  See also: MySQL manual, chapter 'Adding New Functions To MySQL'
  and comments in item_sum.cc.
*/

class Item_sum_sum_distinct :public Item_sum_distinct
{
private:
  Item_sum_sum_distinct(THD *thd, Item_sum_sum_distinct *item)
    :Item_sum_distinct(thd, item) {}
public:
  Item_sum_sum_distinct(Item *item_arg) :Item_sum_distinct(item_arg) {}

  enum Sumfunctype sum_func () const { return SUM_DISTINCT_FUNC; }
  const char *func_name() const { return "sum(distinct "; }
  Item *copy_or_same(THD* thd) { return new Item_sum_sum_distinct(thd, this); }
};


/* Item_sum_avg_distinct - SELECT AVG(DISTINCT expr) FROM ... */

class Item_sum_avg_distinct: public Item_sum_distinct
{
private:
  Item_sum_avg_distinct(THD *thd, Item_sum_avg_distinct *original)
    :Item_sum_distinct(thd, original) {}
public:
  uint prec_increment;
  Item_sum_avg_distinct(Item *item_arg) : Item_sum_distinct(item_arg) {}

  void fix_length_and_dec();
  virtual void calculate_val_and_count();
  enum Sumfunctype sum_func () const { return AVG_DISTINCT_FUNC; }
  const char *func_name() const { return "avg(distinct "; }
  Item *copy_or_same(THD* thd) { return new Item_sum_avg_distinct(thd, this); }
};


class Item_sum_count :public Item_sum_int
{
  longlong count;
  table_map used_table_cache;

  public:
  Item_sum_count(Item *item_par)
    :Item_sum_int(item_par),count(0),used_table_cache(~(table_map) 0)
  {}
  Item_sum_count(THD *thd, Item_sum_count *item)
    :Item_sum_int(thd, item), count(item->count),
     used_table_cache(item->used_table_cache)
  {}
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_FUNC; }
  void clear();
  void no_rows_in_result() { count=0; }
  bool add();
  void make_const(longlong count_arg) { count=count_arg; used_table_cache=0; }
  longlong val_int();
  void reset_field();
  void cleanup();
  void update_field();
  const char *func_name() const { return "count("; }
  Item *copy_or_same(THD* thd);
};


class TMP_TABLE_PARAM;

class Item_sum_count_distinct :public Item_sum_int
{
  TABLE *table;
  uint32 *field_lengths;
  TMP_TABLE_PARAM *tmp_table_param;
  /*
    If there are no blobs, we can use a tree, which
    is faster than heap table. In that case, we still use the table
    to help get things set up, but we insert nothing in it
  */
  Unique *tree;
  /*
    Following is 0 normal object and pointer to original one for copy 
    (to correctly free resources)
  */
  Item_sum_count_distinct *original;
  uint tree_key_length;


  bool always_null;		// Set to 1 if the result is always NULL


  friend int composite_key_cmp(void* arg, byte* key1, byte* key2);
  friend int simple_str_key_cmp(void* arg, byte* key1, byte* key2);

public:
  Item_sum_count_distinct(List<Item> &list)
    :Item_sum_int(list), table(0), field_lengths(0), tmp_table_param(0),
     tree(0), original(0), always_null(FALSE)
  { quick_group= 0; }
  Item_sum_count_distinct(THD *thd, Item_sum_count_distinct *item)
    :Item_sum_int(thd, item), table(item->table),
     field_lengths(item->field_lengths),
     tmp_table_param(item->tmp_table_param),
     tree(item->tree), original(item), tree_key_length(item->tree_key_length),
     always_null(item->always_null)
  {}
  ~Item_sum_count_distinct();

  void cleanup();

  enum Sumfunctype sum_func () const { return COUNT_DISTINCT_FUNC; }
  void clear();
  bool add();
  longlong val_int();
  void reset_field() { return ;}		// Never called
  void update_field() { return ; }		// Never called
  const char *func_name() const { return "count(distinct "; }
  bool setup(THD *thd);
  void make_unique();
  Item *copy_or_same(THD* thd);
  void no_rows_in_result() {}
};


/* Item to get the value of a stored sum function */

class Item_sum_avg;

class Item_avg_field :public Item_result_field
{
public:
  Field *field;
  Item_result hybrid_type;
  uint f_precision, f_scale, dec_bin_size;
  uint prec_increment;
  Item_avg_field(Item_result res_type, Item_sum_avg *item);
  enum Type type() const { return FIELD_AVG_ITEM; }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  bool is_null() { (void) val_int(); return null_value; }
  String *val_str(String*);
  enum_field_types field_type() const
  {
    return hybrid_type == DECIMAL_RESULT ?
      MYSQL_TYPE_NEWDECIMAL : MYSQL_TYPE_DOUBLE;
  }
  void fix_length_and_dec() {}
  enum Item_result result_type () const { return hybrid_type; }
};


class Item_sum_avg :public Item_sum_sum
{
public:
  ulonglong count;
  uint prec_increment;
  uint f_precision, f_scale, dec_bin_size;

  Item_sum_avg(Item *item_par) :Item_sum_sum(item_par), count(0) {}
  Item_sum_avg(THD *thd, Item_sum_avg *item)
    :Item_sum_sum(thd, item), count(item->count),
    prec_increment(item->prec_increment) {}

  void fix_length_and_dec();
  enum Sumfunctype sum_func () const {return AVG_FUNC;}
  void clear();
  bool add();
  double val_real();
  // In SPs we might force the "wrong" type with select into a declare variable
  longlong val_int() { return (longlong)val_real(); }
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String *str);
  void reset_field();
  void update_field();
  Item *result_item(Field *field)
  { return new Item_avg_field(hybrid_type, this); }
  void no_rows_in_result() {}
  const char *func_name() const { return "avg("; }
  Item *copy_or_same(THD* thd);
  Field *create_tmp_field(bool group, TABLE *table, uint convert_blob_length);
};

class Item_sum_variance;

class Item_variance_field :public Item_result_field
{
public:
  Field *field;
  Item_result hybrid_type;
  uint f_precision0, f_scale0;
  uint f_precision1, f_scale1;
  uint dec_bin_size0, dec_bin_size1;
  uint sample;
  uint prec_increment;
  Item_variance_field(Item_sum_variance *item);
  enum Type type() const {return FIELD_VARIANCE_ITEM; }
  double val_real();
  longlong val_int()
  { /* can't be fix_fields()ed */ return (longlong) val_real(); }
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  bool is_null() { (void) val_int(); return null_value; }
  enum_field_types field_type() const
  {
    return hybrid_type == DECIMAL_RESULT ?
      MYSQL_TYPE_NEWDECIMAL : MYSQL_TYPE_DOUBLE;
  }
  void fix_length_and_dec() {}
  enum Item_result result_type () const { return hybrid_type; }
};


/*
  variance(a) =

  =  sum (ai - avg(a))^2 / count(a) )
  =  sum (ai^2 - 2*ai*avg(a) + avg(a)^2) / count(a)
  =  (sum(ai^2) - sum(2*ai*avg(a)) + sum(avg(a)^2))/count(a) = 
  =  (sum(ai^2) - 2*avg(a)*sum(a) + count(a)*avg(a)^2)/count(a) = 
  =  (sum(ai^2) - 2*sum(a)*sum(a)/count(a) + count(a)*sum(a)^2/count(a)^2 )/count(a) = 
  =  (sum(ai^2) - 2*sum(a)^2/count(a) + sum(a)^2/count(a) )/count(a) = 
  =  (sum(ai^2) - sum(a)^2/count(a))/count(a)
*/

class Item_sum_variance : public Item_sum_num
{
  void fix_length_and_dec();

public:
  Item_result hybrid_type;
  double sum, sum_sqr;
  my_decimal dec_sum[2], dec_sqr[2];
  int cur_dec;
  ulonglong count;
  uint f_precision0, f_scale0;
  uint f_precision1, f_scale1;
  uint dec_bin_size0, dec_bin_size1;
  uint sample;
  uint prec_increment;

  Item_sum_variance(Item *item_par, uint sample_arg) :Item_sum_num(item_par),
    hybrid_type(REAL_RESULT), cur_dec(0), count(0), sample(sample_arg)
    {}
  Item_sum_variance(THD *thd, Item_sum_variance *item);
  enum Sumfunctype sum_func () const { return VARIANCE_FUNC; }
  void clear();
  bool add();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  void reset_field();
  void update_field();
  Item *result_item(Field *field)
  { return new Item_variance_field(this); }
  void no_rows_in_result() {}
  const char *func_name() const
    { return sample ? "var_samp(" : "variance("; }
  Item *copy_or_same(THD* thd);
  Field *create_tmp_field(bool group, TABLE *table, uint convert_blob_length);
  enum Item_result result_type () const { return hybrid_type; }
};

class Item_sum_std;

class Item_std_field :public Item_variance_field
{
public:
  Item_std_field(Item_sum_std *item);
  enum Type type() const { return FIELD_STD_ITEM; }
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return REAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE;}
};

/*
   standard_deviation(a) = sqrt(variance(a))
*/

class Item_sum_std :public Item_sum_variance
{
  public:
  Item_sum_std(Item *item_par, uint sample_arg)
    :Item_sum_variance(item_par, sample_arg) {}
  Item_sum_std(THD *thd, Item_sum_std *item)
    :Item_sum_variance(thd, item)
    {}
  enum Sumfunctype sum_func () const { return STD_FUNC; }
  double val_real();
  Item *result_item(Field *field)
    { return new Item_std_field(this); }
  const char *func_name() const { return "std("; }
  Item *copy_or_same(THD* thd);
  enum Item_result result_type () const { return REAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE;}
};

// This class is a string or number function depending on num_func

class Item_sum_hybrid :public Item_sum
{
protected:
  String value,tmp_value;
  double sum;
  longlong sum_int;
  my_decimal sum_dec;
  Item_result hybrid_type;
  enum_field_types hybrid_field_type;
  int cmp_sign;
  table_map used_table_cache;
  bool was_values;  // Set if we have found at least one row (for max/min only)

  public:
  Item_sum_hybrid(Item *item_par,int sign)
    :Item_sum(item_par), sum(0.0), sum_int(0),
    hybrid_type(INT_RESULT), hybrid_field_type(FIELD_TYPE_LONGLONG),
    cmp_sign(sign), used_table_cache(~(table_map) 0),
    was_values(TRUE)
  { collation.set(&my_charset_bin); }
  Item_sum_hybrid(THD *thd, Item_sum_hybrid *item);
  bool fix_fields(THD *, Item **);
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }

  void clear();
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  void reset_field();
  String *val_str(String *);
  void make_const() { used_table_cache=0; }
  bool keep_field_type(void) const { return 1; }
  enum Item_result result_type () const { return hybrid_type; }
  enum enum_field_types field_type() const { return hybrid_field_type; }
  void update_field();
  void min_max_update_str_field();
  void min_max_update_real_field();
  void min_max_update_int_field();
  void min_max_update_decimal_field();
  void cleanup();
  bool any_value() { return was_values; }
  void no_rows_in_result();
  Field *create_tmp_field(bool group, TABLE *table,
			  uint convert_blob_length);
};


class Item_sum_min :public Item_sum_hybrid
{
public:
  Item_sum_min(Item *item_par) :Item_sum_hybrid(item_par,1) {}
  Item_sum_min(THD *thd, Item_sum_min *item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func () const {return MIN_FUNC;}

  bool add();
  const char *func_name() const { return "min("; }
  Item *copy_or_same(THD* thd);
};


class Item_sum_max :public Item_sum_hybrid
{
public:
  Item_sum_max(Item *item_par) :Item_sum_hybrid(item_par,-1) {}
  Item_sum_max(THD *thd, Item_sum_max *item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func () const {return MAX_FUNC;}

  bool add();
  const char *func_name() const { return "max("; }
  Item *copy_or_same(THD* thd);
};


class Item_sum_bit :public Item_sum_int
{
protected:
  ulonglong reset_bits,bits;

public:
  Item_sum_bit(Item *item_par,ulonglong reset_arg)
    :Item_sum_int(item_par),reset_bits(reset_arg),bits(reset_arg) {}
  Item_sum_bit(THD *thd, Item_sum_bit *item):
    Item_sum_int(thd, item), reset_bits(item->reset_bits), bits(item->bits) {}
  enum Sumfunctype sum_func () const {return SUM_BIT_FUNC;}
  void clear();
  longlong val_int();
  void reset_field();
  void update_field();
  void fix_length_and_dec()
  { decimals= 0; max_length=21; unsigned_flag= 1; maybe_null= null_value= 0; }
};


class Item_sum_or :public Item_sum_bit
{
public:
  Item_sum_or(Item *item_par) :Item_sum_bit(item_par,LL(0)) {}
  Item_sum_or(THD *thd, Item_sum_or *item) :Item_sum_bit(thd, item) {}
  bool add();
  const char *func_name() const { return "bit_or("; }
  Item *copy_or_same(THD* thd);
};


class Item_sum_and :public Item_sum_bit
{
  public:
  Item_sum_and(Item *item_par) :Item_sum_bit(item_par, ULONGLONG_MAX) {}
  Item_sum_and(THD *thd, Item_sum_and *item) :Item_sum_bit(thd, item) {}
  bool add();
  const char *func_name() const { return "bit_and("; }
  Item *copy_or_same(THD* thd);
};

class Item_sum_xor :public Item_sum_bit
{
  public:
  Item_sum_xor(Item *item_par) :Item_sum_bit(item_par,LL(0)) {}
  Item_sum_xor(THD *thd, Item_sum_xor *item) :Item_sum_bit(thd, item) {}
  bool add();
  const char *func_name() const { return "bit_xor("; }
  Item *copy_or_same(THD* thd);
};


/*
  User defined aggregates
*/

#ifdef HAVE_DLOPEN

class Item_udf_sum : public Item_sum
{
protected:
  udf_handler udf;

public:
  Item_udf_sum(udf_func *udf_arg)
    :Item_sum(), udf(udf_arg)
  { quick_group=0; }
  Item_udf_sum(udf_func *udf_arg, List<Item> &list)
    :Item_sum(list), udf(udf_arg)
  { quick_group=0;}
  Item_udf_sum(THD *thd, Item_udf_sum *item)
    :Item_sum(thd, item), udf(item->udf)
  { udf.not_original= TRUE; }
  const char *func_name() const { return udf.name(); }
  bool fix_fields(THD *thd, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    fixed= 1;
    return udf.fix_fields(thd, this, this->arg_count, this->args);
  }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  virtual bool have_field_update(void) const { return 0; }

  void clear();
  bool add();
  void reset_field() {};
  void update_field() {};
  void cleanup();
  void print(String *str);
};


class Item_sum_udf_float :public Item_udf_sum
{
 public:
  Item_sum_udf_float(udf_func *udf_arg)
    :Item_udf_sum(udf_arg) {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg, list) {}
  Item_sum_udf_float(THD *thd, Item_sum_udf_float *item)
    :Item_udf_sum(thd, item) {}
  longlong val_int()
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) Item_sum_udf_float::val_real();
  }
  double val_real();
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
  Item *copy_or_same(THD* thd);
};


class Item_sum_udf_int :public Item_udf_sum
{
public:
  Item_sum_udf_int(udf_func *udf_arg)
    :Item_udf_sum(udf_arg) {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg, list) {}
  Item_sum_udf_int(THD *thd, Item_sum_udf_int *item)
    :Item_udf_sum(thd, item) {}
  longlong val_int();
  double val_real()
    { DBUG_ASSERT(fixed == 1); return (double) Item_sum_udf_int::val_int(); }
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
  Item *copy_or_same(THD* thd);
};


class Item_sum_udf_str :public Item_udf_sum
{
public:
  Item_sum_udf_str(udf_func *udf_arg)
    :Item_udf_sum(udf_arg) {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  Item_sum_udf_str(THD *thd, Item_sum_udf_str *item)
    :Item_udf_sum(thd, item) {}
  String *val_str(String *);
  double val_real()
  {
    int err_not_used;
    char *end_not_used;
    String *res;
    res=val_str(&str_value);
    return res ? my_strntod(res->charset(),(char*) res->ptr(),res->length(),
			    &end_not_used, &err_not_used) : 0.0;
  }
  longlong val_int()
  {
    int err_not_used;
    char *end;
    String *res;
    CHARSET_INFO *cs;

    if (!(res= val_str(&str_value)))
      return 0;                                 /* Null value */
    cs= res->charset();
    end= (char*) res->ptr()+res->length();
    return cs->cset->strtoll10(cs, res->ptr(), &end, &err_not_used);
  }
  my_decimal *val_decimal(my_decimal *dec);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
  Item *copy_or_same(THD* thd);
};


class Item_sum_udf_decimal :public Item_udf_sum
{
public:
  Item_sum_udf_decimal(udf_func *udf_arg)
    :Item_udf_sum(udf_arg) {}
  Item_sum_udf_decimal(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg, list) {}
  Item_sum_udf_decimal(THD *thd, Item_sum_udf_decimal *item)
    :Item_udf_sum(thd, item) {}
  String *val_str(String *);
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
  Item *copy_or_same(THD* thd);
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_sum_udf_float :public Item_sum_num
{
 public:
  Item_sum_udf_float(udf_func *udf_arg)
    :Item_sum_num() {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  Item_sum_udf_float(THD *thd, Item_sum_udf_float *item)
    :Item_sum_num(thd, item) {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
  void clear() {}
  bool add() { return 0; }  
  void update_field() {}
};


class Item_sum_udf_int :public Item_sum_num
{
public:
  Item_sum_udf_int(udf_func *udf_arg)
    :Item_sum_num() {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  Item_sum_udf_int(THD *thd, Item_sum_udf_int *item)
    :Item_sum_num(thd, item) {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return 0; }
  double val_real() { DBUG_ASSERT(fixed == 1); return 0; }
  void clear() {}
  bool add() { return 0; }  
  void update_field() {}
};


class Item_sum_udf_decimal :public Item_sum_num
{
 public:
  Item_sum_udf_decimal(udf_func *udf_arg)
    :Item_sum_num() {}
  Item_sum_udf_decimal(udf_func *udf_arg, List<Item> &list)
    :Item_sum_num() {}
  Item_sum_udf_decimal(THD *thd, Item_sum_udf_float *item)
    :Item_sum_num(thd, item) {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
  my_decimal *val_decimal(my_decimal *) { DBUG_ASSERT(fixed == 1); return 0; }
  void clear() {}
  bool add() { return 0; }
  void update_field() {}
};


class Item_sum_udf_str :public Item_sum_num
{
public:
  Item_sum_udf_str(udf_func *udf_arg)
    :Item_sum_num() {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_sum_num() {}
  Item_sum_udf_str(THD *thd, Item_sum_udf_str *item)
    :Item_sum_num(thd, item) {}
  String *val_str(String *)
    { DBUG_ASSERT(fixed == 1); null_value=1; return 0; }
  double val_real() { DBUG_ASSERT(fixed == 1); null_value=1; return 0.0; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  void clear() {}
  bool add() { return 0; }  
  void update_field() {}
};

#endif /* HAVE_DLOPEN */

class MYSQL_ERROR;

class Item_func_group_concat : public Item_sum
{
  TMP_TABLE_PARAM *tmp_table_param;
  MYSQL_ERROR *warning;
  String result;
  String *separator;
  TREE tree_base;
  TREE *tree;
  TABLE *table;
  ORDER **order;
  Name_resolution_context *context;
  uint arg_count_order;               // total count of ORDER BY items
  uint arg_count_field;               // count of arguments
  uint count_cut_values;
  bool distinct;
  bool warning_for_row;
  bool always_null;
  /*
    Following is 0 normal object and pointer to original one for copy
    (to correctly free resources)
  */
  Item_func_group_concat *original;

  friend int group_concat_key_cmp_with_distinct(void* arg, byte* key1,
					      byte* key2);
  friend int group_concat_key_cmp_with_order(void* arg, byte* key1,
					     byte* key2);
  friend int group_concat_key_cmp_with_distinct_and_order(void* arg,
							byte* key1,
							byte* key2);
  friend int dump_leaf_key(byte* key,
                           element_count count __attribute__((unused)),
			   Item_func_group_concat *group_concat_item);

  bool no_appended;
public:
  Item_func_group_concat(Name_resolution_context *context_arg,
                         bool is_distinct, List<Item> *is_select,
                         SQL_LIST *is_order, String *is_separator);

  Item_func_group_concat(THD *thd, Item_func_group_concat *item);
  ~Item_func_group_concat() {}
  void cleanup();

  enum Sumfunctype sum_func () const {return GROUP_CONCAT_FUNC;}
  const char *func_name() const { return "group_concat"; }
  virtual Item_result result_type () const { return STRING_RESULT; }
  void clear();
  bool add();
  void reset_field() {}                         // not used
  void update_field() {}                        // not used
  bool fix_fields(THD *,Item **);
  bool setup(THD *thd);
  void make_unique();
  double val_real()
  {
    String *res;  res=val_str(&str_value);
    return res ? my_atof(res->c_ptr()) : 0.0;
  }
  longlong val_int()
  {
    String *res;
    char *end_ptr;
    int error;
    if (!(res= val_str(&str_value)))
      return (longlong) 0;
    end_ptr= (char*) res->ptr()+ res->length();
    return my_strtoll10(res->ptr(), &end_ptr, &error);
  }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    return val_decimal_from_string(decimal_value);
  }
  String* val_str(String* str);
  Item *copy_or_same(THD* thd);
  void no_rows_in_result() {}
  void print(String *str);
  virtual bool change_context_processor(byte *cntx)
    { context= (Name_resolution_context *)cntx; return FALSE; }
};

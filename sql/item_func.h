#ifndef ITEM_FUNC_INCLUDED
#define ITEM_FUNC_INCLUDED
/* Copyright (c) 2000, 2016, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/* Function items used by mysql */

#ifdef USE_PRAGMA_INTERFACE
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
  /*
    Allowed numbers of columns in result (usually 1, which means scalar value)
    0 means get this number from first argument
  */
  uint allowed_arg_cols;
  String *val_str_from_val_str_ascii(String *str, String *str2);

  void count_only_length(Item **item, uint nitems);
  void count_real_length(Item **item, uint nitems);
  void count_decimal_length(Item **item, uint nitems);
  void count_datetime_length(Item **item, uint nitems);
  bool count_string_result_length(enum_field_types field_type,
                                  Item **item, uint nitems);
public:
  uint arg_count;
  table_map used_tables_cache, not_null_tables_cache;
  bool const_item_cache;
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,EQUAL_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,
		  GE_FUNC,GT_FUNC,FT_FUNC,
		  LIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
		  COND_AND_FUNC, COND_OR_FUNC, XOR_FUNC,
                  BETWEEN, IN_FUNC, MULT_EQUAL_FUNC,
		  INTERVAL_FUNC, ISNOTNULLTEST_FUNC,
		  SP_EQUALS_FUNC, SP_DISJOINT_FUNC,SP_INTERSECTS_FUNC,
		  SP_TOUCHES_FUNC,SP_CROSSES_FUNC,SP_WITHIN_FUNC,
		  SP_CONTAINS_FUNC,SP_OVERLAPS_FUNC,
		  SP_STARTPOINT,SP_ENDPOINT,SP_EXTERIORRING,
		  SP_POINTN,SP_GEOMETRYN,SP_INTERIORRINGN,
                  NOT_FUNC, NOT_ALL_FUNC,
                  NOW_FUNC, TRIG_COND_FUNC,
                  SUSERVAR_FUNC, GUSERVAR_FUNC, COLLATE_FUNC,
                  EXTRACT_FUNC, CHAR_TYPECAST_FUNC, FUNC_SP, UDF_FUNC,
                  NEG_FUNC, GSYSVAR_FUNC };
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP, OPTIMIZE_NULL,
                       OPTIMIZE_EQUAL };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  Item_func(void):
    allowed_arg_cols(1), arg_count(0)
  {
    with_sum_func= 0;
    with_field= 0;
  }
  Item_func(Item *a):
    allowed_arg_cols(1), arg_count(1)
  {
    args= tmp_arg;
    args[0]= a;
    with_sum_func= a->with_sum_func;
    with_field= a->with_field;
  }
  Item_func(Item *a,Item *b):
    allowed_arg_cols(1), arg_count(2)
  {
    args= tmp_arg;
    args[0]= a; args[1]= b;
    with_sum_func= a->with_sum_func || b->with_sum_func;
    with_field= a->with_field || b->with_field;
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
      with_field= a->with_field || b->with_field || c->with_field;
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
      with_field= a->with_field || b->with_field ||
        c->with_field || d->with_field;
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
      with_field= a->with_field || b->with_field ||
        c->with_field || d->with_field || e->with_field;
    }
  }
  Item_func(List<Item> &list);
  // Constructor used for Item_cond_and/or (see Item comment)
  Item_func(THD *thd, Item_func *item);
  bool fix_fields(THD *, Item **ref);
  void fix_after_pullout(st_select_lex *new_parent, Item **ref);
  void quick_fix_field();
  table_map used_tables() const;
  table_map not_null_tables() const;
  void update_used_tables();
  bool eq(const Item *item, bool binary_cmp) const;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual bool have_rev_func() const { return 0; }
  virtual Item *key_item() const { return args[0]; }
  virtual bool const_item() const { return const_item_cache; }
  inline Item **arguments() const { return args; }
  void set_arguments(List<Item> &list);
  inline uint argument_count() const { return arg_count; }
  inline void remove_arguments() { arg_count=0; }
  void split_sum_func(THD *thd, Item **ref_pointer_array, List<Item> &fields);
  virtual void print(String *str, enum_query_type query_type);
  void print_op(String *str, enum_query_type query_type);
  void print_args(String *str, uint from, enum_query_type query_type);
  inline bool get_arg0_date(MYSQL_TIME *ltime, ulonglong fuzzy_date)
  {
    return (null_value=args[0]->get_date(ltime, fuzzy_date));
  }
  inline bool get_arg0_time(MYSQL_TIME *ltime)
  {
    null_value= args[0]->get_time(ltime);
    DBUG_ASSERT(null_value ||
                ltime->time_type != MYSQL_TIMESTAMP_TIME || ltime->day == 0);
    return null_value;
  }
  bool is_null() { 
    update_null_value();
    return null_value; 
  }
  void signal_divide_by_null();
  friend class udf_handler;
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg);
  Item *get_tmp_table_item(THD *thd);

  my_decimal *val_decimal(my_decimal *);

  void fix_char_length_ulonglong(ulonglong max_char_length_arg)
  {
    ulonglong max_result_length= max_char_length_arg *
                                 collation.collation->mbmaxlen;
    if (max_result_length >= MAX_BLOB_WIDTH)
    {
      max_length= MAX_BLOB_WIDTH;
      maybe_null= 1;
    }
    else
      max_length= (uint32) max_result_length;
  }
  bool agg_arg_charsets(DTCollation &c, Item **items, uint nitems,
                        uint flags, int item_sep)
  {
    return agg_item_charsets(c, func_name(), items, nitems, flags, item_sep);
  }
  /*
    Aggregate arguments for string result, e.g: CONCAT(a,b)
    - convert to @@character_set_connection if all arguments are numbers
    - allow DERIVATION_NONE
  */
  bool agg_arg_charsets_for_string_result(DTCollation &c,
                                          Item **items, uint nitems,
                                          int item_sep= 1)
  {
    return agg_item_charsets_for_string_result(c, func_name(),
                                               items, nitems, item_sep);
  }
  /*
    Aggregate arguments for comparison, e.g: a=b, a LIKE b, a RLIKE b
    - don't convert to @@character_set_connection if all arguments are numbers
    - don't allow DERIVATION_NONE
  */
  bool agg_arg_charsets_for_comparison(DTCollation &c,
                                       Item **items, uint nitems,
                                       int item_sep= 1)
  {
    return agg_item_charsets_for_comparison(c, func_name(),
                                            items, nitems, item_sep);
  }
  /*
    Aggregate arguments for string result, when some comparison
    is involved internally, e.g: REPLACE(a,b,c)
    - convert to @@character_set_connection if all arguments are numbers
    - disallow DERIVATION_NONE
  */
  bool agg_arg_charsets_for_string_result_with_comparison(DTCollation &c,
                                                          Item **items,
                                                          uint nitems,
                                                          int item_sep= 1)
  {
    return agg_item_charsets_for_string_result_with_comparison(c, func_name(),
                                                               items, nitems,
                                                               item_sep);
  }
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg);
  Item *transform(Item_transformer transformer, uchar *arg);
  Item* compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t);
  void traverse_cond(Cond_traverser traverser,
                     void * arg, traverse_order order);
  bool eval_not_null_tables(uchar *opt_arg);
 // bool is_expensive_processor(uchar *arg);
 // virtual bool is_expensive() { return 0; }
  inline void raise_numeric_overflow(const char *type_name)
  {
    char buf[256];
    String str(buf, sizeof(buf), system_charset_info);
    str.length(0);
    print(&str, QT_NO_DATA_EXPANSION);
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), type_name, str.c_ptr_safe());
  }
  inline double raise_float_overflow()
  {
    raise_numeric_overflow("DOUBLE");
    return 0.0;
  }
  inline longlong raise_integer_overflow()
  {
    raise_numeric_overflow(unsigned_flag ? "BIGINT UNSIGNED": "BIGINT");
    return 0;
  }
  inline int raise_decimal_overflow()
  {
    raise_numeric_overflow("DECIMAL");
    return E_DEC_OVERFLOW;
  }
  /**
     Throw an error if the input double number is not finite, i.e. is either
     +/-INF or NAN.
  */
  inline double check_float_overflow(double value)
  {
    return isfinite(value) ? value : raise_float_overflow();
  }
  /**
    Throw an error if the input BIGINT value represented by the
    (longlong value, bool unsigned flag) pair cannot be returned by the
    function, i.e. is not compatible with this Item's unsigned_flag.
  */
  inline longlong check_integer_overflow(longlong value, bool val_unsigned)
  {
    if ((unsigned_flag && !val_unsigned && value < 0) ||
        (!unsigned_flag && val_unsigned &&
         (ulonglong) value > (ulonglong) LONGLONG_MAX))
      return raise_integer_overflow();
    return value;
  }
  /**
     Throw an error if the error code of a DECIMAL operation is E_DEC_OVERFLOW.
  */
  inline int check_decimal_overflow(int error)
  {
    return (error == E_DEC_OVERFLOW) ? raise_decimal_overflow() : error;
  }

  bool has_timestamp_args()
  {
    DBUG_ASSERT(fixed == TRUE);
    for (uint i= 0; i < arg_count; i++)
    {
      if (args[i]->type() == Item::FIELD_ITEM &&
          args[i]->field_type() == MYSQL_TYPE_TIMESTAMP)
        return TRUE;
    }
    return FALSE;
  }

  bool has_date_args()
  {
    DBUG_ASSERT(fixed == TRUE);
    for (uint i= 0; i < arg_count; i++)
    {
      if (args[i]->type() == Item::FIELD_ITEM &&
          (args[i]->field_type() == MYSQL_TYPE_DATE ||
           args[i]->field_type() == MYSQL_TYPE_DATETIME))
        return TRUE;
    }
    return FALSE;
  }

  bool has_time_args()
  {
    DBUG_ASSERT(fixed == TRUE);
    for (uint i= 0; i < arg_count; i++)
    {
      if (args[i]->type() == Item::FIELD_ITEM &&
          (args[i]->field_type() == MYSQL_TYPE_TIME ||
           args[i]->field_type() == MYSQL_TYPE_DATETIME))
        return TRUE;
    }
    return FALSE;
  }

  bool has_datetime_args()
  {
    DBUG_ASSERT(fixed == TRUE);
    for (uint i= 0; i < arg_count; i++)
    {
      if (args[i]->type() == Item::FIELD_ITEM &&
          args[i]->field_type() == MYSQL_TYPE_DATETIME)
        return TRUE;
    }
    return FALSE;
  }

  /*
    By default only substitution for a field whose two different values
    are never equal is allowed in the arguments of a function.
    This is overruled for the direct arguments of comparison functions.
  */ 
  bool subst_argument_checker(uchar **arg) 
  { 
    if (*arg)
    {
      *arg= (uchar *) Item::IDENTITY_SUBST;
      return TRUE;
    }
    return FALSE;
  }

  /*
    We assume the result of any function that has a TIMESTAMP argument to be
    timezone-dependent, since a TIMESTAMP value in both numeric and string
    contexts is interpreted according to the current timezone.
    The only exception is UNIX_TIMESTAMP() which returns the internal
    representation of a TIMESTAMP argument verbatim, and thus does not depend on
    the timezone.
   */
  virtual bool check_valid_arguments_processor(uchar *bool_arg)
  {
    return has_timestamp_args();
  }

  virtual bool find_function_processor (uchar *arg)
  {
    return functype() == *(Functype *) arg;
  }

  void no_rows_in_result()
  {
    for (uint i= 0; i < arg_count; i++)
    {
      args[i]->no_rows_in_result();
    }
  }
  void restore_to_before_no_rows_in_result()
  {
    for (uint i= 0; i < arg_count; i++)
    {
      args[i]->no_rows_in_result();
    }
  }
};


class Item_real_func :public Item_func
{
public:
  Item_real_func() :Item_func() { collation.set_numeric(); }
  Item_real_func(Item *a) :Item_func(a) { collation.set_numeric(); }
  Item_real_func(Item *a,Item *b) :Item_func(a,b) { collation.set_numeric(); }
  Item_real_func(List<Item> &list) :Item_func(list) { collation.set_numeric(); }
  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *decimal_value);
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return (longlong) rint(val_real()); }
  enum Item_result result_type () const { return REAL_RESULT; }
  void fix_length_and_dec()
  { decimals= NOT_FIXED_DEC; max_length= float_length(decimals); }
};


class Item_func_hybrid_result_type: public Item_func
{
  /*
    Helper methods to make sure that the result of
    decimal_op(), str_op() and date_op() is properly synched with null_value.
  */
  bool date_op_with_null_check(MYSQL_TIME *ltime)
  {
     bool rc= date_op(ltime,
                      field_type() == MYSQL_TYPE_TIME ? TIME_TIME_ONLY : 0);
     DBUG_ASSERT(!rc ^ null_value);
     return rc;
  }
  String *str_op_with_null_check(String *str)
  {
    String *res= str_op(str);
    DBUG_ASSERT((res != NULL) ^ null_value);
    return res;
  }
  my_decimal *decimal_op_with_null_check(my_decimal *decimal_buffer)
  {
    my_decimal *res= decimal_op(decimal_buffer);
    DBUG_ASSERT((res != NULL) ^ null_value);
    return res;
  }
protected:
  Item_result cached_result_type;
  void fix_attributes(Item **item, uint nitems);
public:
  Item_func_hybrid_result_type() :Item_func(), cached_result_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_hybrid_result_type(Item *a) :Item_func(a), cached_result_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_hybrid_result_type(Item *a,Item *b)
    :Item_func(a,b), cached_result_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_hybrid_result_type(Item *a,Item *b,Item *c)
    :Item_func(a,b,c), cached_result_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_hybrid_result_type(List<Item> &list)
    :Item_func(list), cached_result_type(REAL_RESULT)
  { collation.set_numeric(); }

  enum Item_result result_type () const { return cached_result_type; }

  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*str);
  bool get_date(MYSQL_TIME *res, ulonglong fuzzy_date);

  /**
     @brief Performs the operation that this functions implements when the
     result type is INT.

     @return The result of the operation.
  */
  virtual longlong int_op()= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is REAL.

     @return The result of the operation.
  */
  virtual double real_op()= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is DECIMAL.

     @param A pointer where the DECIMAL value will be allocated.
     @return 
       - 0 If the result is NULL
       - The same pointer it was given, with the area initialized to the
         result of the operation.
  */
  virtual my_decimal *decimal_op(my_decimal *)= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is a string type.

     @return The result of the operation.
  */
  virtual String *str_op(String *)= 0;

  /**
     @brief Performs the operation that this functions implements when
     field type is a temporal type.
     @return The result of the operation.
  */
  virtual bool date_op(MYSQL_TIME *res, uint fuzzy_date)= 0;

};



class Item_func_hybrid_field_type :public Item_func_hybrid_result_type
{
protected:
  enum_field_types cached_field_type;
public:
  Item_func_hybrid_field_type()
    :Item_func_hybrid_result_type(), cached_field_type(MYSQL_TYPE_DOUBLE)
  {}
  Item_func_hybrid_field_type(Item *a, Item *b)
    :Item_func_hybrid_result_type(a, b), cached_field_type(MYSQL_TYPE_DOUBLE)
  {}
  Item_func_hybrid_field_type(Item *a, Item *b, Item *c)
    :Item_func_hybrid_result_type(a, b, c),
    cached_field_type(MYSQL_TYPE_DOUBLE)
  {}
  Item_func_hybrid_field_type(List<Item> &list)
    :Item_func_hybrid_result_type(list),
    cached_field_type(MYSQL_TYPE_DOUBLE)
  {}
  enum_field_types field_type() const { return cached_field_type; }
};



class Item_func_numhybrid: public Item_func_hybrid_result_type
{
protected:

  inline void fix_decimals()
  {
    DBUG_ASSERT(result_type() == DECIMAL_RESULT);
    if (decimals == NOT_FIXED_DEC)
      set_if_smaller(decimals, max_length - 1);
  }

public:
  Item_func_numhybrid() :Item_func_hybrid_result_type()
  { }
  Item_func_numhybrid(Item *a) :Item_func_hybrid_result_type(a)
  { }
  Item_func_numhybrid(Item *a,Item *b)
    :Item_func_hybrid_result_type(a,b)
  { }
  Item_func_numhybrid(Item *a,Item *b,Item *c)
    :Item_func_hybrid_result_type(a,b,c)
  { }
  Item_func_numhybrid(List<Item> &list)
    :Item_func_hybrid_result_type(list)
  { }
  String *str_op(String *str) { DBUG_ASSERT(0); return 0; }
  bool date_op(MYSQL_TIME *ltime, uint fuzzydate) { DBUG_ASSERT(0); return true; }
};


/* function where type of result detected by first argument */
class Item_func_num1: public Item_func_numhybrid
{
public:
  Item_func_num1(Item *a) :Item_func_numhybrid(a) {}
  Item_func_num1(Item *a, Item *b) :Item_func_numhybrid(a, b) {}
  void fix_length_and_dec();
};


/* Base class for operations like '+', '-', '*' */
class Item_num_op :public Item_func_numhybrid
{
 public:
  Item_num_op(Item *a,Item *b) :Item_func_numhybrid(a, b) {}
  virtual void result_precision()= 0;

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }

  void fix_length_and_dec();
};


class Item_int_func :public Item_func
{
protected:
  bool sargable;
public:
  Item_int_func() :Item_func()
  { collation.set_numeric(); fix_char_length(21); sargable= false; }
  Item_int_func(Item *a) :Item_func(a)
  { collation.set_numeric(); fix_char_length(21); sargable= false; }
  Item_int_func(Item *a,Item *b) :Item_func(a,b)
  { collation.set_numeric(); fix_char_length(21); sargable= false; }
  Item_int_func(Item *a,Item *b,Item *c) :Item_func(a,b,c)
  { collation.set_numeric(); fix_char_length(21); sargable= false; }
  Item_int_func(List<Item> &list) :Item_func(list)
  { collation.set_numeric(); fix_char_length(21); sargable= false; }
  Item_int_func(THD *thd, Item_int_func *item) :Item_func(thd, item)
  { collation.set_numeric(); sargable= false; }
  double val_real();
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() {}
  bool count_sargable_conds(uchar *arg);
};


class Item_func_connection_id :public Item_int_func
{
  longlong value;

public:
  Item_func_connection_id() {}
  const char *func_name() const { return "connection_id"; }
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  bool check_vcol_func_processor(uchar *int_arg) { return TRUE;}
};


class Item_func_signed :public Item_int_func
{
public:
  Item_func_signed(Item *a) :Item_int_func(a)
  {
    unsigned_flag= 0;
  }
  const char *func_name() const { return "cast_as_signed"; }
  longlong val_int();
  longlong val_int_from_str(int *error);
  void fix_length_and_dec()
  {
    fix_char_length(min(args[0]->max_char_length(),
                        MY_INT64_NUM_DECIMAL_DIGITS));
  }
  virtual void print(String *str, enum_query_type query_type);
  uint decimal_precision() const { return args[0]->decimal_precision(); }
};


class Item_func_unsigned :public Item_func_signed
{
public:
  Item_func_unsigned(Item *a) :Item_func_signed(a)
  {
    unsigned_flag= 1;
  }
  const char *func_name() const { return "cast_as_unsigned"; }
  longlong val_int();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_decimal_typecast :public Item_func
{
  my_decimal decimal_value;
public:
  Item_decimal_typecast(Item *a, int len, int dec) :Item_func(a)
  {
    decimals= (uint8) dec;
    collation.set_numeric();
    fix_char_length(my_decimal_precision_to_length_no_truncation(len, dec,
                                                                 unsigned_flag));
  }
  String *val_str(String *str);
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal*);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NEWDECIMAL; }
  void fix_length_and_dec() {}
  const char *func_name() const { return "decimal_typecast"; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_double_typecast :public Item_real_func
{
public:
  Item_double_typecast(Item *a, int len, int dec) :Item_real_func(a)
  {
    decimals=   (uint8)  dec;
    max_length= (uint32) len;
  }
  double val_real();
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE; }
  void fix_length_and_dec() { maybe_null= 1; }
  const char *func_name() const { return "double_typecast"; }
  virtual void print(String *str, enum_query_type query_type);
};



class Item_func_additive_op :public Item_num_op
{
public:
  Item_func_additive_op(Item *a,Item *b) :Item_num_op(a,b) {}
  void result_precision();
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_plus :public Item_func_additive_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  const char *func_name() const { return "+"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
};

class Item_func_minus :public Item_func_additive_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  const char *func_name() const { return "-"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
};


class Item_func_mul :public Item_num_op
{
public:
  Item_func_mul(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "*"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  void result_precision();
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_div :public Item_num_op
{
public:
  uint prec_increment;
  Item_func_div(Item *a,Item *b) :Item_num_op(a,b) {}
  longlong int_op() { DBUG_ASSERT(0); return 0; }
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "/"; }
  void fix_length_and_dec();
  void result_precision();
};


class Item_func_int_div :public Item_int_func
{
public:
  Item_func_int_div(Item *a,Item *b) :Item_int_func(a,b)
  {}
  longlong val_int();
  const char *func_name() const { return "DIV"; }
  void fix_length_and_dec();

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }

  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "%"; }
  void result_precision();
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_neg :public Item_func_num1
{
public:
  Item_func_neg(Item *a) :Item_func_num1(a) {}
  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "-"; }
  enum Functype functype() const   { return NEG_FUNC; }
  void fix_length_and_dec();
  uint decimal_precision() const { return args[0]->decimal_precision(); }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_abs :public Item_func_num1
{
public:
  Item_func_abs(Item *a) :Item_func_num1(a) {}
  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "abs"; }
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};

// A class to handle logarithmic and trigonometric functions

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

class Item_func_cot :public Item_dec_func
{
public:
  Item_func_cot(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "cot"; }
};

class Item_func_integer :public Item_int_func
{
public:
  inline Item_func_integer(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec();
};


class Item_func_int_val :public Item_func_num1
{
public:
  Item_func_int_val(Item *a) :Item_func_num1(a) {}
  void fix_length_and_dec();
};


class Item_func_ceiling :public Item_func_int_val
{
public:
  Item_func_ceiling(Item *a) :Item_func_int_val(a) {}
  const char *func_name() const { return "ceiling"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};


class Item_func_floor :public Item_func_int_val
{
public:
  Item_func_floor(Item *a) :Item_func_int_val(a) {}
  const char *func_name() const { return "floor"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  bool check_vcol_func_processor(uchar *int_arg) { return FALSE;}
};

/* This handles round and truncate */

class Item_func_round :public Item_func_num1
{
  bool truncate;
public:
  Item_func_round(Item *a, Item *b, bool trunc_arg)
    :Item_func_num1(a,b), truncate(trunc_arg) {}
  const char *func_name() const { return truncate ? "truncate" : "round"; }
  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
  struct my_rnd_struct *rand;
  bool first_eval; // TRUE if val_real() is called 1st time
public:
  Item_func_rand(Item *a) :Item_real_func(a), rand(0), first_eval(TRUE) {}
  Item_func_rand()	  :Item_real_func() {}
  double val_real();
  const char *func_name() const { return "rand"; }
  bool const_item() const { return 0; }
  void update_used_tables();
  bool fix_fields(THD *thd, Item **ref);
  void cleanup() { first_eval= TRUE; Item_real_func::cleanup(); }
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
private:
  void seed_random (Item * val);  
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
  void fix_length_and_dec()
  { decimals= NOT_FIXED_DEC; max_length= float_length(decimals); }
};


class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
  /* An item used for issuing warnings while string to DATETIME conversion. */
  Item *compare_as_dates;
protected:
  enum_field_types cached_field_type;
public:
  Item_func_min_max(List<Item> &list,int cmp_sign_arg) :Item_func(list),
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg), compare_as_dates(0) {}
  double val_real();
  longlong val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *res, ulonglong fuzzy_date);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
  enum_field_types field_type() const { return cached_field_type; }
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


/* 
  Objects of this class are used for ROLLUP queries to wrap up 
  each constant item referred to in GROUP BY list. 
*/

class Item_func_rollup_const :public Item_func
{
public:
  Item_func_rollup_const(Item *a) :Item_func(a)
  {
    name= a->name;
    name_length= a->name_length;
  }
  double val_real() { return args[0]->val_real(); }
  longlong val_int() { return args[0]->val_int(); }
  String *val_str(String *str) { return args[0]->val_str(str); }
  my_decimal *val_decimal(my_decimal *dec) { return args[0]->val_decimal(dec); }
  const char *func_name() const { return "rollup_const"; }
  bool const_item() const { return 0; }
  Item_result result_type() const { return args[0]->result_type(); }
  void fix_length_and_dec()
  {
    collation= args[0]->collation;
    max_length= args[0]->max_length;
    decimals=args[0]->decimals; 
    /* The item could be a NULL constant. */
    null_value= args[0]->is_null();
  }
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
  void fix_length_and_dec() { max_length=10; maybe_null= 0; }
  table_map not_null_tables() const { return 0; }
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
  virtual void print(String *str, enum_query_type query_type);
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

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }
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

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
};


class Item_func_last_insert_id :public Item_int_func
{
public:
  Item_func_last_insert_id() :Item_int_func() {}
  Item_func_last_insert_id(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec()
  {
    unsigned_flag= TRUE;
    if (arg_count)
      max_length= args[0]->max_length;
    unsigned_flag=1;
  }
  bool fix_fields(THD *thd, Item **ref);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_benchmark :public Item_int_func
{
public:
  Item_func_benchmark(Item *count_expr, Item *expr)
    :Item_int_func(count_expr, expr)
  {}
  longlong val_int();
  const char *func_name() const { return "benchmark"; }
  void fix_length_and_dec() { max_length=1; maybe_null=0; }
  virtual void print(String *str, enum_query_type query_type);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_sleep :public Item_int_func
{
public:
  Item_func_sleep(Item *a) :Item_int_func(a) {}
  bool const_item() const { return 0; }
  const char *func_name() const { return "sleep"; }
  void update_used_tables()
  {
    Item_int_func::update_used_tables();
    used_tables_cache|= RAND_TABLE_BIT;
  }
  longlong val_int();
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};



#ifdef HAVE_DLOPEN

class Item_udf_func :public Item_func
{
protected:
  udf_handler udf;
  bool is_expensive_processor(uchar *arg) { return TRUE; }

public:
  Item_udf_func(udf_func *udf_arg)
    :Item_func(), udf(udf_arg) {}
  Item_udf_func(udf_func *udf_arg, List<Item> &list)
    :Item_func(list), udf(udf_arg) {}
  const char *func_name() const { return udf.name(); }
  enum Functype functype() const   { return UDF_FUNC; }
  bool fix_fields(THD *thd, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    bool res= udf.fix_fields(thd, this, arg_count, args);
    used_tables_cache= udf.used_tables_cache;
    const_item_cache= udf.const_item_cache;
    fixed= 1;
    return res;
  }
  void fix_num_length_and_dec();
  void update_used_tables() 
  {
    /*
      TODO: Make a member in UDF_INIT and return if a UDF is deterministic or
      not.
      Currently UDF_INIT has a member (const_item) that is an in/out 
      parameter to the init() call.
      The code in udf_handler::fix_fields also duplicates the arguments 
      handling code in Item_func::fix_fields().
      
      The lack of information if a UDF is deterministic makes writing
      a correct update_used_tables() for UDFs impossible.
      One solution to this would be :
       - Add a is_deterministic member of UDF_INIT
       - (optionally) deprecate the const_item member of UDF_INIT
       - Take away the duplicate code from udf_handler::fix_fields() and
         make Item_udf_func call Item_func::fix_fields() to process its 
         arguments as for any other function.
       - Store the deterministic flag returned by <udf>_init into the 
       udf_handler. 
       - Don't implement Item_udf_func::fix_fields, implement
       Item_udf_func::fix_length_and_dec() instead (similar to non-UDF
       functions).
       - Override Item_func::update_used_tables to call 
       Item_func::update_used_tables() and add a RAND_TABLE_BIT to the 
       result of Item_func::update_used_tables() if the UDF is 
       non-deterministic.
       - (optionally) rename RAND_TABLE_BIT to NONDETERMINISTIC_BIT to
       better describe its usage.
       
      The above would require a change of the UDF API.
      Until that change is done here's how the current code works:
      We call Item_func::update_used_tables() only when we know that
      the function depends on real non-const tables and is deterministic.
      This can be done only because we know that the optimizer will
      call update_used_tables() only when there's possibly a new const
      table. So update_used_tables() can only make a Item_func more
      constant than it is currently.
      That's why we don't need to do anything if a function is guaranteed
      to return non-constant (it's non-deterministic) or is already a
      const.
    */  
    if ((used_tables_cache & ~PSEUDO_TABLE_BITS) && 
        !(used_tables_cache & RAND_TABLE_BIT))
    {
      Item_func::update_used_tables();
      if (!const_item_cache && !used_tables_cache)
        used_tables_cache= RAND_TABLE_BIT;
    }
  }
  void cleanup();
  Item_result result_type () const { return udf.result_type(); }
  table_map not_null_tables() const { return 0; }
  bool is_expensive() { return 1; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_udf_float :public Item_udf_func
{
 public:
  Item_func_udf_float(udf_func *udf_arg)
    :Item_udf_func(udf_arg) {}
  Item_func_udf_float(udf_func *udf_arg,
                      List<Item> &list)
    :Item_udf_func(udf_arg, list) {}
  longlong val_int()
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) rint(Item_func_udf_float::val_real());
  }
  my_decimal *val_decimal(my_decimal *dec_buf)
  {
    double res=val_real();
    if (null_value)
      return NULL;
    double2my_decimal(E_DEC_FATAL_ERROR, res, dec_buf);
    return dec_buf;
  }
  double val_real();
  String *val_str(String *str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_int :public Item_udf_func
{
public:
  Item_func_udf_int(udf_func *udf_arg)
    :Item_udf_func(udf_arg) {}
  Item_func_udf_int(udf_func *udf_arg,
                    List<Item> &list)
    :Item_udf_func(udf_arg, list) {}
  longlong val_int();
  double val_real() { return (double) Item_func_udf_int::val_int(); }
  String *val_str(String *str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals= 0; max_length= 21; }
};


class Item_func_udf_decimal :public Item_udf_func
{
public:
  Item_func_udf_decimal(udf_func *udf_arg)
    :Item_udf_func(udf_arg) {}
  Item_func_udf_decimal(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg, list) {}
  longlong val_int();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String *str);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_str :public Item_udf_func
{
public:
  Item_func_udf_str(udf_func *udf_arg)
    :Item_udf_func(udf_arg) {}
  Item_func_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_udf_func(udf_arg, list) {}
  String *val_str(String *);
  double val_real()
  {
    int err_not_used;
    char *end_not_used;
    String *res;
    res= val_str(&str_value);
    return res ? my_strntod(res->charset(),(char*) res->ptr(), 
                            res->length(), &end_not_used, &err_not_used) : 0.0;
  }
  longlong val_int()
  {
    int err_not_used;
    String *res;  res=val_str(&str_value);
    return res ? my_strntoll(res->charset(),res->ptr(),res->length(),10,
                             (char**) 0, &err_not_used) : (longlong) 0;
  }
  my_decimal *val_decimal(my_decimal *dec_buf)
  {
    String *res=val_str(&str_value);
    if (!res)
      return NULL;
    string2my_decimal(E_DEC_FATAL_ERROR, res, dec_buf);
    return dec_buf;
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_func_udf_float :public Item_real_func
{
 public:
  Item_func_udf_float(udf_func *udf_arg)
    :Item_real_func() {}
  Item_func_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_real_func(list) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
};


class Item_func_udf_int :public Item_int_func
{
public:
  Item_func_udf_int(udf_func *udf_arg)
    :Item_int_func() {}
  Item_func_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_int_func(list) {}
  longlong val_int() { DBUG_ASSERT(fixed == 1); return 0; }
};


class Item_func_udf_decimal :public Item_int_func
{
public:
  Item_func_udf_decimal(udf_func *udf_arg)
    :Item_int_func() {}
  Item_func_udf_decimal(udf_func *udf_arg, List<Item> &list)
    :Item_int_func(list) {}
  my_decimal *val_decimal(my_decimal *) { DBUG_ASSERT(fixed == 1); return 0; }
};


class Item_func_udf_str :public Item_func
{
public:
  Item_func_udf_str(udf_func *udf_arg)
    :Item_func() {}
  Item_func_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_func(list) {}
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
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};

class Item_func_release_lock :public Item_int_func
{
  String value;
public:
  Item_func_release_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "release_lock"; }
  void fix_length_and_dec() { max_length=1; maybe_null=1;}
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
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
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


/* Handling of user definable variables */

class user_var_entry;

class Item_func_set_user_var :public Item_func
{
  enum Item_result cached_result_type;
  user_var_entry *entry;
  /*
    The entry_thread_id variable is used:
    1) to skip unnecessary updates of the entry field (see above);
    2) to reset the entry field that was initialized in the other thread
       (for example, an item tree of a trigger that updates user variables
       may be shared between several connections, and the entry_thread_id field
       prevents updates of one connection user variables from a concurrent
       connection calling the same trigger that initially updated some
       user variable it the first connection context).
  */
  my_thread_id entry_thread_id;
  char buffer[MAX_FIELD_WIDTH];
  String value;
  my_decimal decimal_buff;
  bool null_item;
  union
  {
    longlong vint;
    double vreal;
    String *vstr;
    my_decimal *vdec;
  } save_result;

public:
  LEX_STRING name; // keep it public
  Item_func_set_user_var(LEX_STRING a,Item *b)
    :Item_func(b), cached_result_type(INT_RESULT),
     entry(NULL), entry_thread_id(0), name(a)
  {}
  Item_func_set_user_var(THD *thd, Item_func_set_user_var *item)
    :Item_func(thd, item), cached_result_type(item->cached_result_type),
    entry(item->entry), entry_thread_id(item->entry_thread_id),
    value(item->value), decimal_buff(item->decimal_buff),
    null_item(item->null_item), save_result(item->save_result),
    name(item->name)
  {}

  enum Functype functype() const { return SUSERVAR_FUNC; }
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  double val_result();
  longlong val_int_result();
  bool val_bool_result();
  String *str_result(String *str);
  my_decimal *val_decimal_result(my_decimal *);
  bool is_null_result();
  bool update_hash(void *ptr, uint length, enum Item_result type,
  		   CHARSET_INFO *cs, Derivation dv, bool unsigned_arg);
  bool send(Protocol *protocol, String *str_arg);
  void make_field(Send_field *tmp_field);
  bool check(bool use_result_field);
  void save_item_result(Item *item);
  bool update();
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  void print_as_stmt(String *str, enum_query_type query_type);
  const char *func_name() const { return "set_user_var"; }
  int save_in_field(Field *field, bool no_conversions,
                    bool can_use_result_field);
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_in_field(field, no_conversions, 1);
  }
  void save_org_in_field(Field *field) { (void)save_in_field(field, 1, 0); }
  bool register_field_in_read_map(uchar *arg);
  bool register_field_in_bitmap(uchar *arg);
  bool set_entry(THD *thd, bool create_if_not_exists);
  void cleanup();
  bool check_vcol_func_processor(uchar *int_arg) {return TRUE;}
};


class Item_func_get_user_var :public Item_func,
                              private Settable_routine_parameter
{
  user_var_entry *var_entry;
  Item_result m_cached_result_type;

public:
  LEX_STRING name; // keep it public
  Item_func_get_user_var(LEX_STRING a):
    Item_func(), m_cached_result_type(STRING_RESULT), name(a) {}
  enum Functype functype() const { return GUSERVAR_FUNC; }
  LEX_STRING get_name() { return name; }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal*);
  String *val_str(String* str);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  enum Item_result result_type() const;
  /*
    We must always return variables as strings to guard against selects of type
    select @t1:=1,@t1,@t:="hello",@t from foo where (@t1:= t2.b)
  */
  const char *func_name() const { return "get_user_var"; }
  bool const_item() const;
  table_map used_tables() const
  { return const_item() ? 0 : RAND_TABLE_BIT; }
  bool eq(const Item *item, bool binary_cmp) const;
private:
  bool set_value(THD *thd, sp_rcontext *ctx, Item **it);

public:
  Settable_routine_parameter *get_settable_routine_parameter()
  {
    return this;
  }
  bool check_vcol_func_processor(uchar *int_arg) { return TRUE;}
};


/*
  This item represents user variable used as out parameter (e.g in LOAD DATA),
  and it is supposed to be used only for this purprose. So it is simplified
  a lot. Actually you should never obtain its value.

  The only two reasons for this thing being an Item is possibility to store it
  in List<Item> and desire to place this code somewhere near other functions
  working with user variables.
*/
class Item_user_var_as_out_param :public Item
{
  LEX_STRING name;
  user_var_entry *entry;
public:
  Item_user_var_as_out_param(LEX_STRING a) : name(a)
  { set_name(a.str, 0, system_charset_info); }
  /* We should return something different from FIELD_ITEM here */
  enum Type type() const { return STRING_ITEM;}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *decimal_buffer);
  /* fix_fields() binds variable name with its entry structure */
  bool fix_fields(THD *thd, Item **ref);
  void print_for_load(THD *thd, String *str);
  void set_null_value(CHARSET_INFO* cs);
  void set_value(const char *str, uint length, CHARSET_INFO* cs);
};


/* A system variable */

#define GET_SYS_VAR_CACHE_LONG     1
#define GET_SYS_VAR_CACHE_DOUBLE   2
#define GET_SYS_VAR_CACHE_STRING   4

class Item_func_get_system_var :public Item_func
{
  sys_var *var;
  enum_var_type var_type, orig_var_type;
  LEX_STRING component;
  longlong cached_llval;
  double cached_dval;
  String cached_strval;
  bool cached_null_value;
  query_id_t used_query_id;
  uchar cache_present;

public:
  Item_func_get_system_var(sys_var *var_arg, enum_var_type var_type_arg,
                           LEX_STRING *component_arg, const char *name_arg,
                           size_t name_len_arg);
  enum Functype functype() const { return GSYSVAR_FUNC; }
  void update_null_value();
  void fix_length_and_dec();
  void print(String *str, enum_query_type query_type);
  bool const_item() const { return true; }
  table_map used_tables() const { return 0; }
  enum Item_result result_type() const;
  enum_field_types field_type() const;
  double val_real();
  longlong val_int();
  String* val_str(String*);
  my_decimal *val_decimal(my_decimal *dec_buf)
  { return val_decimal_from_real(dec_buf); }
  /* TODO: fix to support views */
  const char *func_name() const { return "get_system_var"; }
  /**
    Indicates whether this system variable is written to the binlog or not.

    Variables are written to the binlog as part of "status_vars" in
    Query_log_event, as an Intvar_log_event, or a Rand_log_event.

    @return true if the variable is written to the binlog, false otherwise.
  */
  bool is_written_to_binlog();
  bool eq(const Item *item, bool binary_cmp) const;

  void cleanup();
  bool check_vcol_func_processor(uchar *int_arg) { return TRUE;}
};


class Item_func_inet_aton : public Item_int_func
{
public:
  Item_func_inet_aton(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "inet_aton"; }
  void fix_length_and_dec() { decimals= 0; max_length= 21; maybe_null= 1; unsigned_flag= 1;}
};


/* for fulltext search */

class Item_func_match :public Item_real_func
{
public:
  uint key, flags;
  bool join_key;
  DTCollation cmp_collation;
  FT_INFO *ft_handler;
  TABLE *table;
  Item_func_match *master;   // for master-slave optimization
  Item *concat_ws;           // Item_func_concat_ws
  String value;              // value of concat_ws
  String search_value;       // key_item()'s value converted to cmp_collation

  Item_func_match(List<Item> &a, uint b): Item_real_func(a), key(0), flags(b),
       join_key(0), ft_handler(0), table(0), master(0), concat_ws(0) { }
  void cleanup()
  {
    DBUG_ENTER("Item_func_match::cleanup");
    Item_real_func::cleanup();
    if (!master && ft_handler)
      ft_handler->please->close_search(ft_handler);
    ft_handler= 0;
    concat_ws= 0;
    table= 0;           // required by Item_func_match::eq()
    DBUG_VOID_RETURN;
  }
  bool is_expensive_processor(uchar *arg) { return TRUE; }
  enum Functype functype() const { return FT_FUNC; }
  const char *func_name() const { return "match"; }
  table_map not_null_tables() const { return 0; }
  bool fix_fields(THD *thd, Item **ref);
  bool eq(const Item *, bool binary_cmp) const;
  /* The following should be safe, even if we compare doubles */
  longlong val_int() { DBUG_ASSERT(fixed == 1); return val_real() != 0.0; }
  double val_real();
  virtual void print(String *str, enum_query_type query_type);

  bool fix_index();
  void init_search(bool no_order);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    /* TODO: consider adding in support for the MATCH-based virtual columns */
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
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
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};

class Item_func_is_used_lock :public Item_int_func
{
  String value;
public:
  Item_func_is_used_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "is_used_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=10; maybe_null=1;}
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};

/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL, ITEM_CAST_DOUBLE
};


class Item_func_row_count :public Item_int_func
{
public:
  Item_func_row_count() :Item_int_func() {}
  longlong val_int();
  const char *func_name() const { return "row_count"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_vcol_func_processor(uchar *int_arg) 
  {

    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


/*
 *
 * Stored FUNCTIONs
 *
 */

class sp_head;
class sp_name;
struct st_sp_security_context;

class Item_func_sp :public Item_func
{
private:
  Name_resolution_context *context;
  sp_name *m_name;
  mutable sp_head *m_sp;
  TABLE *dummy_table;
  uchar result_buf[64];
  /*
     The result field of the concrete stored function.
  */
  Field *sp_result_field;

  bool execute();
  bool execute_impl(THD *thd);
  bool init_result_field(THD *thd);

protected:
  bool is_expensive_processor(uchar *arg)
  { return is_expensive(); }
  
public:

  Item_func_sp(Name_resolution_context *context_arg, sp_name *name);

  Item_func_sp(Name_resolution_context *context_arg,
               sp_name *name, List<Item> &list);

  virtual ~Item_func_sp()
  {}

  void update_used_tables();

  void cleanup();

  const char *func_name() const;

  enum enum_field_types field_type() const;

  Field *tmp_table_field(TABLE *t_arg);

  void make_field(Send_field *tmp_field);

  Item_result result_type() const;

  longlong val_int()
  {
    if (execute())
      return (longlong) 0;
    return sp_result_field->val_int();
  }

  double val_real()
  {
    if (execute())
      return 0.0;
    return sp_result_field->val_real();
  }

  my_decimal *val_decimal(my_decimal *dec_buf)
  {
    if (execute())
      return NULL;
    return sp_result_field->val_decimal(dec_buf);
  }

  String *val_str(String *str)
  {
    String buf;
    char buff[20];
    buf.set(buff, 20, str->charset());
    buf.length(0);
    if (execute())
      return NULL;
    /*
      result_field will set buf pointing to internal buffer
      of the resul_field. Due to this it will change any time
      when SP is executed. In order to prevent occasional
      corruption of returned value, we make here a copy.
    */
    sp_result_field->val_str(&buf);
    str->copy(buf);
    return str;
  }

  void update_null_value()
  { 
    execute();
  }

  virtual bool change_context_processor(uchar *cntx)
    { context= (Name_resolution_context *)cntx; return FALSE; }

  bool sp_check_access(THD * thd);
  virtual enum Functype functype() const { return FUNC_SP; }

  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec(void);
  bool is_expensive();

  inline Field *get_sp_result_field()
  {
    return sp_result_field;
  }

  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
  bool limit_index_condition_pushdown_processor(uchar *opt_arg)
  {
    return TRUE;
  }
};


class Item_func_found_rows :public Item_int_func
{
public:
  Item_func_found_rows() :Item_int_func() {}
  longlong val_int();
  const char *func_name() const { return "found_rows"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


void uuid_short_init();

class Item_func_uuid_short :public Item_int_func
{
public:
  Item_func_uuid_short() :Item_int_func() {}
  const char *func_name() const { return "uuid_short"; }
  longlong val_int();
  void fix_length_and_dec()
  { max_length= 21; unsigned_flag=1; }
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_last_value :public Item_func
{
protected:
  Item *last_value;
public:
  Item_func_last_value(List<Item> &list) :Item_func(list) {}
  double val_real();
  longlong val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return last_value->result_type(); }
  const char *func_name() const { return "last_value"; }
  table_map not_null_tables() const { return 0; }
  enum_field_types field_type() const { return last_value->field_type(); }
  bool const_item() const { return 0; }
  void evaluate_sideeffects();
  void update_used_tables()
  {
    Item_func::update_used_tables();
    maybe_null= last_value->maybe_null;
  }
};


Item *get_system_var(THD *thd, enum_var_type var_type, LEX_STRING name,
                     LEX_STRING component);
extern bool check_reserved_words(LEX_STRING *name);
extern enum_field_types agg_field_type(Item **items, uint nitems);
Item *find_date_time_item(Item **args, uint nargs, uint col);
double my_double_round(double value, longlong dec, bool dec_unsigned,
                       bool truncate);
bool eval_const_cond(COND *cond);

extern bool volatile  mqh_used;

#endif /* ITEM_FUNC_INCLUDED */

#ifndef ITEM_FUNC_INCLUDED
#define ITEM_FUNC_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "item.h"       // Item_result_field
#include "my_decimal.h" // string2my_decimal
#include "set_var.h"    // enum_var_type
#include "sql_udf.h"    // udf_handler

class PT_item_list;

/* Function items used by mysql */

extern void reject_geometry_args(uint arg_count, Item **args,
                                 Item_result_field *me);
void unsupported_json_comparison(size_t arg_count, Item **args,
                                 const char *msg);

class Item_func :public Item_result_field
{
  typedef Item_result_field super;
protected:
  Item **args, *tmp_arg[2];
  /// Value used in calculation of result of const_item()
  bool const_item_cache;
  /*
    Allowed numbers of columns in result (usually 1, which means scalar value)
    0 means get this number from first argument
  */
  uint allowed_arg_cols;
  /// Value used in calculation of result of used_tables()
  table_map used_tables_cache;
  /// Value used in calculation of result of not_null_tables()
  table_map not_null_tables_cache;
public:
  uint arg_count;
  //bool const_item_cache;
  // When updating Functype with new spatial functions,
  // is_spatial_operator() should also be updated.
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,EQUAL_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,
		  GE_FUNC,GT_FUNC,FT_FUNC,
		  LIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
		  COND_AND_FUNC, COND_OR_FUNC, XOR_FUNC,
                  BETWEEN, IN_FUNC, MULT_EQUAL_FUNC,
		  INTERVAL_FUNC, ISNOTNULLTEST_FUNC,
		  SP_EQUALS_FUNC, SP_DISJOINT_FUNC,SP_INTERSECTS_FUNC,
		  SP_TOUCHES_FUNC,SP_CROSSES_FUNC,SP_WITHIN_FUNC,
		  SP_CONTAINS_FUNC,SP_COVEREDBY_FUNC,SP_COVERS_FUNC,
                  SP_OVERLAPS_FUNC,
		  SP_STARTPOINT,SP_ENDPOINT,SP_EXTERIORRING,
		  SP_POINTN,SP_GEOMETRYN,SP_INTERIORRINGN,
                  NOT_FUNC, NOT_ALL_FUNC,
                  NOW_FUNC, TRIG_COND_FUNC,
                  SUSERVAR_FUNC, GUSERVAR_FUNC, COLLATE_FUNC,
                  EXTRACT_FUNC, TYPECAST_FUNC, FUNC_SP, UDF_FUNC,
                  NEG_FUNC, GSYSVAR_FUNC };
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP, OPTIMIZE_NULL,
                       OPTIMIZE_EQUAL };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  Item_func(void):
    allowed_arg_cols(1), arg_count(0)
  {
    args= tmp_arg;
    with_sum_func= 0;
  }

  explicit Item_func(const POS &pos)
    : super(pos), allowed_arg_cols(1), arg_count(0)
  {
    args= tmp_arg;
  }

  Item_func(Item *a):
    allowed_arg_cols(1), arg_count(1)
  {
    args= tmp_arg;
    args[0]= a;
    with_sum_func= a->with_sum_func;
  }
  Item_func(const POS &pos, Item *a): super(pos),
    allowed_arg_cols(1), arg_count(1)
  {
    args= tmp_arg;
    args[0]= a;
  }

  Item_func(Item *a,Item *b):
    allowed_arg_cols(1), arg_count(2)
  {
    args= tmp_arg;
    args[0]= a; args[1]= b;
    with_sum_func= a->with_sum_func || b->with_sum_func;
  }
  Item_func(const POS &pos, Item *a,Item *b): super(pos),
    allowed_arg_cols(1), arg_count(2)
  {
    args= tmp_arg;
    args[0]= a; args[1]= b;
  }

  Item_func(Item *a,Item *b,Item *c):
    allowed_arg_cols(1), arg_count(3)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*3)))
    {
      args[0]= a; args[1]= b; args[2]= c;
      with_sum_func= a->with_sum_func || b->with_sum_func || c->with_sum_func;
    }
    else
      arg_count= 0; // OOM
  }

  Item_func(const POS &pos, Item *a,Item *b,Item *c): super(pos),
    allowed_arg_cols(1), arg_count(3)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*3)))
    {
      args[0]= a; args[1]= b; args[2]= c;
    }
    else
      arg_count= 0; // OOM
  }

  Item_func(Item *a,Item *b,Item *c,Item *d):
    allowed_arg_cols(1), arg_count(4)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*4)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
	c->with_sum_func || d->with_sum_func;
    }
    else
      arg_count= 0; // OOM
  }
  Item_func(const POS &pos, Item *a,Item *b,Item *c,Item *d): super(pos),
    allowed_arg_cols(1), arg_count(4)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*4)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d;
    }
    else
      arg_count= 0; // OOM
  }

  Item_func(Item *a,Item *b,Item *c,Item *d,Item* e):
    allowed_arg_cols(1), arg_count(5)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*5)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d; args[4]= e;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
	c->with_sum_func || d->with_sum_func || e->with_sum_func ;
    }
    else
      arg_count= 0; // OOM
  }
  Item_func(const POS &pos, Item *a, Item *b, Item *c, Item *d, Item* e)
    : super(pos), allowed_arg_cols(1), arg_count(5)
  {
    if ((args= (Item**) sql_alloc(sizeof(Item*)*5)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d; args[4]= e;
    }
    else
      arg_count= 0; // OOM
  }

  Item_func(List<Item> &list);
  Item_func(const POS &pos, PT_item_list *opt_list);

  // Constructor used for Item_cond_and/or (see Item comment)
  Item_func(THD *thd, Item_func *item);

  virtual bool itemize(Parse_context *pc, Item **res);

  bool fix_fields(THD *, Item **ref);
  bool fix_func_arg(THD *, Item **arg);
  virtual void fix_after_pullout(st_select_lex *parent_select,
                                 st_select_lex *removed_select);
  table_map used_tables() const;
  /**
     Returns the pseudo tables depended upon in order to evaluate this
     function expression. The default implementation returns the empty
     set.
  */
  virtual table_map get_initial_pseudo_tables() const { return 0; }
  table_map not_null_tables() const;
  void update_used_tables();
  void set_used_tables(table_map map) { used_tables_cache= map; }
  void set_not_null_tables(table_map map) { not_null_tables_cache= map; }
  bool eq(const Item *item, bool binary_cmp) const;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual bool have_rev_func() const { return 0; }
  virtual Item *key_item() const { return args[0]; }
  virtual bool const_item() const { return const_item_cache; }
  inline Item **arguments() const
  { DBUG_ASSERT(argument_count() > 0); return args; }
  /**
    Copy arguments from list to args array

    @param list           function argument list
    @param context_free   true: for use in context-independent
                          constructors (Item_func(POS,...)) i.e. for use
                          in the parser
  */
  void set_arguments(List<Item> &list, bool context_free);
  inline uint argument_count() const { return arg_count; }
  inline void remove_arguments() { arg_count=0; }
  void split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                      List<Item> &fields);
  virtual void print(String *str, enum_query_type query_type);
  void print_op(String *str, enum_query_type query_type);
  void print_args(String *str, uint from, enum_query_type query_type);
  virtual void fix_num_length_and_dec();
  void count_only_length(Item **item, uint nitems);
  void count_real_length(Item **item, uint nitems);
  void count_decimal_length(Item **item, uint nitems);
  void count_datetime_length(Item **item, uint nitems);
  bool count_string_result_length(enum_field_types field_type,
                                  Item **item, uint nitems);
  bool get_arg0_date(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date)
  {
    return (null_value=args[0]->get_date(ltime, fuzzy_date));
  }
  inline bool get_arg0_time(MYSQL_TIME *ltime)
  {
    return (null_value= args[0]->get_time(ltime));
  }
  bool is_null() { 
    update_null_value();
    return null_value; 
  }
  void signal_divide_by_null();
  void signal_invalid_argument_for_log();
  friend class udf_handler;
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg);
  Item *get_tmp_table_item(THD *thd);

  my_decimal *val_decimal(my_decimal *);

  /**
    Same as save_in_field except that special logic is added
    to handle JSON values. If the target field has JSON type,
    then do NOT first serialize the JSON value into a string form.

    A better solution might be to put this logic into
    Item_func::save_in_field_inner() or even Item::save_in_field_inner().
    But that would mean providing val_json() overrides for
    more Item subclasses. And that feels like pulling on a
    ball of yarn late in the release cycle for 5.7. FIXME.

    @param[out] field  The field to set the value to.
    @retval 0         On success.
    @retval > 0       On error.
  */
  virtual type_conversion_status save_possibly_as_json(Field *field,
                                                       bool no_conversions);

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
  bool walk(Item_processor processor, enum_walk walk, uchar *arg);
  Item *transform(Item_transformer transformer, uchar *arg);
  Item* compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t);
  void traverse_cond(Cond_traverser traverser,
                     void * arg, traverse_order order);
  inline double fix_result(double value)
  {
    if (my_isfinite(value))
      return value;
    null_value=1;
    return 0.0;
  }
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
    return my_isfinite(value) ? value : raise_float_overflow();
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
         (ulonglong) value > (ulonglong) LLONG_MAX))
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
  virtual Item *gc_subst_transformer(uchar *arg);

  /**
    Does essentially the same as THD::change_item_tree, plus
    maintains any necessary any invariants.
  */
  virtual void replace_argument(THD *thd, Item **oldpp, Item *newp);

protected:
  /**
    Whether or not an item should contribute to the filtering effect
    (@see get_filtering_effect()). First it verifies that table
    requirements are satisfied as follows:

     1) The item must refer to a field in 'filter_for_table' in some
        way. This reference may be indirect through any number of
        intermediate items. For example, this item may be an
        Item_cond_and which refers to an Item_func_eq which refers to
        the field.
     2) The item must not refer to other tables than those already
        read and the table in 'filter_for_table'

    Then it contines to other properties as follows:

    Item_funcs represent "<operand1> OP <operand2> [OP ...]". If the
    Item_func is to contribute to the filtering effect, then

    1) one of the operands must be a field from 'filter_for_table' that is not
       in 'fields_to_ignore', and
    2) depending on the Item_func type filtering effect is calculated
       for, one or all [1] of the other operand(s) must be an available
       value, i.e.:
       - a constant, or
       - a constant subquery, or
       - a field value read from a table in 'read_tables', or
       - a second field in 'filter_for_table', or
       - a function that only refers to constants or tables in
         'read_tables', or
       - special case: an implicit value like NULL in the case of
         "field IS NULL". Such Item_funcs have arg_count==1.

    [1] "At least one" for multiple equality (X = Y = Z = ...), "all"
    for the rest (e.g. BETWEEN)

    @param read_tables       Tables earlier in the join sequence.
                             Predicates for table 'filter_for_table' that
                             rely on values from these tables can be part of
                             the filter effect.
    @param filter_for_table  The table we are calculating filter effect for

    @return Item_field that participates in the predicate if none of the
            requirements are broken, NULL otherwise

    @note: This function only applies to items doing comparison, i.e.
    boolean predicates. Unfortunately, some of those items do not
    inherit from Item_bool_func so the member function has to be
    placed in Item_func.
  */
  const Item_field*
    contributes_to_filter(table_map read_tables,
                          table_map filter_for_table,
                          const MY_BITMAP *fields_to_ignore) const;
  /**
    Named parameters are allowed in a parameter list

    The syntax to name parameters in a function call is as follow:
    <code>foo(expr AS named, expr named, expr AS "named", expr "named")</code>
    where "AS" is optional.
    Only UDF function support that syntax.

    @return true if the function item can have named parameters
  */
  virtual bool may_have_named_parameters() const { return false; }
};


class Item_real_func :public Item_func
{
public:
  Item_real_func() :Item_func() { collation.set_numeric(); }
  explicit
  Item_real_func(const POS &pos) :Item_func(pos) { collation.set_numeric(); }

  Item_real_func(Item *a) :Item_func(a) { collation.set_numeric(); }
  Item_real_func(const POS &pos, Item *a) :Item_func(pos, a)
  { collation.set_numeric(); }

  Item_real_func(Item *a,Item *b) :Item_func(a,b) { collation.set_numeric(); }
  Item_real_func(const POS &pos, Item *a,Item *b) :Item_func(pos, a,b)
  { collation.set_numeric(); }

  Item_real_func(List<Item> &list) :Item_func(list) { collation.set_numeric(); }
  Item_real_func(const POS &pos, PT_item_list *list) :Item_func(pos, list)
  { collation.set_numeric(); }

  String *val_str(String*str);
  my_decimal *val_decimal(my_decimal *decimal_value);
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return (longlong) rint(val_real()); }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_real(ltime);
  }
  enum Item_result result_type () const { return REAL_RESULT; }
  void fix_length_and_dec()
  { decimals= NOT_FIXED_DEC; max_length= float_length(decimals); }
};


class Item_func_numhybrid: public Item_func
{
protected:
  Item_result hybrid_type;
public:
  Item_func_numhybrid(Item *a) :Item_func(a), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_numhybrid(const POS &pos, Item *a)
    :Item_func(pos, a), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }

  Item_func_numhybrid(Item *a,Item *b)
    :Item_func(a,b), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_numhybrid(const POS &pos, Item *a,Item *b)
    :Item_func(pos, a,b), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }

  Item_func_numhybrid(List<Item> &list)
    :Item_func(list), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }
  Item_func_numhybrid(const POS &pos, PT_item_list *list)
    :Item_func(pos, list), hybrid_type(REAL_RESULT)
  { collation.set_numeric(); }

  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec();
  void fix_num_length_and_dec();
  virtual void find_num_type()= 0; /* To be called from fix_length_and_dec */

  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*str);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
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
     @brief Performs the operation that this functions implements when the
     result type is MYSQL_TYPE_DATE or MYSQL_TYPE_DATETIME.

     @return The result of the operation.
  */
  virtual bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)= 0;
  virtual bool time_op(MYSQL_TIME *ltime)= 0;
  bool is_null() { update_null_value(); return null_value; }
};

/* function where type of result detected by first argument */
class Item_func_num1: public Item_func_numhybrid
{
public:
  Item_func_num1(Item *a) :Item_func_numhybrid(a) {}
  Item_func_num1(const POS &pos, Item *a) :Item_func_numhybrid(pos, a) {}

  Item_func_num1(Item *a, Item *b) :Item_func_numhybrid(a, b) {}
  Item_func_num1(const POS &pos, Item *a, Item *b)
    :Item_func_numhybrid(pos, a, b)
  {}

  void fix_num_length_and_dec();
  void find_num_type();
  String *str_op(String *str) { DBUG_ASSERT(0); return 0; }
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  { DBUG_ASSERT(0); return 0; }
  bool time_op(MYSQL_TIME *ltime)
  { DBUG_ASSERT(0); return 0; }
};


/* Base class for operations like '+', '-', '*' */
class Item_num_op :public Item_func_numhybrid
{
 public:
  Item_num_op(Item *a,Item *b) :Item_func_numhybrid(a, b) {}
  Item_num_op(const POS &pos, Item *a,Item *b) :Item_func_numhybrid(pos, a, b)
  {}

  virtual void result_precision()= 0;

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }

  void find_num_type();
  String *str_op(String *str) { DBUG_ASSERT(0); return 0; }
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  { DBUG_ASSERT(0); return 0; }
  bool time_op(MYSQL_TIME *ltime)
  { DBUG_ASSERT(0); return 0; }
};


class Item_int_func :public Item_func
{
public:
  Item_int_func() :Item_func()
  { collation.set_numeric(); fix_char_length(21); }
  explicit Item_int_func(const POS &pos) :Item_func(pos)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(Item *a) :Item_func(a)
  { collation.set_numeric(); fix_char_length(21); }
  Item_int_func(const POS &pos, Item *a) :Item_func(pos, a)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(Item *a,Item *b) :Item_func(a,b)
  { collation.set_numeric(); fix_char_length(21); }
  Item_int_func(const POS &pos, Item *a,Item *b) :Item_func(pos, a,b)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(Item *a,Item *b,Item *c) :Item_func(a,b,c)
  { collation.set_numeric(); fix_char_length(21); }
  Item_int_func(const POS &pos, Item *a,Item *b,Item *c) :Item_func(pos, a,b,c)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(Item *a, Item *b, Item *c, Item *d): Item_func(a,b,c,d)
  { collation.set_numeric(); fix_char_length(21); }
  Item_int_func(const POS &pos, Item *a, Item *b, Item *c, Item *d)
    :Item_func(pos,a,b,c,d)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(List<Item> &list) :Item_func(list)
  { collation.set_numeric(); fix_char_length(21); }
  Item_int_func(const POS &pos, PT_item_list *opt_list)
    :Item_func(pos, opt_list)
  { collation.set_numeric(); fix_char_length(21); }

  Item_int_func(THD *thd, Item_int_func *item) :Item_func(thd, item)
  { collation.set_numeric(); }
  double val_real();
  String *val_str(String*str);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() {}
};


class Item_func_connection_id :public Item_int_func
{
  typedef Item_int_func super;

  longlong value;

public:
  Item_func_connection_id(const POS &pos) :Item_int_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  const char *func_name() const { return "connection_id"; }
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  bool check_gcol_func_processor(uchar *int_arg) { return true;}
};


class Item_func_signed :public Item_int_func
{
public:
  Item_func_signed(const POS &pos, Item *a) :Item_int_func(pos, a) 
  {
    unsigned_flag= 0;
  }
  const char *func_name() const { return "cast_as_signed"; }
  longlong val_int();
  longlong val_int_from_str(int *error);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  uint decimal_precision() const { return args[0]->decimal_precision(); }
  enum Functype functype() const { return TYPECAST_FUNC; }
};


class Item_func_unsigned :public Item_func_signed
{
public:
  Item_func_unsigned(const POS &pos, Item *a) :Item_func_signed(pos, a)
  {
    unsigned_flag= 1;
  }
  const char *func_name() const { return "cast_as_unsigned"; }
  longlong val_int();
  virtual void print(String *str, enum_query_type query_type);
  enum Functype functype() const { return TYPECAST_FUNC; }
};


class Item_decimal_typecast :public Item_func
{
  my_decimal decimal_value;
public:
  Item_decimal_typecast(const POS &pos, Item *a, int len, int dec)
    :Item_func(pos, a)
  {
    decimals= dec;
    collation.set_numeric();
    fix_char_length(my_decimal_precision_to_length_no_truncation(len, dec,
                                                                 unsigned_flag));
  }
  String *val_str(String *str);
  double val_real();
  longlong val_int();
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_decimal(ltime);
  }
  my_decimal *val_decimal(my_decimal*);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NEWDECIMAL; }
  void fix_length_and_dec() {};
  const char *func_name() const { return "decimal_typecast"; }
  enum Functype functype() const { return TYPECAST_FUNC; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_additive_op :public Item_num_op
{
public:
  Item_func_additive_op(Item *a,Item *b) :Item_num_op(a,b) {}
  Item_func_additive_op(const POS &pos, Item *a,Item *b) :Item_num_op(pos, a,b)
  {}

  void result_precision();
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_plus :public Item_func_additive_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  Item_func_plus(const POS &pos, Item *a,Item *b)
    :Item_func_additive_op(pos, a, b)
  {}

  const char *func_name() const { return "+"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
};

class Item_func_minus :public Item_func_additive_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  Item_func_minus(const POS &pos, Item *a,Item *b)
    :Item_func_additive_op(pos, a, b)
  {}

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
  Item_func_mul(const POS &pos, Item *a,Item *b) :Item_num_op(pos, a,b) {}

  const char *func_name() const { return "*"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  void result_precision();
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_div :public Item_num_op
{
public:
  uint prec_increment;
  Item_func_div(const POS &pos, Item *a,Item *b) :Item_num_op(pos, a,b) {}
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
  Item_func_int_div(const POS &pos, Item *a,Item *b) :Item_int_func(pos, a,b)
  {}
  longlong val_int();
  const char *func_name() const { return "DIV"; }
  void fix_length_and_dec();

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }

  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  Item_func_mod(const POS &pos, Item *a,Item *b) :Item_num_op(pos, a,b) {}

  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "%"; }
  void result_precision();
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_neg :public Item_func_num1
{
public:
  Item_func_neg(Item *a) :Item_func_num1(a) {}
  Item_func_neg(const POS &pos, Item *a) :Item_func_num1(pos, a) {}

  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "-"; }
  enum Functype functype() const   { return NEG_FUNC; }
  void fix_length_and_dec();
  void fix_num_length_and_dec();
  uint decimal_precision() const { return args[0]->decimal_precision(); }
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_abs :public Item_func_num1
{
public:
  Item_func_abs(const POS &pos, Item *a) :Item_func_num1(pos, a) {}
  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "abs"; }
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


/**
  This is a superclass for Item_func_longfromgeohash and
  Item_func_latfromgeohash, since they share almost all code.
*/
class Item_func_latlongfromgeohash :public Item_real_func
{
private:
  /**
   The lower limit for latitude output value. Normally, this will be
   set to -90.0.
  */
  const double lower_latitude;

  /**
   The upper limit for latitude output value. Normally, this will be
   set to 90.0.
  */
  const double upper_latitude;

  /**
   The lower limit for longitude output value. Normally, this will
   be set to -180.0.
  */
  const double lower_longitude;

  /**
   The upper limit for longitude output value. Normally, this will
   be set to 180.0.
  */
  const double upper_longitude;

  /**
   If this is set to TRUE the algorithm will start decoding on the first bit,
   which decodes a longitude value. If it is FALSE, it will start on the
   second bit which decodes a latitude value.
  */
  const bool start_on_even_bit;
public:
  Item_func_latlongfromgeohash(const POS &pos, Item *a,
                               double lower_latitude, double upper_latitude,
                               double lower_longitude, double upper_longitude,
                               bool start_on_even_bit_arg)
    :Item_real_func(pos, a), lower_latitude(lower_latitude),
    upper_latitude(upper_latitude), lower_longitude(lower_longitude),
    upper_longitude(upper_longitude), start_on_even_bit(start_on_even_bit_arg)
  {}
  double val_real();
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  static bool decode_geohash(String *geohash, double upper_latitude,
                             double lower_latitude, double upper_longitude,
                             double lower_longitude, double *result_latitude,
                             double *result_longitude);
  static double round_latlongitude(double latlongitude, double error_range,
                                   double lower_limit, double upper_limit);
  static bool check_geohash_argument_valid_type(Item *item);
};


/**
  This handles the <double> = ST_LATFROMGEOHASH(<string>) funtion.
  It returns the latitude-part of a geohash, in the range of [-90, 90].
*/
class Item_func_latfromgeohash :public Item_func_latlongfromgeohash
{
public:
  Item_func_latfromgeohash(const POS &pos, Item *a)
    :Item_func_latlongfromgeohash(pos, a, -90.0, 90.0, -180.0, 180.0, false)
  {}

  const char *func_name() const { return "ST_LATFROMGEOHASH"; }
};


/**
  This handles the <double> = ST_LONGFROMGEOHASH(<string>) funtion.
  It returns the longitude-part of a geohash, in the range of [-180, 180].
*/
class Item_func_longfromgeohash :public Item_func_latlongfromgeohash
{
public:
  Item_func_longfromgeohash(const POS &pos, Item *a) 
    :Item_func_latlongfromgeohash(pos, a, -90.0, 90.0, -180.0, 180.0, true)
  {}

  const char *func_name() const { return "ST_LONGFROMGEOHASH"; }
};


// A class to handle logarithmic and trigonometric functions

class Item_dec_func :public Item_real_func
{
 public:
  Item_dec_func(Item *a) :Item_real_func(a) {}
  Item_dec_func(const POS &pos, Item *a) :Item_real_func(pos, a) {}

  Item_dec_func(const POS &pos, Item *a,Item *b) :Item_real_func(pos, a,b) {}
  void fix_length_and_dec();
};

class Item_func_exp :public Item_dec_func
{
public:
  Item_func_exp(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "exp"; }
};


class Item_func_ln :public Item_dec_func
{
public:
  Item_func_ln(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "ln"; }
};


class Item_func_log :public Item_dec_func
{
public:
  Item_func_log(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  Item_func_log(const POS &pos, Item *a, Item *b) :Item_dec_func(pos, a, b) {}
  double val_real();
  const char *func_name() const { return "log"; }
};


class Item_func_log2 :public Item_dec_func
{
public:
  Item_func_log2(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "log2"; }
};


class Item_func_log10 :public Item_dec_func
{
public:
  Item_func_log10(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "log10"; }
};


class Item_func_sqrt :public Item_dec_func
{
public:
  Item_func_sqrt(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "sqrt"; }
};


class Item_func_pow :public Item_dec_func
{
public:
  Item_func_pow(const POS &pos, Item *a, Item *b) :Item_dec_func(pos, a, b) {}
  double val_real();
  const char *func_name() const { return "pow"; }
};


class Item_func_acos :public Item_dec_func
{
public:
  Item_func_acos(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "acos"; }
};

class Item_func_asin :public Item_dec_func
{
public:
  Item_func_asin(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "asin"; }
};

class Item_func_atan :public Item_dec_func
{
public:
  Item_func_atan(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  Item_func_atan(const POS &pos, Item *a, Item *b) :Item_dec_func(pos, a, b) {}
  double val_real();
  const char *func_name() const { return "atan"; }
};

class Item_func_cos :public Item_dec_func
{
public:
  Item_func_cos(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "cos"; }
};

class Item_func_sin :public Item_dec_func
{
public:
  Item_func_sin(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "sin"; }
};

class Item_func_tan :public Item_dec_func
{
public:
  Item_func_tan(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "tan"; }
};

class Item_func_cot :public Item_dec_func
{
public:
  Item_func_cot(const POS &pos, Item *a) :Item_dec_func(pos, a) {}
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
  Item_func_int_val(const POS &pos, Item *a) :Item_func_num1(pos, a) {}
  void fix_num_length_and_dec();
  void find_num_type();
};


class Item_func_ceiling :public Item_func_int_val
{
public:
  Item_func_ceiling(Item *a) :Item_func_int_val(a) {}
  Item_func_ceiling(const POS &pos, Item *a) :Item_func_int_val(pos, a) {}
  const char *func_name() const { return "ceiling"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};


class Item_func_floor :public Item_func_int_val
{
public:
  Item_func_floor(Item *a) :Item_func_int_val(a) {}
  Item_func_floor(const POS &pos, Item *a) :Item_func_int_val(pos, a) {}
  const char *func_name() const { return "floor"; }
  longlong int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg) { return false;}
};

/* This handles round and truncate */

class Item_func_round :public Item_func_num1
{
  bool truncate;
public:
  Item_func_round(Item *a, Item *b, bool trunc_arg)
    :Item_func_num1(a,b), truncate(trunc_arg) {}
  Item_func_round(const POS &pos, Item *a, Item *b, bool trunc_arg)
    :Item_func_num1(pos, a,b), truncate(trunc_arg)
  {}

  const char *func_name() const { return truncate ? "truncate" : "round"; }
  double real_op();
  longlong int_op();
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
  typedef Item_real_func super;

  struct rand_struct *rand;
  bool first_eval; // TRUE if val_real() is called 1st time
public:
  Item_func_rand(const POS &pos, Item *a)
    :Item_real_func(pos, a), rand(0), first_eval(true)
  {}
  explicit
  Item_func_rand(const POS &pos) :Item_real_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  double val_real();
  const char *func_name() const { return "rand"; }
  bool const_item() const { return 0; }
  /**
    This function is non-deterministic and hence depends on the
    'RAND' pseudo-table.
    
    @retval RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const { return RAND_TABLE_BIT; }
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  void cleanup() { first_eval= TRUE; Item_real_func::cleanup(); }
private:
  void seed_random (Item * val);  
};


class Item_func_sign :public Item_int_func
{
public:
  Item_func_sign(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  const char *func_name() const { return "sign"; }
  longlong val_int();
  void fix_length_and_dec();
};


class Item_func_units :public Item_real_func
{
  char *name;
  double mul,add;
public:
  Item_func_units(const POS &pos, char *name_arg, Item *a, double mul_arg,
                  double add_arg)
    :Item_real_func(pos, a),name(name_arg),mul(mul_arg),add(add_arg)
  {}
  double val_real();
  const char *func_name() const { return name; }
  void fix_length_and_dec();
};


class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
  /* TRUE <=> arguments should be compared in the DATETIME context. */
  bool compare_as_dates;
  /* An item used for issuing warnings while string to DATETIME conversion. */
  Item *datetime_item;
protected:
  enum_field_types cached_field_type;
  uint cmp_datetimes(longlong *value);
  uint cmp_times(longlong *value);
public:
  Item_func_min_max(List<Item> &list,int cmp_sign_arg) :Item_func(list), // TODO: remove
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg), compare_as_dates(FALSE),
    datetime_item(0) {}
  Item_func_min_max(const POS &pos, PT_item_list *opt_list, int cmp_sign_arg)
    :Item_func(pos, opt_list),
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg), compare_as_dates(FALSE),
    datetime_item(0)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);  
  void fix_length_and_dec();
  enum Item_result result_type () const
  {
    /*
      If we compare as dates, then:
      - field_type is MYSQL_TYPE_VARSTRING, MYSQL_TYPE_DATETIME
        or MYSQL_TYPE_DATE.
      - cmp_type is INT_RESULT or DECIMAL_RESULT,
        depending on the amount of fractional digits.
      We need to return STRING_RESULT in this case instead of cmp_type.
    */
    return compare_as_dates ? STRING_RESULT : cmp_type;
  }
  enum Item_result cast_to_int_type () const
  {
    /*
      make CAST(LEAST_OR_GREATEST(datetime_expr, varchar_expr))
      return a number in format "YYYMMDDhhmmss".
    */
    return compare_as_dates ? INT_RESULT : result_type();
  }
  enum_field_types field_type() const { return cached_field_type; }
};

class Item_func_min :public Item_func_min_max
{
public:
  Item_func_min(const POS &pos, PT_item_list *opt_list)
    :Item_func_min_max(pos, opt_list, 1)
  {}
  const char *func_name() const { return "least"; }
};

class Item_func_max :public Item_func_min_max
{
public:
  Item_func_max(const POS &pos, PT_item_list *opt_list)
    :Item_func_min_max(pos, opt_list, -1)
  {}
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
    item_name= a->item_name;
  }
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *dec);
  bool val_json(Json_wrapper *result);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return (null_value= args[0]->get_date(ltime, fuzzydate));
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return (null_value= args[0]->get_time(ltime));
  }
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
  enum_field_types field_type() const { return args[0]->field_type(); }
};


class Item_func_length :public Item_int_func
{
  String value;
public:
  Item_func_length(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "length"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_bit_length :public Item_func_length
{
public:
  Item_func_bit_length(const POS &pos, Item *a) :Item_func_length(pos, a) {}
  longlong val_int()
    { DBUG_ASSERT(fixed == 1); return Item_func_length::val_int()*8; }
  const char *func_name() const { return "bit_length"; }
};

class Item_func_char_length :public Item_int_func
{
  String value;
public:
  Item_func_char_length(Item *a) :Item_int_func(a) {}
  Item_func_char_length(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "char_length"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_coercibility :public Item_int_func
{
public:
  Item_func_coercibility(const POS &pos, Item *a) :Item_int_func(pos, a) {}
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
  Item_func_locate(const POS &pos, Item *a, Item *b) :Item_int_func(pos, a, b)
  {}
  Item_func_locate(const POS &pos, Item *a, Item *b, Item *c)
    :Item_int_func(pos, a, b, c)
  {}

  const char *func_name() const { return "locate"; }
  longlong val_int();
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_instr : public Item_func_locate
{
public:
  Item_func_instr(const POS &pos, Item *a, Item *b) :Item_func_locate(pos, a, b)
  {}

  const char *func_name() const { return "instr"; }
};


class Item_func_validate_password_strength :public Item_int_func
{
public:
  Item_func_validate_password_strength(const POS &pos, Item *a)
    :Item_int_func(pos, a)
  {}
  longlong val_int();
  const char *func_name() const { return "validate_password_strength"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_field :public Item_int_func
{
  String value,tmp;
  Item_result cmp_type;
  DTCollation cmp_collation;
public:
  Item_func_field(const POS &pos, PT_item_list *opt_list)
    :Item_int_func(pos, opt_list)
  {}
  longlong val_int();
  const char *func_name() const { return "field"; }
  void fix_length_and_dec();
};


class Item_func_ascii :public Item_int_func
{
  String value;
public:
  Item_func_ascii(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "ascii"; }
  void fix_length_and_dec() { max_length=3; }
};

class Item_func_ord :public Item_int_func
{
  String value;
public:
  Item_func_ord(const POS &pos, Item *a) :Item_int_func(pos, a) {}
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
  Item_func_find_in_set(const POS &pos, Item *a, Item *b)
    :Item_int_func(pos, a, b), enum_value(0)
  {}
  longlong val_int();
  const char *func_name() const { return "find_in_set"; }
  void fix_length_and_dec();
};

/* Base class for all bit functions: '~', '|', '^', '&', '>>', '<<' */

class Item_func_bit: public Item_int_func
{
protected:
  /// @returns Second arg which check_deprecated_bin_op() should check.
  virtual Item* check_deprecated_second_arg() const= 0;
public:
  Item_func_bit(Item *a, Item *b) :Item_int_func(a, b) {}
  Item_func_bit(const POS &pos, Item *a, Item *b) :Item_int_func(pos, a, b) {}

  Item_func_bit(Item *a) :Item_int_func(a) {}
  Item_func_bit(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  void fix_length_and_dec()
  {
    unsigned_flag= 1;
    check_deprecated_bin_op(args[0], check_deprecated_second_arg());
  }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }
};

class Item_func_bit_or :public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return args[1]; }
public:
  Item_func_bit_or(const POS &pos, Item *a, Item *b) :Item_func_bit(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "|"; }
};

class Item_func_bit_and :public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return args[1]; }
public:
  Item_func_bit_and(const POS &pos, Item *a, Item *b) :Item_func_bit(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "&"; }
};

class Item_func_bit_count :public Item_int_func
{
public:
  Item_func_bit_count(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "bit_count"; }
  void fix_length_and_dec()
  {
    max_length=2;
    check_deprecated_bin_op(args[0], NULL);
  }
};

class Item_func_shift_left :public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return NULL; }
public:
  Item_func_shift_left(const POS &pos, Item *a, Item *b)
    :Item_func_bit(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "<<"; }
};

class Item_func_shift_right :public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return NULL; }
public:
  Item_func_shift_right(const POS &pos, Item *a, Item *b)
    :Item_func_bit(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return ">>"; }
};

class Item_func_bit_neg :public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return NULL; }
public:
  Item_func_bit_neg(const POS &pos, Item *a) :Item_func_bit(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "~"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
};


class Item_func_last_insert_id :public Item_int_func
{
  typedef Item_int_func super;
public:
  explicit
  Item_func_last_insert_id(const POS &pos) :Item_int_func(pos) {}
  Item_func_last_insert_id(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec()
  {
    unsigned_flag= TRUE;
    if (arg_count)
      max_length= args[0]->max_length;
  }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_benchmark :public Item_int_func
{
  typedef Item_int_func super;
public:
  Item_func_benchmark(const POS &pos, Item *count_expr, Item *expr)
    :Item_int_func(pos, count_expr, expr)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "benchmark"; }
  void fix_length_and_dec() { max_length=1; maybe_null= true; }
  virtual void print(String *str, enum_query_type query_type);
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


void item_func_sleep_init();
void item_func_sleep_free();

class Item_func_sleep :public Item_int_func
{
  typedef Item_int_func super;
public:
  Item_func_sleep(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  bool const_item() const { return 0; }
  const char *func_name() const { return "sleep"; }
  /**
    This function is non-deterministic and hence depends on the
    'RAND' pseudo-table.
    
    @retval RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const { return RAND_TABLE_BIT; }
  longlong val_int();
};



#ifdef HAVE_DLOPEN

class Item_udf_func :public Item_func
{
  typedef Item_func super;
protected:
  udf_handler udf;
  bool is_expensive_processor(uchar *arg) { return true; }

public:
  Item_udf_func(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_func(pos, opt_list), udf(udf_arg)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
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
  bool is_expensive() { return true; }
  virtual void print(String *str, enum_query_type query_type);

protected:
  virtual bool may_have_named_parameters() const { return true; }
};


class Item_func_udf_float :public Item_udf_func
{
 public:
  Item_func_udf_float(const POS &pos, udf_func *udf_arg,
                      PT_item_list *opt_list)
    :Item_udf_func(pos, udf_arg, opt_list)
  {}
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
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_real(ltime);
  }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_int :public Item_udf_func
{
public:
  Item_func_udf_int(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_func(pos, udf_arg, opt_list)
  {}
  longlong val_int();
  double val_real() { return (double) Item_func_udf_int::val_int(); }
  String *val_str(String *str);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals= 0; max_length= 21; }
};


class Item_func_udf_decimal :public Item_udf_func
{
public:
  Item_func_udf_decimal(const POS &pos, udf_func *udf_arg,
                        PT_item_list *opt_list)
    :Item_udf_func(pos, udf_arg, opt_list)
  {}
  longlong val_int();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String *str);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_decimal(ltime);
  }
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  void fix_length_and_dec();
};


class Item_func_udf_str :public Item_udf_func
{
public:
  Item_func_udf_str(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_func(pos, udf_arg, opt_list)
  {}

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
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
};

#endif /* HAVE_DLOPEN */

void mysql_ull_cleanup(THD *thd);
void mysql_ull_set_explicit_lock_duration(THD *thd);

class Item_func_get_lock :public Item_int_func
{
  typedef Item_int_func super;

  String value;
 public:
  Item_func_get_lock(const POS &pos, Item *a, Item *b) :Item_int_func(pos, a, b)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "get_lock"; }
  void fix_length_and_dec() { max_length=1; maybe_null=1;}
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
  virtual uint decimal_precision() const { return max_length; }
};

class Item_func_release_lock :public Item_int_func
{
  typedef Item_int_func super;

  String value;
public:
  Item_func_release_lock(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  virtual bool itemize(Parse_context *pc, Item **res);

  longlong val_int();
  const char *func_name() const { return "release_lock"; }
  void fix_length_and_dec() { max_length=1; maybe_null=1;}
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
  virtual uint decimal_precision() const { return max_length; }
};

class Item_func_release_all_locks :public Item_int_func
{
  typedef Item_int_func super;

public:
  explicit Item_func_release_all_locks(const POS &pos) :Item_int_func(pos) {}
  virtual bool itemize(Parse_context *pc, Item **res);

  longlong val_int();
  const char *func_name() const { return "release_all_locks"; }
  void fix_length_and_dec() { unsigned_flag= TRUE; }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

/* replication functions */

class Item_master_pos_wait :public Item_int_func
{
  typedef Item_int_func super;
  String value;
public:
  Item_master_pos_wait(const POS &pos, Item *a, Item *b)
    :Item_int_func(pos, a, b)
  {}
  Item_master_pos_wait(const POS &pos, Item *a, Item *b, Item *c)
    :Item_int_func(pos, a, b, c)
  {}
  Item_master_pos_wait(const POS &pos, Item *a, Item *b, Item *c, Item *d)
    :Item_int_func(pos, a, b, c, d)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "master_pos_wait"; }
  void fix_length_and_dec() { max_length=21; maybe_null=1;}
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

/**
  This class is used for implementing the new wait_for_executed_gtid_set
  function and the functions related to them. This new function is independent
  of the slave threads.
*/
class Item_wait_for_executed_gtid_set :public Item_int_func
{
  typedef Item_int_func super;

  String value;
public:
  Item_wait_for_executed_gtid_set(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  Item_wait_for_executed_gtid_set(const POS &pos, Item *a, Item *b)
    :Item_int_func(pos, a, b)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "wait_for_executed_gtid_set"; }
  void fix_length_and_dec() { max_length= 21; maybe_null= 1; }
};

class Item_master_gtid_set_wait :public Item_int_func
{
  typedef Item_int_func super;

  String value;
public:
  Item_master_gtid_set_wait(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  Item_master_gtid_set_wait(const POS &pos, Item *a, Item *b)
    :Item_int_func(pos, a, b)
  {}
  Item_master_gtid_set_wait(const POS &pos, Item *a, Item *b, Item *c)
    :Item_int_func(pos, a, b, c)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "wait_until_sql_thread_after_gtids"; }
  void fix_length_and_dec() { max_length= 21; maybe_null= 1; }
};

class Item_func_gtid_subset : public Item_int_func
{
  String buf1;
  String buf2;
public:
  Item_func_gtid_subset(const POS &pos, Item *a, Item *b)
    : Item_int_func(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "gtid_subset"; }
  void fix_length_and_dec() { max_length= 21; maybe_null= 0; }
  bool is_bool_func() { return true; }
};


/**
  Common class for:
    Item_func_get_system_var
    Item_func_get_user_var
    Item_func_set_user_var
*/
class Item_var_func :public Item_func
{
public:
  Item_var_func() :Item_func() { }
  explicit Item_var_func(const POS &pos) :Item_func(pos) { }

  Item_var_func(THD *thd, Item_var_func *item) :Item_func(thd, item) { }

  Item_var_func(Item *a) :Item_func(a) { }
  Item_var_func(const POS &pos, Item *a) :Item_func(pos, a) { }

  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_non_temporal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_non_temporal(ltime);
  }
  bool check_gcol_func_processor(uchar *int_arg) { return true;}
};


/* Handling of user definable variables */

class user_var_entry;

class Item_func_set_user_var :public Item_var_func
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
  /**
    Delayed setting of non-constness.

    Normally, Item_func_get_user_var objects are tagged as not const
    when Item_func_set_user_var::fix_fields() is called for the same
    variable in the same query. If delayed_non_constness is set, the
    tagging is delayed until the variable is actually set. This means
    that Item_func_get_user_var objects will still be treated as a
    constant by the optimizer and executor until the variable is
    actually changed.

    @see select_dumpvar::send_data().
   */
  bool delayed_non_constness;
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
  Name_string name; // keep it public

  Item_func_set_user_var(Name_string a, Item *b, bool delayed)
    :Item_var_func(b), cached_result_type(INT_RESULT),
     entry(NULL), entry_thread_id(0), delayed_non_constness(delayed), name(a)
  {}
  Item_func_set_user_var(const POS &pos, Name_string a, Item *b, bool delayed)
    :Item_var_func(pos, b), cached_result_type(INT_RESULT),
     entry(NULL), entry_thread_id(0), delayed_non_constness(delayed), name(a)
  {}

  Item_func_set_user_var(THD *thd, Item_func_set_user_var *item)
    :Item_var_func(thd, item), cached_result_type(item->cached_result_type),
     entry(item->entry), entry_thread_id(item->entry_thread_id),
     delayed_non_constness(item->delayed_non_constness), value(item->value),
     decimal_buff(item->decimal_buff), null_item(item->null_item),
     save_result(item->save_result), name(item->name)
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
  bool update_hash(const void *ptr, uint length, enum Item_result type,
  		   const CHARSET_INFO *cs, Derivation dv, bool unsigned_arg);
  bool send(Protocol *protocol, String *str_arg);
  void make_field(Send_field *tmp_field);
  bool check(bool use_result_field);
  void save_item_result(Item *item);
  bool update();
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  void print_assignment(String *str, enum_query_type query_type);
  const char *func_name() const { return "set_user_var"; }

  type_conversion_status save_in_field(Field *field, bool no_conversions,
                                       bool can_use_result_field);

  void save_org_in_field(Field *field)
  { save_in_field(field, true, false); }

  bool set_entry(THD *thd, bool create_if_not_exists);
  void cleanup();
protected:
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions)
  { return save_in_field(field, no_conversions, true); }
};


class Item_func_get_user_var :public Item_var_func,
                              private Settable_routine_parameter
{
  user_var_entry *var_entry;
  Item_result m_cached_result_type;

public:
  Name_string name; // keep it public

  Item_func_get_user_var(Name_string a):
    Item_var_func(), m_cached_result_type(STRING_RESULT), name(a) {}
  Item_func_get_user_var(const POS &pos, Name_string a):
    Item_var_func(pos), m_cached_result_type(STRING_RESULT), name(a) {}

  enum Functype functype() const { return GUSERVAR_FUNC; }
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
  Name_string name;
  user_var_entry *entry;
public:
  Item_user_var_as_out_param(Name_string a) :name(a)
  { item_name.copy(a); }
  /* We should return something different from FIELD_ITEM here */
  enum Type type() const { return STRING_ITEM;}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *decimal_buffer);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    DBUG_ASSERT(0);
    return true;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(0);
    return true;
  }

  /* fix_fields() binds variable name with its entry structure */
  bool fix_fields(THD *thd, Item **ref);
  virtual void print(String *str, enum_query_type query_type);
  void set_null_value(const CHARSET_INFO* cs);
  void set_value(const char *str, size_t length, const CHARSET_INFO* cs);
};


/* A system variable */

#define GET_SYS_VAR_CACHE_LONG     1
#define GET_SYS_VAR_CACHE_DOUBLE   2
#define GET_SYS_VAR_CACHE_STRING   4

class Item_func_get_system_var :public Item_var_func
{
  sys_var *var;
  enum_var_type var_type, orig_var_type;
  LEX_STRING component;
  longlong cached_llval;
  double cached_dval;
  String cached_strval;
  my_bool cached_null_value;
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
};


class JOIN;

class Item_func_match :public Item_real_func
{
  typedef Item_real_func super;

public:
  Item *against;
  uint key, flags;
  bool join_key;
  DTCollation cmp_collation;
  FT_INFO *ft_handler;
  TABLE_LIST *table_ref;
  /**
     Master item means that if idendical items are present in the
     statement, they use the same FT handler. FT handler is initialized
     only for master item and slave items just use it. FT hints initialized
     for master only, slave items HINTS are not accessed.
  */
  Item_func_match *master;
  Item *concat_ws;           // Item_func_concat_ws
  String value;              // value of concat_ws
  String search_value;       // key_item()'s value converted to cmp_collation

  /**
     Constructor for Item_func_match class.

     @param a  List of arguments.
     @param b  FT Flags.
     @param c  Parsing context.
  */
  Item_func_match(const POS &pos, PT_item_list *a, Item *against_arg, uint b):
    Item_real_func(pos, a), against(against_arg), key(0), flags(b),
    join_key(false),
    ft_handler(NULL), table_ref(NULL),
    master(NULL), concat_ws(NULL), hints(NULL), simple_expression(false)
  {}
  
  virtual bool itemize(Parse_context *pc, Item **res);

  void cleanup()
  {
    DBUG_ENTER("Item_func_match::cleanup");
    Item_real_func::cleanup();
    if (!master && ft_handler)
    {
      ft_handler->please->close_search(ft_handler);
      delete hints;
    }
    ft_handler= NULL;
    concat_ws= NULL;
    table_ref= NULL;           // required by Item_func_match::eq()
    master= NULL;
    DBUG_VOID_RETURN;
  }
  virtual Item *key_item() const { return against; }
  enum Functype functype() const { return FT_FUNC; }
  const char *func_name() const { return "match"; }
  void update_used_tables() {}
  table_map not_null_tables() const { return 0; }
  bool fix_fields(THD *thd, Item **ref);
  bool eq(const Item *, bool binary_cmp) const;
  /* The following should be safe, even if we compare doubles */
  longlong val_int() { DBUG_ASSERT(fixed == 1); return val_real() != 0.0; }
  double val_real();
  virtual void print(String *str, enum_query_type query_type);

  bool fix_index();
  bool init_search(THD *thd);
  bool check_gcol_func_processor(uchar *int_arg)
  // TODO: consider adding in support for the MATCH-based generated columns
  { return true; }

  /**
     Get number of matching rows from FT handler.

     @note Requires that FT handler supports the extended API

     @return Number of matching rows in result 
   */
  ulonglong get_count()
  {
    DBUG_ASSERT(ft_handler);
    DBUG_ASSERT(table_ref->table->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT);

    return ((FT_INFO_EXT *)ft_handler)->could_you->
      count_matches((FT_INFO_EXT *)ft_handler);
  }

  /**
     Check whether FT result is ordered on rank

     @return true if result is ordered
     @return false otherwise
   */
  bool ordered_result()
  {
    DBUG_ASSERT(!master);
    if (hints->get_flags() & FT_SORTED)
      return true;

    if ((table_ref->table->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT) == 0)
      return false;

    DBUG_ASSERT(ft_handler);
    return ((FT_INFO_EXT *)ft_handler)->could_you->get_flags() & 
      FTS_ORDERED_RESULT;
  }

  /**
     Check whether FT result contains the document ID

     @return true if document ID is available
     @return false otherwise
   */
  bool docid_in_result()
  {
    DBUG_ASSERT(ft_handler);

    if ((table_ref->table->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT) == 0)
      return false;

    return ((FT_INFO_EXT *)ft_handler)->could_you->get_flags() & 
      FTS_DOCID_IN_RESULT;
  }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);

  /**
     Returns master MATCH function.

     @return pointer to master MATCH function.
  */
  Item_func_match *get_master()
  {
    if (master)
      return master->get_master();
    return this;
  }

  /**
     Set master MATCH function and adjust used_in_where_only value.

     @param item item for which master should be set.
  */
  void set_master(Item_func_match *item)
  {
    used_in_where_only&= item->used_in_where_only;
    item->master= this;
  }

  /**
     Returns pointer to Ft_hints object belonging to master MATCH function.

     @return pointer to Ft_hints object
  */
  Ft_hints *get_hints()
  {
    DBUG_ASSERT(!master);
    return hints;
  }

  /**
     Set comparison operation type and and value for master MATCH function.

     @param type   comparison operation type
     @param value  comparison operation value
  */
  void set_hints_op(enum ft_operation type, double value_arg)
  {
    DBUG_ASSERT(!master);
    hints->set_hint_op(type, value_arg);
  }
  
  /**
     Set FT hints.
  */
  void set_hints(JOIN *join, uint ft_flag, ha_rows ft_limit, bool no_cond);
  
  /**
     Check if ranking is not needed.

     @return true if ranking is not needed
     @return false otherwise
  */
  bool can_skip_ranking()
  {
    DBUG_ASSERT(!master);
    return (!(hints->get_flags() & FT_SORTED) && // FT_SORTED is no set
            used_in_where_only &&                // MATCH result is not used
                                                 // in expression
            hints->get_op_type() == FT_OP_NO);   // MATCH is single function
  }

  /**
     Set flag that the function is a simple expression.

     @param val true if the function is a simple expression, false otherwise
  */
  void set_simple_expression(bool val)
  {
    DBUG_ASSERT(!master);
    simple_expression= val;
  }

  /**
     Check if this MATCH function is a simple expression in WHERE condition.

     @return true if simple expression
     @return false otherwise
  */
  bool is_simple_expression()
  {
    DBUG_ASSERT(!master);
    return simple_expression;
  }

private:
  /**
     Fulltext index hints, initialized for master MATCH function only.
  */
  Ft_hints *hints;
  /**
     Flag is true when MATCH function is used as a simple expression in
     WHERE condition, i.e. there is no AND/OR combinations, just simple
     MATCH function or [MATCH, rank] comparison operation.
  */
  bool simple_expression;
  /**
     true if MATCH function is used in WHERE condition only.
     Used to dermine what hints can be used for FT handler. 
     Note that only master MATCH function has valid value.
     it's ok since only master function is involved in the hint processing.
  */
  bool used_in_where_only;
  /**
     Check whether storage engine for given table, 
     allows FTS Boolean search on non-indexed columns.

     @todo A flag should be added to the extended fulltext API so that 
           it may be checked whether search on non-indexed columns are 
           supported. Currently, it is not possible to check for such a 
           flag since @c this->ft_handler is not yet set when this function is 
           called.  The current hack is to assume that search on non-indexed
           columns are supported for engines that does not support the extended
           fulltext API (e.g., MyISAM), while it is not supported for other 
           engines (e.g., InnoDB)

     @param tr Table for which storage engine to check

     @retval true if BOOLEAN search on non-indexed columns is supported
     @retval false otherwise
   */
  bool allows_search_on_non_indexed_columns(const TABLE *tr)
  {
    // Only Boolean search may support non_indexed columns
    if (!(flags & FT_BOOL))
      return false;

    DBUG_ASSERT(tr && tr->file);

    // Assume that if extended fulltext API is not supported,
    // non-indexed columns are allowed.  This will be true for MyISAM.
    if ((tr->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT) == 0)
      return true;

    return false;
  }
};


class Item_func_bit_xor : public Item_func_bit
{
  Item *check_deprecated_second_arg() const { return args[1]; }
public:
  Item_func_bit_xor(const POS &pos, Item *a, Item *b) :Item_func_bit(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "^"; }
};

class Item_func_is_free_lock :public Item_int_func
{
  typedef Item_int_func super;

  String value;
public:
  Item_func_is_free_lock(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "is_free_lock"; }
  void fix_length_and_dec() { max_length= 1; maybe_null= TRUE;}
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

class Item_func_is_used_lock :public Item_int_func
{
  typedef Item_int_func super;

  String value;
public:
  Item_func_is_used_lock(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "is_used_lock"; }
  void fix_length_and_dec() { unsigned_flag= TRUE; maybe_null= TRUE; }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL, ITEM_CAST_JSON
};


class Item_func_row_count :public Item_int_func
{
  typedef Item_int_func super;

public:
  explicit Item_func_row_count(const POS &pos) :Item_int_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);

  longlong val_int();
  const char *func_name() const { return "row_count"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
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
  typedef Item_func super;
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
  bool is_expensive_processor(uchar *arg) { return true; }
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions);

public:

  Item_func_sp(const POS &pos,
               const LEX_STRING &db_name, const LEX_STRING &fn_name,
               bool use_explicit_name, PT_item_list *opt_list);

  virtual bool itemize(Parse_context *pc, Item **res);
  /**
    Must not be called before the procedure is resolved,
    i.e. ::init_result_field().
  */
  table_map get_initial_pseudo_tables() const;
  void update_used_tables();

  virtual void fix_after_pullout(st_select_lex *parent_select,
                                 st_select_lex *removed_select);

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

  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    if (execute())
      return true;
    return sp_result_field->get_date(ltime, fuzzydate);
  }

  bool get_time(MYSQL_TIME *ltime)
  {
    if (execute())
      return true;
    return sp_result_field->get_time(ltime);
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

  bool val_json(Json_wrapper *result);

  virtual bool change_context_processor(uchar *cntx)
  {
    context= reinterpret_cast<Name_resolution_context *>(cntx);
    return false;
  }

  bool sp_check_access(THD * thd);
  virtual enum Functype functype() const { return FUNC_SP; }

  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec(void);
  bool is_expensive() { return true; }

  inline Field *get_sp_result_field()
  {
    return sp_result_field;
  }

  virtual void update_null_value();

  /**
    Ensure that deterministic functions are not evaluated in preparation phase
    by returning false before tables are locked and true after they are locked.
    (can_be_evaluated_now() handles this because a function has the
    has_subquery() property).

     @retval true if tables are locked for deterministic functions
     @retval false Otherwise
  */
  bool const_item() const
  {
    if (used_tables() == 0)
      return can_be_evaluated_now();
    return false;
  }
};


class Item_func_found_rows :public Item_int_func
{
  typedef Item_int_func super;
public:
  explicit Item_func_found_rows(const POS &pos) :Item_int_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  const char *func_name() const { return "found_rows"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


void uuid_short_init();

class Item_func_uuid_short :public Item_int_func
{
  typedef Item_int_func super;
public:
  Item_func_uuid_short(const POS &pos) :Item_int_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  const char *func_name() const { return "uuid_short"; }
  longlong val_int();
  void fix_length_and_dec()
  { max_length= 21; unsigned_flag=1; }
  bool check_partition_func_processor(uchar *int_arg) {return false;}
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_version : public Item_static_string_func
{
  typedef Item_static_string_func super;
public:
  explicit Item_func_version(const POS &pos)
    : Item_static_string_func(pos, NAME_STRING("version()"),
                              server_version,
                              strlen(server_version),
                              system_charset_info,
                              DERIVATION_SYSCONST)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
};


Item *get_system_var(Parse_context *pc, enum_var_type var_type, LEX_STRING name,
                     LEX_STRING component);
extern bool check_reserved_words(LEX_STRING *name);
extern enum_field_types agg_field_type(Item **items, uint nitems);
double my_double_round(double value, longlong dec, bool dec_unsigned,
                       bool truncate);
bool eval_const_cond(THD *thd, Item *cond, bool *value);
Item_field *get_gc_for_expr(Item_func **func, Field *fld, Item_result type);

extern bool volatile  mqh_used;

#endif /* ITEM_FUNC_INCLUDED */

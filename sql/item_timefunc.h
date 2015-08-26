#ifndef ITEM_TIMEFUNC_INCLUDED
#define ITEM_TIMEFUNC_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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


/* Function items used by mysql */

#include "item_strfunc.h"  // Item_str_func

#include <algorithm>

class MY_LOCALE;
struct Interval;
struct Date_time_format;

bool get_interval_value(Item *args,interval_type int_type,
			       String *str_value, Interval *interval);

class Item_func_period_add :public Item_int_func
{
public:
  Item_func_period_add(const POS &pos, Item *a, Item *b)
    :Item_int_func(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "period_add"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(6); /* YYYYMM */
  }
};


class Item_func_period_diff :public Item_int_func
{
public:
  Item_func_period_diff(const POS &pos, Item *a,Item *b)
    :Item_int_func(pos, a,b)
  {}
  longlong val_int();
  const char *func_name() const { return "period_diff"; }
  void fix_length_and_dec()
  { 
    fix_char_length(6); /* YYYYMM */
  }
};


class Item_func_to_days :public Item_int_func
{
public:
  Item_func_to_days(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "to_days"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(6);
    maybe_null=1; 
  }
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_to_seconds :public Item_int_func
{
public:
  Item_func_to_seconds(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "to_seconds"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(6);
    maybe_null=1; 
  }
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  bool check_partition_func_processor(uchar *bool_arg) { return false; }

  bool intro_version(uchar *int_arg)
  {
    int *input_version= (int*)int_arg;
    /* This function was introduced in 5.5 */
    int output_version= std::max(*input_version, 50500);
    *input_version= output_version;
    return 0;
  }

  /* Only meaningful with date part and optional time part */
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_dayofmonth :public Item_int_func
{
public:
  Item_func_dayofmonth(Item *a) :Item_int_func(a) {}
  Item_func_dayofmonth(const POS &pos, Item *a) :Item_int_func(pos, a) {}

  longlong val_int();
  const char *func_name() const { return "dayofmonth"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(2); /* 1..31 */
    maybe_null=1; 
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


/**
  TS-TODO: This should probably have Item_int_func as parent class.
*/
class Item_func_month :public Item_func
{
public:
  Item_func_month(const POS &pos, Item *a) :Item_func(pos, a)
  { collation.set_numeric(); }
  longlong val_int();
  double val_real()
  { DBUG_ASSERT(fixed == 1); return (double) Item_func_month::val_int(); }
  String *val_str(String *str) 
  {
    longlong nr= val_int();
    if (null_value)
      return 0;
    str->set(nr, collation.collation);
    return str;
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  const char *func_name() const { return "month"; }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() 
  { 
    fix_char_length(2);
    maybe_null= 1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_monthname :public Item_str_func
{
  MY_LOCALE *locale;
public:
  Item_func_monthname(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  const char *func_name() const { return "monthname"; }
  String *val_str(String *str);
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *arg) { return true; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_dayofyear :public Item_int_func
{
public:
  Item_func_dayofyear(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "dayofyear"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(3);
    maybe_null= 1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_hour :public Item_int_func
{
public:
  Item_func_hour(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "hour"; }
  void fix_length_and_dec()
  {
    fix_char_length(2); /* 0..23 */
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_time_args();
  }
};


class Item_func_minute :public Item_int_func
{
public:
  Item_func_minute(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "minute"; }
  void fix_length_and_dec()
  {
    fix_char_length(2); /* 0..59 */
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_time_args();
  }
};


class Item_func_quarter :public Item_int_func
{
public:
  Item_func_quarter(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "quarter"; }
  void fix_length_and_dec()
  { 
     fix_char_length(1); /* 1..4 */
     maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_second :public Item_int_func
{
public:
  Item_func_second(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "second"; }
  void fix_length_and_dec() 
  { 
    fix_char_length(2); /* 0..59 */
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_time_args();
  }
};


class Item_func_week :public Item_int_func
{
  typedef Item_int_func super;

public:
  Item_func_week(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_func_week(const POS &pos, Item *a,Item *b) :super(pos, a, b) {}

  virtual bool itemize(Parse_context *pc, Item **res);

  longlong val_int();
  const char *func_name() const { return "week"; }
  void fix_length_and_dec()
  { 
    fix_char_length(2); /* 0..54 */
    maybe_null=1;
  }
};

class Item_func_yearweek :public Item_int_func
{
public:
  Item_func_yearweek(const POS &pos, Item *a, Item *b) :Item_int_func(pos, a, b)
  {}
  longlong val_int();
  const char *func_name() const { return "yearweek"; }
  void fix_length_and_dec()
  { 
    fix_char_length(6); /* YYYYWW */
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


class Item_func_year :public Item_int_func
{
public:
  Item_func_year(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "year"; }
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  void fix_length_and_dec()
  { 
    fix_char_length(4); /* 9999 */
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};


/**
  TS-TODO: This should probably have Item_int_func as parent class.
*/
class Item_func_weekday :public Item_func
{
  bool odbc_type;
public:
  Item_func_weekday(const POS &pos, Item *a,bool type_arg)
    :Item_func(pos, a), odbc_type(type_arg)
  { collation.set_numeric(); }
  longlong val_int();
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    str->set(val_int(), &my_charset_bin);
    return null_value ? 0 : str;
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  const char *func_name() const
  {
     return (odbc_type ? "dayofweek" : "weekday");
  }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec()
  {
    fix_char_length(1);
    maybe_null= 1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_date_args();
  }
};

/**
  TS-TODO: Item_func_dayname should be derived from Item_str_func.
  In the current implementation funny things can happen:
  select dayname(now())+1 -> 4
*/
class Item_func_dayname :public Item_func_weekday
{
  MY_LOCALE *locale;
 public:
  Item_func_dayname(const POS &pos, Item *a) :Item_func_weekday(pos, a, 0) {}
  const char *func_name() const { return "dayname"; }
  String *val_str(String *str);
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
  bool check_partition_func_processor(uchar *int_arg) { return true; }
};


/*
  Abstract class for functions returning "struct timeval".
*/
class Item_timeval_func :public Item_func
{
public:
  explicit
  Item_timeval_func(const POS &pos) :Item_func(pos) { }

  Item_timeval_func(Item *a) :Item_func(a) { }
  Item_timeval_func(const POS &pos, Item *a) :Item_func(pos, a) { }
  /**
    Return timestamp in "struct timeval" format.
    @param[out] tm The value is store here.
    @retval false On success
    @retval true  On error
  */
  virtual bool val_timeval(struct timeval *tm)= 0;
  longlong val_int();
  double val_real();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *decimal_value);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_numeric(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_numeric(ltime);
  }
  enum Item_result result_type() const
  {
    return decimals ? DECIMAL_RESULT : INT_RESULT;
  }
};


class Item_func_unix_timestamp :public Item_timeval_func
{
  typedef Item_timeval_func super;
public:
  explicit
  Item_func_unix_timestamp(const POS &pos) :Item_timeval_func(pos) {}

  Item_func_unix_timestamp(Item *a) :Item_timeval_func(a) {}
  Item_func_unix_timestamp(const POS &pos, Item *a) :Item_timeval_func(pos, a)
  {}

  const char *func_name() const { return "unix_timestamp"; }

  virtual bool itemize(Parse_context *pc, Item **res);
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  bool check_partition_func_processor(uchar *int_arg) { return false; }
  /*
    UNIX_TIMESTAMP() depends on the current timezone
    (and thus may not be used as a partitioning function)
    when its argument is NOT of the TIMESTAMP type.
  */
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_timestamp_args();
  }
  void fix_length_and_dec()
  {
    fix_length_and_dec_and_charset_datetime(11, arg_count ==  0 ?  0 :
                                                args[0]->datetime_precision());
  }
  bool val_timeval(struct timeval *tm);
  bool check_gcol_func_processor(uchar *int_arg)
    /*
      TODO: Allow UNIX_TIMESTAMP called with an argument to be a part
      of the expression for a generated column
    */
  { return true; }

};


class Item_func_time_to_sec :public Item_int_func
{
public:
  Item_func_time_to_sec(const POS &pos, Item *item) :Item_int_func(pos, item) {}
  longlong val_int();
  const char *func_name() const { return "time_to_sec"; }
  void fix_length_and_dec()
  {
    maybe_null= TRUE;
    fix_char_length(10);
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_time_args();
  }
};


/**
  Abstract class for functions returning TIME, DATE, DATETIME types
  whose data type is known at constructor time.
*/
class Item_temporal_func :public Item_func
{
protected:
  bool check_precision();
public:
  Item_temporal_func() :Item_func() {}
  explicit Item_temporal_func(const POS &pos) :Item_func(pos) {}

  Item_temporal_func(Item *a) :Item_func(a) {}
  Item_temporal_func(const POS &pos, Item *a) :Item_func(pos, a) {}

  Item_temporal_func(const POS &pos, Item *a, Item *b) :Item_func(pos, a, b) {}

  Item_temporal_func(Item *a, Item *b, Item *c) :Item_func(a, b, c) {}
  Item_temporal_func(const POS &pos, Item *a, Item *b, Item *c)
    :Item_func(pos, a, b, c)
  {}

  enum Item_result result_type () const
  {
    return STRING_RESULT;
  }
  CHARSET_INFO *charset_for_protocol() const
  {
    return &my_charset_bin;
  }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  uint time_precision()
  {
    DBUG_ASSERT(fixed);
    return decimals;
  }
  uint datetime_precision()
  {
    DBUG_ASSERT(fixed);
    return decimals;
  }
  virtual void print(String *str, enum_query_type query_type);
};


/**
  Abstract class for functions returning TIME, DATE, DATETIME or string values,
  whose data type depends on parameters and is set at fix_field time.
*/
class Item_temporal_hybrid_func :public Item_str_func
{
protected:
  sql_mode_t sql_mode; // sql_mode value is cached here in fix_length_and_dec()
  enum_field_types cached_field_type; // TIME, DATE, DATETIME or STRING
  String ascii_buf; // Conversion buffer
  /**
    Get "native" temporal value as MYSQL_TIME
    @param[out] ltime       The value is stored here.
    @param[in]  fuzzy_date  Date flags.
    @retval     false       On success.
    @retval     true        On error.
  */
  virtual bool val_datetime(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date)= 0;
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions);

public:
  Item_temporal_hybrid_func(Item *a, Item *b) :Item_str_func(a, b),
    sql_mode(0)
  { }
  Item_temporal_hybrid_func(const POS &pos, Item *a, Item *b)
    :Item_str_func(pos, a, b), sql_mode(0)
  { }

  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return cached_field_type; }
  const CHARSET_INFO *charset_for_protocol() const
  {
    /*
      Can return TIME, DATE, DATETIME or VARCHAR depending on arguments.
      Send using "binary" when TIME, DATE or DATETIME,
      or using collation.collation when VARCHAR
      (which is fixed from @collation_connection in fix_length_and_dec).
    */
    DBUG_ASSERT(fixed == 1);
    return cached_field_type == MYSQL_TYPE_STRING ?
                                collation.collation : &my_charset_bin;
  }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  longlong val_int() { return val_int_from_decimal(); }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value);
  /**
    Return string value in ASCII character set.
  */
  String *val_str_ascii(String *str);
  /**
    Return string value in @@character_set_connection.
  */
  String *val_str(String *str)
  {
    return val_str_from_val_str_ascii(str, &ascii_buf);
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
};


/*
  This can't be a Item_str_func, because the val_real() functions are special
*/

/**
  Abstract class for functions returning DATE values.
*/
class Item_date_func :public Item_temporal_func
{
protected:
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
public:
  Item_date_func() :Item_temporal_func()
  { }
  explicit Item_date_func(const POS &pos) :Item_temporal_func(pos)
  { }

  Item_date_func(Item *a) :Item_temporal_func(a)
  { }
  Item_date_func(const POS &pos, Item *a) :Item_temporal_func(pos, a)
  { }

  Item_date_func(const POS &pos, Item *a, Item *b)
    :Item_temporal_func(pos, a, b)
  { }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_date(ltime);
  }
  String *val_str(String *str)
  {
    return val_string_from_date(str);
  }
  longlong val_int()
  {  
    return val_int_from_date();
  }
  longlong val_date_temporal();
  double val_real() { return (double) val_int(); }
  const char *func_name() const { return "date"; }
  void fix_length_and_dec()
  { 
    fix_length_and_dec_and_charset_datetime(MAX_DATE_WIDTH, 0);
  }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  // All date functions must implement get_date()
  // to avoid use of generic Item::get_date()
  // which converts to string and then parses the string as DATE.
  virtual bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date)= 0;
};


/**
  Abstract class for functions returning DATETIME values.
*/
class Item_datetime_func :public Item_temporal_func
{
protected:
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
public:
  Item_datetime_func() :Item_temporal_func()
  { }
  Item_datetime_func(const POS &pos) :Item_temporal_func(pos)
  { }

  Item_datetime_func(Item *a) :Item_temporal_func(a)
  { }
  Item_datetime_func(const POS &pos, Item *a) :Item_temporal_func(pos, a)
  { }

  Item_datetime_func(const POS &pos, Item *a, Item *b)
    :Item_temporal_func(pos, a, b)
  { }

  Item_datetime_func(Item *a,Item *b, Item *c) :Item_temporal_func(a,b,c)
  { }
  Item_datetime_func(const POS &pos, Item *a,Item *b, Item *c)
    :Item_temporal_func(pos, a, b, c)
  {}

  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
  double val_real() { return val_real_from_decimal(); }
  String *val_str(String *str)
  {
    return val_string_from_datetime(str);
  }
  longlong val_int()
  {
    return val_int_from_datetime();
  }
  longlong val_date_temporal();
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_datetime(ltime);
  }
  // All datetime functions must implement get_date()
  // to avoid use of generic Item::get_date()
  // which converts to string and then parses the string as DATETIME.
  virtual bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date)= 0;
};


/**
  Abstract class for functions returning TIME values.
*/
class Item_time_func :public Item_temporal_func
{
protected:
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions)
  {
    return save_time_in_field(field);
  }
public:
  Item_time_func() :Item_temporal_func() {}
  explicit Item_time_func(const POS &pos) :Item_temporal_func(pos) {}

  Item_time_func(Item *a) :Item_temporal_func(a) {}
  Item_time_func(const POS &pos, Item *a) :Item_temporal_func(pos, a) {}

  Item_time_func(const POS &pos, Item *a, Item *b)
    :Item_temporal_func(pos, a, b)
  {}
  Item_time_func(const POS &pos, Item *a, Item *b, Item *c)
    :Item_temporal_func(pos, a, b ,c)
  {}
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_time(decimal_value);
  }
  longlong val_int()
  {
    return val_int_from_time();
  }
  longlong val_time_temporal();
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date)
  {
    return get_date_from_time(res);
  }
  String *val_str(String *str)
  {
    return val_string_from_time(str);
  }
  // All time functions must implement get_time()
  // to avoid use of generic Item::get_time()
  // which converts to string and then parses the string as TIME.
  virtual bool get_time(MYSQL_TIME *res)= 0;
};


/**
  Cache for MYSQL_TIME value with various representations.
  
  - MYSQL_TIME representation (time) is initialized during set_XXX().
  - Packed representation (time_packed) is also initialized during set_XXX().
  - String representation (string_buff) is not initialized during set_XXX();
    it's initialized only if val_str() or cptr() are called.
*/
class MYSQL_TIME_cache
{
  MYSQL_TIME time;                              ///< MYSQL_TIME representation
  longlong time_packed;                         ///< packed representation
  char string_buff[MAX_DATE_STRING_REP_LENGTH]; ///< string representation
  uint string_length;                           ///< length of string
  uint8 dec;                                    ///< Number of decimals
  /**
    Cache string representation from the cached MYSQL_TIME representation.
    If string representation has already been cached, then nothing happens.
  */
  void cache_string();
  /**
    Reset string representation.
  */
  void reset_string()
  {
    string_length= 0;
    string_buff[0]= '\0';
  }
  /**
    Reset all members.
  */
  void reset()
  {
    time.time_type= MYSQL_TIMESTAMP_NONE;
    time_packed= 0;
    reset_string();
    dec= 0;
  }
  /**
    Store MYSQL_TIME representation into the given MYSQL_TIME variable.
  */
  void get_TIME(MYSQL_TIME *ltime) const
  {
    DBUG_ASSERT(time.time_type != MYSQL_TIMESTAMP_NONE);
    *ltime= time;
  }
public:

  MYSQL_TIME_cache()
  {
    reset();
  }
  /**
    Set time and time_packed from a DATE value.
  */
  void set_date(MYSQL_TIME *ltime);
  /**
    Set time and time_packed from a TIME value.
  */
  void set_time(MYSQL_TIME *ltime, uint8 dec_arg);
  /**
    Set time and time_packed from a DATETIME value.
  */
  void set_datetime(MYSQL_TIME *ltime, uint8 dec_arg);
  /**
    Set time and time_packed according to DATE value
    in "struct timeval" representation and its time zone.
  */
  void set_date(struct timeval tv, Time_zone *tz);
  /**
    Set time and time_packed according to TIME value
    in "struct timeval" representation and its time zone.
  */
  void set_time(struct timeval tv, uint8 dec_arg, Time_zone *tz);
  /**
    Set time and time_packed according to DATETIME value
    in "struct timeval" representation and its time zone.
  */
  void set_datetime(struct timeval tv, uint8 dec_arg, Time_zone *tz);
  /**
    Test if cached value is equal to another MYSQL_TIME_cache value.
  */
  bool eq(const MYSQL_TIME_cache &tm) const
  {
    return val_packed() == tm.val_packed();
  }

  /**
    Return number of decimal digits.
  */
  uint8 decimals() const
  {
    DBUG_ASSERT(time.time_type != MYSQL_TIMESTAMP_NONE);
    return dec;
  }

  /**
    Return packed representation.
  */
  longlong val_packed() const
  {
    DBUG_ASSERT(time.time_type != MYSQL_TIMESTAMP_NONE);
    return time_packed;
  }
  /**
    Store MYSQL_TIME representation into the given date/datetime variable
    checking date flags.
  */
  bool get_date(MYSQL_TIME *ltime, uint fuzzyflags) const;
  /**
    Store MYSQL_TIME representation into the given time variable.
  */
  bool get_time(MYSQL_TIME *ltime) const
  {
    get_TIME(ltime);
    return false;
  }
  /**
    Return pointer to MYSQL_TIME representation.
  */
  MYSQL_TIME *get_TIME_ptr()
  {
    DBUG_ASSERT(time.time_type != MYSQL_TIMESTAMP_NONE);
    return &time;
  }
  /**
    Store string representation into String.
  */
  String *val_str(String *str);
  /**
    Return C string representation.
  */
  const char *cptr();
};


/**
  DATE'2010-01-01'
*/
class Item_date_literal :public Item_date_func
{
  MYSQL_TIME_cache cached_time;
public:
  /**
    Constructor for Item_date_literal.
    @param ltime  DATE value.
  */
  Item_date_literal(MYSQL_TIME *ltime)
  {
    cached_time.set_date(ltime);
    fix_length_and_dec();
    fixed= 1;
  }
  const char *func_name() const { return "date_literal"; }
  void print(String *str, enum_query_type query_type);
  longlong val_date_temporal()
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_packed();
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date)
  {
    DBUG_ASSERT(fixed);
    return cached_time.get_date(ltime, fuzzy_date);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_str(str);
  }
  void fix_length_and_dec()
  {
    fix_length_and_dec_and_charset_datetime(MAX_DATE_WIDTH, 0);
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool basic_const_item() const { return true; }
  bool const_item() const { return true; }
  table_map used_tables() const { return (table_map) 0L; }
  table_map not_null_tables() const { return used_tables(); }
  void cleanup()
  {
    // See Item_basic_const::cleanup()
    if (orig_name.is_set())
      item_name= orig_name;
  }
  bool eq(const Item *item, bool binary_cmp) const;
};


/**
  TIME'10:10:10'
*/
class Item_time_literal :public Item_time_func
{
  MYSQL_TIME_cache cached_time;
public:
  /**
    Constructor for Item_time_literal.
    @param ltime    TIME value.
    @param dec_arg  number of fractional digits in ltime.
  */
  Item_time_literal(MYSQL_TIME *ltime, uint dec_arg)
  {
    decimals= MY_MIN(dec_arg, DATETIME_MAX_DECIMALS);
    cached_time.set_time(ltime, decimals);
    fix_length_and_dec();
    fixed= 1;
  }
  const char *func_name() const { return "time_literal"; }
  void print(String *str, enum_query_type query_type);
  longlong val_time_temporal()
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_packed();
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(fixed);
    return cached_time.get_time(ltime);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_str(str);
  }
  void fix_length_and_dec()
  {
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH, decimals);
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool basic_const_item() const { return true; }
  bool const_item() const { return true; }
  table_map used_tables() const { return (table_map) 0L; }
  table_map not_null_tables() const { return used_tables(); }
  void cleanup()
  {
    // See Item_basic_const::cleanup()
    if (orig_name.is_set())
      item_name= orig_name;
  }
  bool eq(const Item *item, bool binary_cmp) const;
};


/**
  TIMESTAMP'2001-01-01 10:20:30'
*/
class Item_datetime_literal :public Item_datetime_func
{
  MYSQL_TIME_cache cached_time;
public:
  /**
    Constructor for Item_datetime_literal.
    @param ltime    DATETIME value.
    @param dec_arg  number of fractional digits in ltime.
  */
  Item_datetime_literal(MYSQL_TIME *ltime, uint dec_arg)
  {
    decimals= MY_MIN(dec_arg, DATETIME_MAX_DECIMALS);
    cached_time.set_datetime(ltime, decimals);
    fix_length_and_dec();
    fixed= 1;
  }
  const char *func_name() const { return "datetime_literal"; }
  void print(String *str, enum_query_type query_type);
  longlong val_date_temporal()
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_packed();
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date)
  {
    DBUG_ASSERT(fixed);
    return cached_time.get_date(ltime, fuzzy_date);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed);
    return cached_time.val_str(str);
  }
  void fix_length_and_dec()
  {
    fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, decimals);
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool basic_const_item() const { return true; }
  bool const_item() const { return true; }
  table_map used_tables() const { return (table_map) 0L; }
  table_map not_null_tables() const { return used_tables(); }
  void cleanup()
  {
    // See Item_basic_const::cleanup()
    if (orig_name.is_set())
      item_name= orig_name;
  }
  bool eq(const Item *item, bool binary_cmp) const;
};


/* Abstract CURTIME function. Children should define what time zone is used */

class Item_func_curtime :public Item_time_func
{
  typedef Item_time_func super;

  MYSQL_TIME_cache cached_time; // Initialized in fix_length_and_dec
protected:
  // Abstract method that defines which time zone is used for conversion.
  virtual Time_zone *time_zone()= 0;
public:
  /**
    Constructor for Item_func_curtime.
    @param dec_arg  Number of fractional digits.
  */
  Item_func_curtime(const POS &pos, uint8 dec_arg) :Item_time_func(pos)
  { decimals= dec_arg; }

  virtual bool itemize(Parse_context *pc, Item **res);

  void fix_length_and_dec();
  longlong val_time_temporal()
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_packed();
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.get_time(ltime);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_str(&str_value);
  }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_curtime_local :public Item_func_curtime
{
protected:
  Time_zone *time_zone();
public:
  Item_func_curtime_local(const POS &pos, uint8 dec_arg)
    :Item_func_curtime(pos, dec_arg)
  {}
  const char *func_name() const { return "curtime"; }
};


class Item_func_curtime_utc :public Item_func_curtime
{
protected:
  Time_zone *time_zone();
public:
  Item_func_curtime_utc(const POS &pos, uint8 dec_arg)
    :Item_func_curtime(pos, dec_arg)
  {}
  const char *func_name() const { return "utc_time"; }
};


/* Abstract CURDATE function. See also Item_func_curtime. */

class Item_func_curdate :public Item_date_func
{
  typedef Item_date_func super;

  MYSQL_TIME_cache cached_time; // Initialized in fix_length_and_dec
protected:
  virtual Time_zone *time_zone()= 0;
public:
  explicit Item_func_curdate(const POS &pos) :Item_date_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);

  void fix_length_and_dec();
  longlong val_date_temporal()
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_packed();
  }
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.get_time(res);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_str(&str_value);
  }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_curdate_local :public Item_func_curdate
{
protected:
  Time_zone *time_zone();
public:
  explicit Item_func_curdate_local(const POS &pos) :Item_func_curdate(pos) {}
  const char *func_name() const { return "curdate"; }
};


class Item_func_curdate_utc :public Item_func_curdate
{
protected:
  Time_zone *time_zone();
public:
  explicit Item_func_curdate_utc(const POS &pos) :Item_func_curdate(pos) {}
  const char *func_name() const { return "utc_date"; }
};


/* Abstract CURRENT_TIMESTAMP function. See also Item_func_curtime */

class Item_func_now :public Item_datetime_func
{
  MYSQL_TIME_cache cached_time; 
protected:
  virtual Time_zone *time_zone()= 0;
  type_conversion_status save_in_field_inner(Field *to, bool no_conversions);
public:
  /**
    Constructor for Item_func_now.
    @param dec_arg  Number of fractional digits.
  */
  Item_func_now(uint8 dec_arg) :Item_datetime_func() { decimals= dec_arg; }
  Item_func_now(const POS &pos, uint8 dec_arg)
    :Item_datetime_func(pos)
  { decimals= dec_arg; }

  void fix_length_and_dec();
  longlong val_date_temporal()
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_packed();
  }
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.get_time(res);
  }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    return cached_time.val_str(&str_value);
  }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_now_local :public Item_func_now
{
protected:
  Time_zone *time_zone();
public:
  /**
     Stores the query start time in a field, truncating to the field's number
     of fractional second digits.
     
     @param field The field to store in.
   */
  static void store_in(Field *field);

  Item_func_now_local(uint8 dec_arg) :Item_func_now(dec_arg) {}
  Item_func_now_local(const POS &pos, uint8 dec_arg)
    :Item_func_now(pos, dec_arg)
  {}

  const char *func_name() const { return "now"; }
  virtual enum Functype functype() const { return NOW_FUNC; }
};


class Item_func_now_utc :public Item_func_now
{
  typedef Item_func_now super;

protected:
  Time_zone *time_zone();
public:
  Item_func_now_utc(const POS &pos, uint8 dec_arg)
    :Item_func_now(pos, dec_arg)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);

  const char *func_name() const { return "utc_timestamp"; }
};


/*
  This is like NOW(), but always uses the real current time, not the
  query_start(). This matches the Oracle behavior.
*/
class Item_func_sysdate_local :public Item_datetime_func
{
public:
  Item_func_sysdate_local(uint8 dec_arg) :
    Item_datetime_func() { decimals= dec_arg; }
  bool const_item() const { return 0; }
  const char *func_name() const { return "sysdate"; }
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
  /**
    This function is non-deterministic and hence depends on the 'RAND' pseudo-table.

    @retval Always RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const { return RAND_TABLE_BIT; }
};


class Item_func_from_days :public Item_date_func
{
public:
  Item_func_from_days(const POS &pos, Item *a) :Item_date_func(pos, a) {}
  const char *func_name() const { return "from_days"; }
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return has_date_args() || has_time_args();
  }
};


class Item_func_date_format :public Item_str_func
{
  int fixed_length;
  const bool is_time_format;
  String value;
public:
  Item_func_date_format(const POS &pos,
                        Item *a, Item *b, bool is_time_format_arg)
    :Item_str_func(pos, a, b), is_time_format(is_time_format_arg)
  {}
  String *val_str(String *str);
  const char *func_name() const
    { return is_time_format ? "time_format" : "date_format"; }
  void fix_length_and_dec();
  uint format_length(const String *format);
  bool eq(const Item *item, bool binary_cmp) const;
};


class Item_func_from_unixtime :public Item_datetime_func
{
  THD *thd;
 public:
  Item_func_from_unixtime(const POS &pos, Item *a) :Item_datetime_func(pos, a)
  {}
  const char *func_name() const { return "from_unixtime"; }
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
};


/* 
  We need Time_zone class declaration for storing pointers in
  Item_func_convert_tz.
*/
class Time_zone;

/*
  This class represents CONVERT_TZ() function.
  The important fact about this function that it is handled in special way.
  When such function is met in expression time_zone system tables are added
  to global list of tables to open, so later those already opened and locked
  tables can be used during this function calculation for loading time zone
  descriptions.
*/
class Item_func_convert_tz :public Item_datetime_func
{
  /*
    If time zone parameters are constants we are caching objects that
    represent them (we use separate from_tz_cached/to_tz_cached members
    to indicate this fact, since NULL is legal value for from_tz/to_tz
    members.
  */
  bool from_tz_cached, to_tz_cached;
  Time_zone *from_tz, *to_tz;
 public:
  Item_func_convert_tz(const POS &pos, Item *a, Item *b, Item *c):
    Item_datetime_func(pos, a, b, c), from_tz_cached(0), to_tz_cached(0) {}
  const char *func_name() const { return "convert_tz"; }
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
  void cleanup();
};


class Item_func_sec_to_time :public Item_time_func
{
public:
  Item_func_sec_to_time(const POS &pos, Item *item) :Item_time_func(pos, item)
  {}
  void fix_length_and_dec()
  { 
    maybe_null=1;
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH,
                                            MY_MIN(args[0]->decimals,
                                                   DATETIME_MAX_DECIMALS));
  }
  const char *func_name() const { return "sec_to_time"; }
  bool get_time(MYSQL_TIME *ltime);
};


class Item_date_add_interval :public Item_temporal_hybrid_func
{
  String value;
  bool get_date_internal(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
  bool get_time_internal(MYSQL_TIME *res);
protected:
  bool val_datetime(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date);

public:
  const interval_type int_type; // keep it public
  const bool date_sub_interval; // keep it public
  Item_date_add_interval(const POS &pos,
                         Item *a, Item *b, interval_type type_arg, bool neg_arg)
    :Item_temporal_hybrid_func(pos, a, b),
     int_type(type_arg), date_sub_interval(neg_arg) {}
  const char *func_name() const { return "date_add_interval"; }
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  void print(String *str, enum_query_type query_type);
};


class Item_extract :public Item_int_func
{
  bool date_value;
 public:
  const interval_type int_type; // keep it public
  Item_extract(const POS &pos, interval_type type_arg, Item *a)
    :Item_int_func(pos, a), int_type(type_arg)
  {}
  longlong val_int();
  enum Functype functype() const { return EXTRACT_FUNC; }
  const char *func_name() const { return "extract"; }
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  virtual void print(String *str, enum_query_type query_type);
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    switch (int_type) {
    case INTERVAL_YEAR:
    case INTERVAL_YEAR_MONTH:
    case INTERVAL_QUARTER:
    case INTERVAL_MONTH:
    /* case INTERVAL_WEEK: Not allowed as partitioning function, bug#57071 */
    case INTERVAL_DAY:
      return !has_date_args();
    case INTERVAL_DAY_HOUR:
    case INTERVAL_DAY_MINUTE:
    case INTERVAL_DAY_SECOND:
    case INTERVAL_DAY_MICROSECOND:
      return !has_datetime_args();
    case INTERVAL_HOUR:
    case INTERVAL_HOUR_MINUTE:
    case INTERVAL_HOUR_SECOND:
    case INTERVAL_MINUTE:
    case INTERVAL_MINUTE_SECOND:
    case INTERVAL_SECOND:
    case INTERVAL_MICROSECOND:
    case INTERVAL_HOUR_MICROSECOND:
    case INTERVAL_MINUTE_MICROSECOND:
    case INTERVAL_SECOND_MICROSECOND:
      return !has_time_args();
    default:
      /*
        INTERVAL_LAST is only an end marker,
        INTERVAL_WEEK depends on default_week_format which is a session
        variable and cannot be used for partitioning. See bug#57071.
      */
      break;
    }
    return true;
  }
};


class Item_date_typecast :public Item_date_func
{
public:
  Item_date_typecast(Item *a) :Item_date_func(a) { maybe_null= 1; }
  Item_date_typecast(const POS &pos, Item *a) :Item_date_func(pos, a)
  { maybe_null= 1; }

  void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "cast_as_date"; }
  enum Functype functype() const { return TYPECAST_FUNC; }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date);
  const char *cast_type() const { return "date"; }
};


class Item_time_typecast :public Item_time_func
{
  bool detect_precision_from_arg;
public:
  Item_time_typecast(Item *a): Item_time_func(a)
  {
    detect_precision_from_arg= true;
  }
  Item_time_typecast(const POS &pos, Item *a): Item_time_func(pos, a)
  {
    detect_precision_from_arg= true;
  }

  Item_time_typecast(const POS &pos, Item *a, uint8 dec_arg)
    : Item_time_func(pos, a)
  {
    detect_precision_from_arg= false;
    decimals= dec_arg;
  }
  void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "cast_as_time"; }
  enum Functype functype() const { return TYPECAST_FUNC; }
  bool get_time(MYSQL_TIME *ltime);
  const char *cast_type() const { return "time"; }
  void fix_length_and_dec()
  {
    maybe_null= 1;
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH,
                                            detect_precision_from_arg ?
                                            args[0]->time_precision() :
                                            decimals);
  }
};


class Item_datetime_typecast :public Item_datetime_func
{
  bool detect_precision_from_arg;
public:
  Item_datetime_typecast(Item *a) :Item_datetime_func(a)
  {
    detect_precision_from_arg= true;
  }
  Item_datetime_typecast(const POS &pos, Item *a) :Item_datetime_func(pos, a)
  {
    detect_precision_from_arg= true;
  }

  Item_datetime_typecast(const POS &pos, Item *a, uint8 dec_arg)
    :Item_datetime_func(pos, a)
  {
    detect_precision_from_arg= false;
    decimals= dec_arg;
  }
  void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "cast_as_datetime"; }
  enum Functype functype() const { return TYPECAST_FUNC; }
  const char *cast_type() const { return "datetime"; }
  void fix_length_and_dec()
  {
    maybe_null= 1;
    fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH,
                                            detect_precision_from_arg ?
                                            args[0]->datetime_precision():
                                            decimals);
  }
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
};


class Item_func_makedate :public Item_date_func
{
public:
  Item_func_makedate(const POS &pos, Item *a, Item *b)
    :Item_date_func(pos, a, b)
  { maybe_null= 1; }
  const char *func_name() const { return "makedate"; }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date);
};


class Item_func_add_time :public Item_temporal_hybrid_func
{
  const bool is_date;
  int sign;
  bool val_datetime(MYSQL_TIME *time, my_time_flags_t fuzzy_date);
public:
  Item_func_add_time(Item *a, Item *b, bool type_arg, bool neg_arg)
    :Item_temporal_hybrid_func(a, b), is_date(type_arg)
  {
    sign= neg_arg ? -1 : 1;
  }
  Item_func_add_time(const POS &pos,
                     Item *a, Item *b, bool type_arg, bool neg_arg)
    :Item_temporal_hybrid_func(pos, a, b), is_date(type_arg)
  {
    sign= neg_arg ? -1 : 1;
  }

  void fix_length_and_dec();
  void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "add_time"; }
};


class Item_func_timediff :public Item_time_func
{
public:
  Item_func_timediff(const POS &pos, Item *a, Item *b)
    :Item_time_func(pos, a, b)
  {}
  const char *func_name() const { return "timediff"; }
  void fix_length_and_dec()
  {
    uint dec= MY_MAX(args[0]->time_precision(), args[1]->time_precision());
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH, dec);
    maybe_null= 1;
  }
  bool get_time(MYSQL_TIME *ltime);
};

class Item_func_maketime :public Item_time_func
{
public:
  Item_func_maketime(const POS &pos, Item *a, Item *b, Item *c)
    :Item_time_func(pos, a, b, c) 
  {
    maybe_null= TRUE;
  }
  void fix_length_and_dec()
  {
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH,
                                            MY_MIN(args[2]->decimals,
                                                   DATETIME_MAX_DECIMALS));
  }
  const char *func_name() const { return "maketime"; }
  bool get_time(MYSQL_TIME *ltime);
};

class Item_func_microsecond :public Item_int_func
{
public:
  Item_func_microsecond(const POS &pos, Item *a) :Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "microsecond"; }
  void fix_length_and_dec() 
  { 
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *arg) { return false; }
  bool check_valid_arguments_processor(uchar *arg)
  {
    return !has_time_args();
  }
};


class Item_func_timestamp_diff :public Item_int_func
{
  const interval_type int_type;
public:
  Item_func_timestamp_diff(const POS &pos,
                           Item *a,Item *b,interval_type type_arg)
    :Item_int_func(pos, a,b), int_type(type_arg)
  {}
  const char *func_name() const { return "timestampdiff"; }
  longlong val_int();
  void fix_length_and_dec()
  {
    maybe_null=1;
  }
  virtual void print(String *str, enum_query_type query_type);
};


enum date_time_format
{
  USA_FORMAT, JIS_FORMAT, ISO_FORMAT, EUR_FORMAT, INTERNAL_FORMAT
};

class Item_func_get_format :public Item_str_ascii_func
{
public:
  const timestamp_type type; // keep it public
  Item_func_get_format(const POS &pos, timestamp_type type_arg, Item *a)
    :Item_str_ascii_func(pos, a), type(type_arg)
  {}
  String *val_str_ascii(String *str);
  const char *func_name() const { return "get_format"; }
  void fix_length_and_dec()
  {
    maybe_null= 1;
    decimals=0;
    fix_length_and_charset(17, default_charset());
  }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_str_to_date :public Item_temporal_hybrid_func
{
  timestamp_type cached_timestamp_type;
  bool const_item;
  void fix_from_format(const char *format, size_t length);
protected:
  bool val_datetime(MYSQL_TIME *ltime, my_time_flags_t fuzzy_date);
public:
  Item_func_str_to_date(const POS &pos, Item *a, Item *b)
    :Item_temporal_hybrid_func(pos, a, b), const_item(false)
  {}
  const char *func_name() const { return "str_to_date"; }
  void fix_length_and_dec();
};


class Item_func_last_day :public Item_date_func
{
public:
  Item_func_last_day(const POS &pos, Item *a) :Item_date_func(pos, a)
  { maybe_null= 1; }
  const char *func_name() const { return "last_day"; }
  bool get_date(MYSQL_TIME *res, my_time_flags_t fuzzy_date);
};


/* Function prototypes */

bool make_date_time(Date_time_format *format, MYSQL_TIME *l_time,
                    timestamp_type type, String *str);

#endif /* ITEM_TIMEFUNC_INCLUDED */

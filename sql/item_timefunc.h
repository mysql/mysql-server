/* Copyright (C) 2000-2006 MySQL AB

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


/* Function items used by mysql */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

enum date_time_format_types 
{ 
  TIME_ONLY= 0, TIME_MICROSECOND, DATE_ONLY, DATE_TIME, DATE_TIME_MICROSECOND
};

bool get_interval_value(Item *args,interval_type int_type,
			       String *str_value, INTERVAL *interval);

class Item_func_period_add :public Item_int_func
{
public:
  Item_func_period_add(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "period_add"; }
  void fix_length_and_dec() 
  { 
    max_length=6*MY_CHARSET_BIN_MB_MAXLEN;
  }
};


class Item_func_period_diff :public Item_int_func
{
public:
  Item_func_period_diff(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "period_diff"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=6*MY_CHARSET_BIN_MB_MAXLEN;
  }
};


class Item_func_to_days :public Item_int_func
{
public:
  Item_func_to_days(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "to_days"; }
  void fix_length_and_dec() 
  { 
    decimals=0; 
    max_length=6*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1; 
  }
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_dayofmonth :public Item_int_func
{
public:
  Item_func_dayofmonth(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "dayofmonth"; }
  void fix_length_and_dec() 
  { 
    decimals=0; 
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1; 
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_month :public Item_func
{
public:
  Item_func_month(Item *a) :Item_func(a) {}
  longlong val_int();
  double val_real()
  { DBUG_ASSERT(fixed == 1); return (double) Item_func_month::val_int(); }
  String *val_str(String *str) 
  {
    str->set(val_int(), &my_charset_bin);
    return null_value ? 0 : str;
  }
  const char *func_name() const { return "month"; }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() 
  { 
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1; 
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_monthname :public Item_func_month
{
  MY_LOCALE *locale;
public:
  Item_func_monthname(Item *a) :Item_func_month(a) {}
  const char *func_name() const { return "monthname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return TRUE;}
};


class Item_func_dayofyear :public Item_int_func
{
public:
  Item_func_dayofyear(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "dayofyear"; }
  void fix_length_and_dec() 
  { 
    decimals=0;
    max_length=3*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1; 
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_hour :public Item_int_func
{
public:
  Item_func_hour(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "hour"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_minute :public Item_int_func
{
public:
  Item_func_minute(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "minute"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_quarter :public Item_int_func
{
public:
  Item_func_quarter(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "quarter"; }
  void fix_length_and_dec()
  { 
     decimals=0;
     max_length=1*MY_CHARSET_BIN_MB_MAXLEN;
     maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_second :public Item_int_func
{
public:
  Item_func_second(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "second"; }
  void fix_length_and_dec() 
  { 
    decimals=0;
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_week :public Item_int_func
{
public:
  Item_func_week(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "week"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=2*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
};

class Item_func_yearweek :public Item_int_func
{
public:
  Item_func_yearweek(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "yearweek"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=6*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_year :public Item_int_func
{
public:
  Item_func_year(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "year"; }
  enum_monotonicity_info get_monotonicity_info() const;
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=4*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_weekday :public Item_func
{
  bool odbc_type;
public:
  Item_func_weekday(Item *a,bool type_arg)
    :Item_func(a), odbc_type(type_arg) {}
  longlong val_int();
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    str->set(val_int(), &my_charset_bin);
    return null_value ? 0 : str;
  }
  const char *func_name() const
  {
     return (odbc_type ? "dayofweek" : "weekday");
  }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=1*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};

class Item_func_dayname :public Item_func_weekday
{
  MY_LOCALE *locale;
 public:
  Item_func_dayname(Item *a) :Item_func_weekday(a,0) {}
  const char *func_name() const { return "dayname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
  bool check_partition_func_processor(uchar *int_arg) {return TRUE;}
};


class Item_func_unix_timestamp :public Item_int_func
{
  String value;
public:
  Item_func_unix_timestamp() :Item_int_func() {}
  Item_func_unix_timestamp(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "unix_timestamp"; }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  /*
    UNIX_TIMESTAMP() depends on the current timezone
    (and thus may not be used as a partitioning function)
    when its argument is NOT of the TIMESTAMP type.
  */
  bool is_timezone_dependent_processor(uchar *int_arg)
  {
    return !has_timestamp_args();
  }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=10*MY_CHARSET_BIN_MB_MAXLEN;
  }
};


class Item_func_time_to_sec :public Item_int_func
{
public:
  Item_func_time_to_sec(Item *item) :Item_int_func(item) {}
  longlong val_int();
  const char *func_name() const { return "time_to_sec"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=10*MY_CHARSET_BIN_MB_MAXLEN;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


/*
  This can't be a Item_str_func, because the val_real() functions are special
*/

class Item_date :public Item_func
{
public:
  Item_date() :Item_func() {}
  Item_date(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  String *val_str(String *str);
  longlong val_int();
  double val_real() { return val_real_from_decimal(); }
  const char *func_name() const { return "date"; }
  void fix_length_and_dec()
  { 
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_longlong() { return TRUE; }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
};


class Item_date_func :public Item_str_func
{
public:
  Item_date_func() :Item_str_func() {}
  Item_date_func(Item *a) :Item_str_func(a) {}
  Item_date_func(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_date_func(Item *a,Item *b, Item *c) :Item_str_func(a,b,c) {}
  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_longlong() { return TRUE; }
  double val_real() { return (double) val_int(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
};


class Item_str_timefunc :public Item_str_func
{
public:
  Item_str_timefunc() :Item_str_func() {}
  Item_str_timefunc(Item *a) :Item_str_func(a) {}
  Item_str_timefunc(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_str_timefunc(Item *a, Item *b, Item *c) :Item_str_func(a, b ,c) {}
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  void fix_length_and_dec()
  {
    decimals= DATETIME_DEC;
    max_length=MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_time(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_time_in_field(field);
  }
  longlong val_int() { return val_int_from_decimal(); }
  bool result_as_longlong() { return TRUE; }
};


/* Abstract CURTIME function. Children should define what time zone is used */

class Item_func_curtime :public Item_str_timefunc
{
  longlong value;
  char buff[9*2+32];
  uint buff_length;
public:
  Item_func_curtime() :Item_str_timefunc() {}
  Item_func_curtime(Item *a) :Item_str_timefunc(a) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) value; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  String *val_str(String *str);
  void fix_length_and_dec();
  /* 
    Abstract method that defines which time zone is used for conversion.
    Converts time current time in my_time_t representation to broken-down
    MYSQL_TIME representation using UTC-SYSTEM or per-thread time zone.
  */
  virtual void store_now_in_TIME(MYSQL_TIME *now_time)=0;
  bool result_as_longlong() { return TRUE; }
};


class Item_func_curtime_local :public Item_func_curtime
{
public:
  Item_func_curtime_local() :Item_func_curtime() {}
  Item_func_curtime_local(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "curtime"; }
  virtual void store_now_in_TIME(MYSQL_TIME *now_time);
};


class Item_func_curtime_utc :public Item_func_curtime
{
public:
  Item_func_curtime_utc() :Item_func_curtime() {}
  Item_func_curtime_utc(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "utc_time"; }
  virtual void store_now_in_TIME(MYSQL_TIME *now_time);
};


/* Abstract CURDATE function. See also Item_func_curtime. */

class Item_func_curdate :public Item_date
{
  longlong value;
  MYSQL_TIME ltime;
public:
  Item_func_curdate() :Item_date() {}
  longlong val_int() { DBUG_ASSERT(fixed == 1); return (value) ; }
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  virtual void store_now_in_TIME(MYSQL_TIME *now_time)=0;
};


class Item_func_curdate_local :public Item_func_curdate
{
public:
  Item_func_curdate_local() :Item_func_curdate() {}
  const char *func_name() const { return "curdate"; }
  void store_now_in_TIME(MYSQL_TIME *now_time);
};


class Item_func_curdate_utc :public Item_func_curdate
{
public:
  Item_func_curdate_utc() :Item_func_curdate() {}
  const char *func_name() const { return "utc_date"; }
  void store_now_in_TIME(MYSQL_TIME *now_time);
};


/* Abstract CURRENT_TIMESTAMP function. See also Item_func_curtime */

class Item_func_now :public Item_date_func
{
protected:
  longlong value;
  char buff[20*2+32];	// +32 to make my_snprintf_{8bit|ucs2} happy
  uint buff_length;
  MYSQL_TIME ltime;
public:
  Item_func_now() :Item_date_func() {}
  Item_func_now(Item *a) :Item_date_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  virtual void store_now_in_TIME(MYSQL_TIME *now_time)=0;
};


class Item_func_now_local :public Item_func_now
{
public:
  Item_func_now_local() :Item_func_now() {}
  Item_func_now_local(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "now"; }
  virtual void store_now_in_TIME(MYSQL_TIME *now_time);
  virtual enum Functype functype() const { return NOW_FUNC; }
};


class Item_func_now_utc :public Item_func_now
{
public:
  Item_func_now_utc() :Item_func_now() {}
  Item_func_now_utc(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "utc_timestamp"; }
  virtual void store_now_in_TIME(MYSQL_TIME *now_time);
};


/*
  This is like NOW(), but always uses the real current time, not the
  query_start(). This matches the Oracle behavior.
*/
class Item_func_sysdate_local :public Item_func_now
{
public:
  Item_func_sysdate_local() :Item_func_now() {}
  Item_func_sysdate_local(Item *a) :Item_func_now(a) {}
  bool const_item() const { return 0; }
  const char *func_name() const { return "sysdate"; }
  void store_now_in_TIME(MYSQL_TIME *now_time);
  double val_real();
  longlong val_int();
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  void update_used_tables()
  {
    Item_func_now::update_used_tables();
    used_tables_cache|= RAND_TABLE_BIT;
  }
};


class Item_func_from_days :public Item_date
{
public:
  Item_func_from_days(Item *a) :Item_date(a) {}
  const char *func_name() const { return "from_days"; }
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_date_format :public Item_str_func
{
  int fixed_length;
  const bool is_time_format;
  String value;
public:
  Item_func_date_format(Item *a,Item *b,bool is_time_format_arg)
    :Item_str_func(a,b),is_time_format(is_time_format_arg) {}
  String *val_str(String *str);
  const char *func_name() const
    { return is_time_format ? "time_format" : "date_format"; }
  void fix_length_and_dec();
  uint format_length(const String *format);
  bool eq(const Item *item, bool binary_cmp) const;
};


class Item_func_from_unixtime :public Item_date_func
{
  THD *thd;
 public:
  Item_func_from_unixtime(Item *a) :Item_date_func(a) {}
  longlong val_int();
  String *val_str(String *str);
  const char *func_name() const { return "from_unixtime"; }
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
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
class Item_func_convert_tz :public Item_date_func
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
  Item_func_convert_tz(Item *a, Item *b, Item *c):
    Item_date_func(a, b, c), from_tz_cached(0), to_tz_cached(0) {}
  longlong val_int();
  String *val_str(String *str);
  const char *func_name() const { return "convert_tz"; }
  void fix_length_and_dec();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  void cleanup();
};


class Item_func_sec_to_time :public Item_str_timefunc
{
public:
  Item_func_sec_to_time(Item *item) :Item_str_timefunc(item) {}
  double val_real()
  {
    DBUG_ASSERT(fixed == 1);
    return (double) Item_func_sec_to_time::val_int();
  }
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec()
  { 
    Item_str_timefunc::fix_length_and_dec();
    collation.set(&my_charset_bin);
    maybe_null=1;
  }
  const char *func_name() const { return "sec_to_time"; }
  bool result_as_longlong() { return TRUE; }
};


class Item_date_add_interval :public Item_date_func
{
  String value;
  enum_field_types cached_field_type;

public:
  const interval_type int_type; // keep it public
  const bool date_sub_interval; // keep it public
  Item_date_add_interval(Item *a,Item *b,interval_type type_arg,bool neg_arg)
    :Item_date_func(a,b),int_type(type_arg), date_sub_interval(neg_arg) {}
  String *val_str(String *);
  const char *func_name() const { return "date_add_interval"; }
  void fix_length_and_dec();
  enum_field_types field_type() const { return cached_field_type; }
  longlong val_int();
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
  bool eq(const Item *item, bool binary_cmp) const;
  virtual void print(String *str, enum_query_type query_type);
};


class Item_extract :public Item_int_func
{
  String value;
  bool date_value;
 public:
  const interval_type int_type; // keep it public
  Item_extract(interval_type type_arg, Item *a)
    :Item_int_func(a), int_type(type_arg) {}
  longlong val_int();
  enum Functype functype() const { return EXTRACT_FUNC; }
  const char *func_name() const { return "extract"; }
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  virtual void print(String *str, enum_query_type query_type);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_typecast :public Item_str_func
{
public:
  Item_typecast(Item *a) :Item_str_func(a) {}
  String *val_str(String *a)
  {
    DBUG_ASSERT(fixed == 1);
    String *tmp=args[0]->val_str(a);
    null_value=args[0]->null_value;
    if (tmp)
      tmp->set_charset(collation.collation);
    return tmp;
  }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
  }
  virtual const char* cast_type() const= 0;
  virtual void print(String *str, enum_query_type query_type);
};


class Item_typecast_maybe_null :public Item_typecast
{
public:
  Item_typecast_maybe_null(Item *a) :Item_typecast(a) {}
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
    maybe_null= 1;
  }
};


class Item_char_typecast :public Item_typecast
{
  int cast_length;
  CHARSET_INFO *cast_cs, *from_cs;
  bool charset_conversion;
  String tmp_value;
public:
  Item_char_typecast(Item *a, int length_arg, CHARSET_INFO *cs_arg)
    :Item_typecast(a), cast_length(length_arg), cast_cs(cs_arg) {}
  enum Functype functype() const { return CHAR_TYPECAST_FUNC; }
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "cast_as_char"; }
  const char* cast_type() const { return "char"; };
  String *val_str(String *a);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_date_typecast :public Item_typecast_maybe_null
{
public:
  Item_date_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_date"; }
  String *val_str(String *str);
  bool get_date(MYSQL_TIME *ltime, uint fuzzy_date);
  bool get_time(MYSQL_TIME *ltime);
  const char *cast_type() const { return "date"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }  
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length= 10;
    maybe_null= 1;
  }
  bool result_as_longlong() { return TRUE; }
  longlong val_int();
  double val_real() { return (double) val_int(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
};


class Item_time_typecast :public Item_typecast_maybe_null
{
public:
  Item_time_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_time"; }
  String *val_str(String *str);
  bool get_time(MYSQL_TIME *ltime);
  const char *cast_type() const { return "time"; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_longlong() { return TRUE; }
  longlong val_int();
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_time(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_time_in_field(field);
  }
};


class Item_datetime_typecast :public Item_typecast_maybe_null
{
public:
  Item_datetime_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_datetime"; }
  String *val_str(String *str);
  const char *cast_type() const { return "datetime"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    maybe_null= 1;
    max_length= MAX_DATETIME_FULL_WIDTH * MY_CHARSET_BIN_MB_MAXLEN;
    decimals= DATETIME_DEC;
  }
  bool result_as_longlong() { return TRUE; }
  longlong val_int();
  double val_real() { return val_real_from_decimal(); }
  double val() { return (double) val_int(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_date_in_field(field);
  }
};

class Item_func_makedate :public Item_date_func
{
public:
  Item_func_makedate(Item *a,Item *b) :Item_date_func(a,b) {}
  String *val_str(String *str);
  const char *func_name() const { return "makedate"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  longlong val_int();
};


class Item_func_add_time :public Item_str_func
{
  const bool is_date;
  int sign;
  enum_field_types cached_field_type;

public:
  Item_func_add_time(Item *a, Item *b, bool type_arg, bool neg_arg)
    :Item_str_func(a, b), is_date(type_arg) { sign= neg_arg ? -1 : 1; }
  String *val_str(String *str);
  enum_field_types field_type() const { return cached_field_type; }
  void fix_length_and_dec();

  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  virtual void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "add_time"; }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    DBUG_ASSERT(fixed == 1);
    if (cached_field_type == MYSQL_TYPE_TIME)
      return  val_decimal_from_time(decimal_value);
    if (cached_field_type == MYSQL_TYPE_DATETIME)
      return  val_decimal_from_date(decimal_value);
    return Item_str_func::val_decimal(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    if (cached_field_type == MYSQL_TYPE_TIME)
      return save_time_in_field(field);
    if (cached_field_type == MYSQL_TYPE_DATETIME)
      return save_date_in_field(field);
    return Item_str_func::save_in_field(field, no_conversions);
  }
};

class Item_func_timediff :public Item_str_timefunc
{
public:
  Item_func_timediff(Item *a, Item *b)
    :Item_str_timefunc(a, b) {}
  String *val_str(String *str);
  const char *func_name() const { return "timediff"; }
  void fix_length_and_dec()
  {
    Item_str_timefunc::fix_length_and_dec();
    maybe_null= 1;
  }
};

class Item_func_maketime :public Item_str_timefunc
{
public:
  Item_func_maketime(Item *a, Item *b, Item *c)
    :Item_str_timefunc(a, b, c) 
  {
    maybe_null= TRUE;
  }
  String *val_str(String *str);
  const char *func_name() const { return "maketime"; }
};

class Item_func_microsecond :public Item_int_func
{
public:
  Item_func_microsecond(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "microsecond"; }
  void fix_length_and_dec() 
  { 
    decimals=0;
    maybe_null=1;
  }
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};


class Item_func_timestamp_diff :public Item_int_func
{
  const interval_type int_type;
public:
  Item_func_timestamp_diff(Item *a,Item *b,interval_type type_arg)
    :Item_int_func(a,b), int_type(type_arg) {}
  const char *func_name() const { return "timestampdiff"; }
  longlong val_int();
  void fix_length_and_dec()
  {
    decimals=0;
    maybe_null=1;
  }
  virtual void print(String *str, enum_query_type query_type);
};


enum date_time_format
{
  USA_FORMAT, JIS_FORMAT, ISO_FORMAT, EUR_FORMAT, INTERNAL_FORMAT
};

class Item_func_get_format :public Item_str_func
{
public:
  const timestamp_type type; // keep it public
  Item_func_get_format(timestamp_type type_arg, Item *a)
    :Item_str_func(a), type(type_arg)
  {}
  String *val_str(String *str);
  const char *func_name() const { return "get_format"; }
  void fix_length_and_dec()
  {
    maybe_null= 1;
    decimals=0;
    max_length=17*MY_CHARSET_BIN_MB_MAXLEN;
  }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_str_to_date :public Item_str_func
{
  enum_field_types cached_field_type;
  date_time_format_types cached_format_type;
  timestamp_type cached_timestamp_type;
  bool const_item;
public:
  Item_func_str_to_date(Item *a, Item *b)
    :Item_str_func(a, b), const_item(false)
  {}
  String *val_str(String *str);
  bool get_date(MYSQL_TIME *ltime, uint fuzzy_date);
  const char *func_name() const { return "str_to_date"; }
  enum_field_types field_type() const { return cached_field_type; }
  void fix_length_and_dec();
  Field *tmp_table_field(TABLE *table)
  {
    return tmp_table_field_from_field_type(table, 1);
  }
};


class Item_func_last_day :public Item_date
{
public:
  Item_func_last_day(Item *a) :Item_date(a) {}
  const char *func_name() const { return "last_day"; }
  bool get_date(MYSQL_TIME *res, uint fuzzy_date);
};

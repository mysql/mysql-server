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

enum date_time_format_types 
{ 
  TIME_ONLY= 0, TIME_MICROSECOND, DATE_ONLY, DATE_TIME, DATE_TIME_MICROSECOND
};

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
};


class Item_func_month :public Item_func
{
public:
  Item_func_month(Item *a) :Item_func(a) {}
  longlong val_int();
  double val()
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
};


class Item_func_monthname :public Item_func_month
{
public:
  Item_func_monthname(Item *a) :Item_func_month(a) {}
  const char *func_name() const { return "monthname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() 
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=10*my_charset_bin.mbmaxlen;
    maybe_null=1; 
  }
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
};


class Item_func_year :public Item_int_func
{
public:
  Item_func_year(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "year"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=4*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
};


class Item_func_weekday :public Item_func
{
  bool odbc_type;
public:
  Item_func_weekday(Item *a,bool type_arg)
    :Item_func(a), odbc_type(type_arg) {}
  longlong val_int();
  double val() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  String *val_str(String *str)
  {
    DBUG_ASSERT(fixed == 1);
    str->set(val_int(), &my_charset_bin);
    return null_value ? 0 : str;
  }
  const char *func_name() const { return "weekday"; }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=1*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1;
  }
};

class Item_func_dayname :public Item_func_weekday
{
 public:
  Item_func_dayname(Item *a) :Item_func_weekday(a,0) {}
  const char *func_name() const { return "dayname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() 
  { 
    collation.set(&my_charset_bin);
    decimals=0; 
    max_length=9*MY_CHARSET_BIN_MB_MAXLEN;
    maybe_null=1; 
  }
};


class Item_func_unix_timestamp :public Item_int_func
{
  String value;
public:
  Item_func_unix_timestamp() :Item_int_func() {}
  Item_func_unix_timestamp(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "unix_timestamp"; }
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
};


/* This can't be a Item_str_func, because the val() functions are special */

class Item_date :public Item_func
{
public:
  Item_date() :Item_func() {}
  Item_date(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  String *val_str(String *str);
  longlong val_int();
  double val() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  const char *func_name() const { return "date"; }
  void fix_length_and_dec()
  { 
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  int save_in_field(Field *to, bool no_conversions);
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_date(maybe_null, name, t_arg, &my_charset_bin));
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
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_datetime(maybe_null, name, t_arg, &my_charset_bin));
  }
};


/* Abstract CURTIME function. Children should define what time zone is used */

class Item_func_curtime :public Item_func
{
  longlong value;
  char buff[9*2+32];
  uint buff_length;
public:
  Item_func_curtime() :Item_func() {}
  Item_func_curtime(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  double val() { DBUG_ASSERT(fixed == 1); return (double) value; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  String *val_str(String *str);
  void fix_length_and_dec();
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
  }
  /* 
    Abstract method that defines which time zone is used for conversion.
    Converts time current time in my_time_t representation to broken-down
    TIME representation using UTC-SYSTEM or per-thread time zone.
  */
  virtual void store_now_in_TIME(TIME *now_time)=0;
};


class Item_func_curtime_local :public Item_func_curtime
{
public:
  Item_func_curtime_local() :Item_func_curtime() {}
  Item_func_curtime_local(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "curtime"; }
  virtual void store_now_in_TIME(TIME *now_time);
};


class Item_func_curtime_utc :public Item_func_curtime
{
public:
  Item_func_curtime_utc() :Item_func_curtime() {}
  Item_func_curtime_utc(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "utc_time"; }
  virtual void store_now_in_TIME(TIME *now_time);
};


/* Abstract CURDATE function. See also Item_func_curtime. */

class Item_func_curdate :public Item_date
{
  longlong value;
  TIME ltime;
public:
  Item_func_curdate() :Item_date() {}
  longlong val_int() { DBUG_ASSERT(fixed == 1); return (value) ; }
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(TIME *res, uint fuzzy_date);
  virtual void store_now_in_TIME(TIME *now_time)=0;
};


class Item_func_curdate_local :public Item_func_curdate
{
public:
  Item_func_curdate_local() :Item_func_curdate() {}
  const char *func_name() const { return "curdate"; }
  void store_now_in_TIME(TIME *now_time);
};


class Item_func_curdate_utc :public Item_func_curdate
{
public:
  Item_func_curdate_utc() :Item_func_curdate() {}
  const char *func_name() const { return "utc_date"; }
  void store_now_in_TIME(TIME *now_time);
};


/* Abstract CURRENT_TIMESTAMP function. See also Item_func_curtime */

class Item_func_now :public Item_date_func
{
  longlong value;
  char buff[20*2+32];	// +32 to make my_snprintf_{8bit|ucs2} happy
  uint buff_length;
  TIME ltime;
public:
  Item_func_now() :Item_date_func() {}
  Item_func_now(Item *a) :Item_date_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  double val()	     { DBUG_ASSERT(fixed == 1); return (double) value; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(TIME *res, uint fuzzy_date);
  virtual void store_now_in_TIME(TIME *now_time)=0;
};


class Item_func_now_local :public Item_func_now
{
public:
  Item_func_now_local() :Item_func_now() {}
  Item_func_now_local(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "now"; }
  virtual void store_now_in_TIME(TIME *now_time);
  virtual enum Functype functype() const { return NOW_FUNC; }
};


class Item_func_now_utc :public Item_func_now
{
public:
  Item_func_now_utc() :Item_func_now() {}
  Item_func_now_utc(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "utc_timestamp"; }
  virtual void store_now_in_TIME(TIME *now_time);
};


class Item_func_from_days :public Item_date
{
public:
  Item_func_from_days(Item *a) :Item_date(a) {}
  const char *func_name() const { return "from_days"; }
  bool get_date(TIME *res, uint fuzzy_date);
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
  const char *func_name() const { return "date_format"; }
  void fix_length_and_dec();
  uint format_length(const String *format);
};


class Item_func_from_unixtime :public Item_date_func
{
  THD *thd;
 public:
  Item_func_from_unixtime(Item *a) :Item_date_func(a) {}
  double val()
  {
    DBUG_ASSERT(fixed == 1);
    return (double) Item_func_from_unixtime::val_int();
  }
  longlong val_int();
  String *val_str(String *str);
  const char *func_name() const { return "from_unixtime"; }
  void fix_length_and_dec();
  bool get_date(TIME *res, uint fuzzy_date);
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
  /* Cached pointer to list of pre-opened time zone tables. */
  TABLE_LIST *tz_tables;
  /*
    If time zone parameters are constants we are caching objects that
    represent them.
  */
  Time_zone *from_tz, *to_tz;
 public:
  Item_func_convert_tz(Item *a, Item *b, Item *c):
    Item_date_func(a, b, c) {}
  longlong val_int();
  double val() { return (double) val_int(); }
  String *val_str(String *str);
  const char *func_name() const { return "convert_tz"; }
  bool fix_fields(THD *, struct st_table_list *, Item **);
  void fix_length_and_dec();
  bool get_date(TIME *res, uint fuzzy_date);
};


class Item_func_sec_to_time :public Item_str_func
{
public:
  Item_func_sec_to_time(Item *item) :Item_str_func(item) {}
  double val()
  {
    DBUG_ASSERT(fixed == 1);
    return (double) Item_func_sec_to_time::val_int();
  }
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec()
  { 
    collation.set(&my_charset_bin);
    maybe_null=1;
    max_length=MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  const char *func_name() const { return "sec_to_time"; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
  }
};

/*
  The following must be sorted so that simple intervals comes first.
  (get_interval_value() depends on this)
*/

enum interval_type
{
  INTERVAL_YEAR, INTERVAL_MONTH, INTERVAL_DAY, INTERVAL_HOUR, INTERVAL_MINUTE,
  INTERVAL_SECOND, INTERVAL_MICROSECOND ,INTERVAL_YEAR_MONTH,
  INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE, INTERVAL_DAY_SECOND,
  INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND, INTERVAL_MINUTE_SECOND,
  INTERVAL_DAY_MICROSECOND, INTERVAL_HOUR_MICROSECOND,
  INTERVAL_MINUTE_MICROSECOND, INTERVAL_SECOND_MICROSECOND
};

class Item_date_add_interval :public Item_date_func
{
  const interval_type int_type;
  String value;
  const bool date_sub_interval;
  enum_field_types cached_field_type;

public:
  Item_date_add_interval(Item *a,Item *b,interval_type type_arg,bool neg_arg)
    :Item_date_func(a,b),int_type(type_arg), date_sub_interval(neg_arg) {}
  String *val_str(String *);
  const char *func_name() const { return "date_add_interval"; }
  void fix_length_and_dec();
  enum_field_types field_type() const { return cached_field_type; }
  double val() { DBUG_ASSERT(fixed == 1); return (double) val_int(); }
  longlong val_int();
  bool get_date(TIME *res, uint fuzzy_date);
  void print(String *str);
};


class Item_extract :public Item_int_func
{
  const interval_type int_type;
  String value;
  bool date_value;
 public:
  Item_extract(interval_type type_arg, Item *a)
    :Item_int_func(a), int_type(type_arg) {}
  longlong val_int();
  const char *func_name() const { return "extract"; }
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  void print(String *str);
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
  void print(String *str);
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
  CHARSET_INFO *cast_cs;
  bool charset_conversion;
  String tmp_value;
public:
  Item_char_typecast(Item *a, int length_arg, CHARSET_INFO *cs_arg)
    :Item_typecast(a), cast_length(length_arg), cast_cs(cs_arg) {}
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "cast_as_char"; }
  const char* cast_type() const { return "char"; };
  String *val_str(String *a);
  void fix_length_and_dec();
  void print(String *str);
};


class Item_date_typecast :public Item_typecast_maybe_null
{
public:
  Item_date_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_date"; }
  String *val_str(String *str);
  bool get_date(TIME *ltime, uint fuzzy_date);
  const char *cast_type() const { return "date"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_date(maybe_null, name, t_arg, &my_charset_bin));
  }  
};


class Item_time_typecast :public Item_typecast_maybe_null
{
public:
  Item_time_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_time"; }
  String *val_str(String *str);
  bool get_time(TIME *ltime);
  const char *cast_type() const { return "time"; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
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
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_datetime(maybe_null, name, t_arg, &my_charset_bin));
  }
};

class Item_func_makedate :public Item_str_func
{
public:
  Item_func_makedate(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *str);
  const char *func_name() const { return "makedate"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_date(maybe_null, name, t_arg, &my_charset_bin));
  }
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

/*
  TODO:
       Change this when we support 
       microseconds in TIME/DATETIME
*/
  Field *tmp_table_field(TABLE *t_arg)
  {
    if (cached_field_type == MYSQL_TYPE_TIME)
      return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
    else if (cached_field_type == MYSQL_TYPE_DATETIME)
      return (new Field_datetime(maybe_null, name, t_arg, &my_charset_bin));
    return (new Field_string(max_length, maybe_null, name, t_arg, &my_charset_bin));
  }
  void print(String *str);
};

class Item_func_timediff :public Item_str_func
{
public:
  Item_func_timediff(Item *a, Item *b)
    :Item_str_func(a, b) {}
  String *val_str(String *str);
  const char *func_name() const { return "timediff"; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(TABLE *t_arg)
  {
      return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
  }
};

class Item_func_maketime :public Item_str_func
{
public:
  Item_func_maketime(Item *a, Item *b, Item *c)
    :Item_str_func(a, b ,c) {}
  String *val_str(String *str);
  const char *func_name() const { return "maketime"; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
  }
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
};


enum date_time_format
{
  USA_FORMAT, JIS_FORMAT, ISO_FORMAT, EUR_FORMAT, INTERNAL_FORMAT
};

class Item_func_get_format :public Item_str_func
{
  const timestamp_type type;
public:
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
  void print(String *str);
};


class Item_func_str_to_date :public Item_str_func
{
  enum_field_types cached_field_type;
  date_time_format_types cached_format_type;
  timestamp_type cached_timestamp_type;
  bool const_item;
public:
  Item_func_str_to_date(Item *a, Item *b)
    :Item_str_func(a, b)
  {}
  String *val_str(String *str);
  bool get_date(TIME *ltime, uint fuzzy_date);
  const char *func_name() const { return "str_to_date"; }
  enum_field_types field_type() const { return cached_field_type; }
  void fix_length_and_dec();
  Field *tmp_table_field(TABLE *t_arg);
};


class Item_func_last_day :public Item_date
{
public:
  Item_func_last_day(Item *a) :Item_date(a) {}
  const char *func_name() const { return "last_day"; }
  bool get_date(TIME *res, uint fuzzy_date);
};

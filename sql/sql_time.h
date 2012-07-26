/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_TIME_INCLUDED
#define SQL_TIME_INCLUDED

#include "my_global.h"                          /* ulong */
#include "my_time.h"
#include "mysql_time.h"                         /* timestamp_type */
#include "sql_error.h"                          /* Sql_condition */
#include "structs.h"                            /* INTERVAL */

typedef enum enum_mysql_timestamp_type timestamp_type;
typedef struct st_date_time_format DATE_TIME_FORMAT;
typedef struct st_known_date_time_format KNOWN_DATE_TIME_FORMAT;

/* Flags for calc_week() function.  */
#define WEEK_MONDAY_FIRST    1
#define WEEK_YEAR            2
#define WEEK_FIRST_WEEKDAY   4

ulong convert_period_to_month(ulong period);
ulong convert_month_to_period(ulong month);
void mix_date_and_time(MYSQL_TIME *ldate, const MYSQL_TIME *ltime);
void get_date_from_daynr(long daynr,uint *year, uint *month, uint *day);
my_time_t TIME_to_timestamp(THD *thd, const MYSQL_TIME *t, my_bool *not_exist);
bool datetime_with_no_zero_in_date_to_timeval(THD *thd, const MYSQL_TIME *t,
                                              struct timeval *tm,
                                              int *warnings);
bool datetime_to_timeval(THD *thd, const MYSQL_TIME *t,
                         struct timeval *tm, int *warnings);
bool str_to_datetime_with_warn(String *str,  MYSQL_TIME *l_time, uint flags);
bool my_decimal_to_datetime_with_warn(const my_decimal *decimal,
                                      MYSQL_TIME *ltime, uint flags);
bool my_double_to_datetime_with_warn(double nr, MYSQL_TIME *ltime, uint flags);
bool my_longlong_to_datetime_with_warn(longlong nr,
                                       MYSQL_TIME *ltime, uint flags);
bool my_decimal_to_time_with_warn(const my_decimal *decimal,
                                  MYSQL_TIME *ltime);
bool my_double_to_time_with_warn(double nr, MYSQL_TIME *ltime);
bool my_longlong_to_time_with_warn(longlong nr, MYSQL_TIME *ltime);
bool str_to_time_with_warn(String *str, MYSQL_TIME *l_time);
void time_to_datetime(THD *thd, const MYSQL_TIME *tm, MYSQL_TIME *dt);
inline void datetime_to_time(MYSQL_TIME *ltime)
{
  ltime->year= ltime->month= ltime->day= 0;
  ltime->time_type= MYSQL_TIMESTAMP_TIME;
}
inline void datetime_to_date(MYSQL_TIME *ltime)
{
  ltime->hour= ltime->minute= ltime->second= ltime->second_part= 0;
  ltime->time_type= MYSQL_TIMESTAMP_DATE;
}
inline void date_to_datetime(MYSQL_TIME *ltime)
{
  ltime->time_type= MYSQL_TIMESTAMP_DATETIME;
}
void make_truncated_value_warning(THD *thd,
                                  Sql_condition::enum_warning_level level,
                                  ErrConvString val,
                                  timestamp_type time_type,
                                  const char *field_name);
inline void make_truncated_value_warning(ErrConvString val,
                                         timestamp_type time_type)
{
  make_truncated_value_warning(current_thd, Sql_condition::WARN_LEVEL_WARN,
                               val, time_type, NullS);
}
extern DATE_TIME_FORMAT *date_time_format_make(timestamp_type format_type,
					       const char *format_str,
					       uint format_length);
extern DATE_TIME_FORMAT *date_time_format_copy(THD *thd,
					       DATE_TIME_FORMAT *format);
const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				     timestamp_type type);
void make_date(const DATE_TIME_FORMAT *format, const MYSQL_TIME *l_time,
               String *str);
void make_time(const DATE_TIME_FORMAT *format, const MYSQL_TIME *l_time,
               String *str, uint dec);
void make_datetime(const DATE_TIME_FORMAT *format, const MYSQL_TIME *l_time,
                   String *str, uint dec);
bool my_TIME_to_str(const MYSQL_TIME *ltime, String *str, uint dec);

/* MYSQL_TIME operations */
bool date_add_interval(MYSQL_TIME *ltime, interval_type int_type,
                       INTERVAL interval);
bool calc_time_diff(const MYSQL_TIME *l_time1, const MYSQL_TIME *l_time2,
                    int l_sign, longlong *seconds_out, long *microseconds_out);
int my_time_compare(MYSQL_TIME *a, MYSQL_TIME *b);
void localtime_to_TIME(MYSQL_TIME *to, struct tm *from);
void calc_time_from_sec(MYSQL_TIME *to, longlong seconds, long microseconds);
uint calc_week(MYSQL_TIME *l_time, uint week_behaviour, uint *year);

int calc_weekday(long daynr,bool sunday_first_day_of_week);
bool parse_date_time_format(timestamp_type format_type, 
                            const char *format, uint format_length,
                            DATE_TIME_FORMAT *date_time_format);
/* Character set-aware version of str_to_time() */
bool str_to_time(const CHARSET_INFO *cs, const char *str, uint length,
                 MYSQL_TIME *l_time, uint flags, MYSQL_TIME_STATUS *status);
static inline bool
str_to_time(const String *str, MYSQL_TIME *ltime, uint flags,
            MYSQL_TIME_STATUS *status)
{
  return str_to_time(str->charset(), str->ptr(), str->length(),
                     ltime, flags, status);
}

bool time_add_nanoseconds_with_round(MYSQL_TIME *ltime, uint nanoseconds,  
                                     int *warnings);
/* Character set-aware version of str_to_datetime() */
bool str_to_datetime(const CHARSET_INFO *cs,
                     const char *str, uint length,
                     MYSQL_TIME *l_time, uint flags,
                     MYSQL_TIME_STATUS *status);
static inline bool
str_to_datetime(const String *str, MYSQL_TIME *ltime, uint flags,
                MYSQL_TIME_STATUS *status)
{
  return str_to_datetime(str->charset(), str->ptr(), str->length(),
                         ltime, flags, status);
}

bool datetime_add_nanoseconds_with_round(MYSQL_TIME *ltime,
                                         uint nanoseconds, int *warnings);

/* convenience wrapper */
inline bool parse_date_time_format(timestamp_type format_type, 
                                   DATE_TIME_FORMAT *date_time_format)
{
  return parse_date_time_format(format_type,
                                date_time_format->format.str,
                                date_time_format->format.length,
                                date_time_format);
}


extern DATE_TIME_FORMAT global_date_format;
extern DATE_TIME_FORMAT global_datetime_format;
extern DATE_TIME_FORMAT global_time_format;
extern KNOWN_DATE_TIME_FORMAT known_date_time_formats[];
extern LEX_STRING interval_type_to_name[];

/* Date/time rounding and truncation functions */
inline long my_time_fraction_remainder(long nr, uint decimals)
{
  DBUG_ASSERT(decimals <= DATETIME_MAX_DECIMALS);
  return nr % (long) log_10_int[DATETIME_MAX_DECIMALS - decimals];
}
inline void my_time_trunc(MYSQL_TIME *ltime, uint decimals)
{
  ltime->second_part-= my_time_fraction_remainder(ltime->second_part, decimals);
}
inline void my_datetime_trunc(MYSQL_TIME *ltime, uint decimals)
{
  return my_time_trunc(ltime, decimals);
}
inline void my_timeval_trunc(struct timeval *tv, uint decimals)
{
  tv->tv_usec-= my_time_fraction_remainder(tv->tv_usec, decimals);
}
bool my_time_round(MYSQL_TIME *ltime, uint decimals);
bool my_datetime_round(MYSQL_TIME *ltime, uint decimals, int *warnings);
bool my_timeval_round(struct timeval *tv, uint decimals);


inline ulonglong TIME_to_ulonglong_datetime_round(const MYSQL_TIME *ltime)
{
  // Catch simple cases
  if (ltime->second_part < 500000)
    return TIME_to_ulonglong_datetime(ltime);
  if (ltime->second < 59)
    return TIME_to_ulonglong_datetime(ltime) + 1;
  // Corner case e.g. 'YYYY-MM-DD hh:mm:59.5'. Proceed with slower method.
  int warnings= 0;
  MYSQL_TIME tmp= *ltime;
  my_datetime_round(&tmp, 0, &warnings);
  return TIME_to_ulonglong_datetime(&tmp);// + TIME_microseconds_round(ltime);
}


inline ulonglong TIME_to_ulonglong_time_round(const MYSQL_TIME *ltime)
{
  if (ltime->second_part < 500000)
    return TIME_to_ulonglong_time(ltime);
  if (ltime->second < 59)
    return TIME_to_ulonglong_time(ltime) + 1;
  // Corner case e.g. 'hh:mm:59.5'. Proceed with slower method.
  MYSQL_TIME tmp= *ltime;
  my_time_round(&tmp, 0);
  return TIME_to_ulonglong_time(&tmp);
}


inline ulonglong TIME_to_ulonglong_round(const MYSQL_TIME *ltime)
{
  switch (ltime->time_type)
  {
  case MYSQL_TIMESTAMP_TIME:
    return TIME_to_ulonglong_time_round(ltime);
  case MYSQL_TIMESTAMP_DATETIME:
    return TIME_to_ulonglong_datetime_round(ltime);
  case MYSQL_TIMESTAMP_DATE:
    return TIME_to_ulonglong_date(ltime);
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}


inline double TIME_microseconds(const MYSQL_TIME *ltime)
{
  return (double) ltime->second_part / 1000000;
}

inline double TIME_to_double_datetime(const MYSQL_TIME *ltime)
{
  return (double) TIME_to_ulonglong_datetime(ltime) + TIME_microseconds(ltime);
}


inline double TIME_to_double_time(const MYSQL_TIME *ltime)
{
  return (double) TIME_to_ulonglong_time(ltime) + TIME_microseconds(ltime);
}


inline double TIME_to_double(const MYSQL_TIME *ltime)
{
  return (double) TIME_to_ulonglong(ltime) + TIME_microseconds(ltime);
}


static inline bool
check_fuzzy_date(const MYSQL_TIME *ltime, uint fuzzydate)
{
  return !(fuzzydate & TIME_FUZZY_DATE) && (!ltime->month || !ltime->day);
}

static inline bool
non_zero_date(const MYSQL_TIME *ltime)
{
  return ltime->year || ltime->month || ltime->day;
}

static inline bool
non_zero_time(const MYSQL_TIME *ltime)
{
  return ltime->hour || ltime->minute || ltime->second || ltime->second_part;
}

longlong TIME_to_longlong_packed(const MYSQL_TIME *tm,
                                 enum enum_field_types type);
void TIME_from_longlong_packed(MYSQL_TIME *ltime,
                               enum enum_field_types type,
                               longlong packed_value);
my_decimal *my_decimal_from_datetime_packed(my_decimal *dec,
                                            enum enum_field_types type,
                                            longlong packed_value);

longlong longlong_from_datetime_packed(enum enum_field_types type,
                                       longlong packed_value);

double double_from_datetime_packed(enum enum_field_types type,
                                   longlong packed_value);

static inline
timestamp_type field_type_to_timestamp_type(enum enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME: return MYSQL_TIMESTAMP_TIME;
  case MYSQL_TYPE_DATE: return MYSQL_TIMESTAMP_DATE;
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATETIME: return MYSQL_TIMESTAMP_DATETIME;
  default: return MYSQL_TIMESTAMP_NONE;
  }
}
#endif /* SQL_TIME_INCLUDED */

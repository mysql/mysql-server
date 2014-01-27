/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_TIME_INCLUDED
#define SQL_TIME_INCLUDED

#include "my_global.h"                          /* ulong */
#include "my_time.h"
#include "mysql_time.h"                         /* timestamp_type */
#include "sql_error.h"                          /* MYSQL_ERROR */
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
void time_to_daytime_interval(MYSQL_TIME *l_time);
bool get_date_from_daynr(long daynr,uint *year, uint *month, uint *day);
my_time_t TIME_to_timestamp(THD *thd, const MYSQL_TIME *t, uint *error_code);
bool str_to_time_with_warn(CHARSET_INFO *cs, const char *str, uint length,
                           MYSQL_TIME *l_time, ulonglong fuzzydate);
timestamp_type str_to_datetime_with_warn(CHARSET_INFO *cs, const char *str,
                                         uint length, MYSQL_TIME *l_time,
                                         ulonglong flags);
bool double_to_datetime_with_warn(double value, MYSQL_TIME *ltime,
                                  ulonglong fuzzydate,
                                  const char *name);
bool decimal_to_datetime_with_warn(const my_decimal *value, MYSQL_TIME *ltime,
                                   ulonglong fuzzydate,
                                   const char *name);
bool int_to_datetime_with_warn(longlong value, MYSQL_TIME *ltime,
                               ulonglong fuzzydate,
                               const char *name);

void make_truncated_value_warning(THD *thd, MYSQL_ERROR::enum_warning_level level,
                                  const ErrConv *str_val,
                                  timestamp_type time_type,
                                  const char *field_name);

static inline void make_truncated_value_warning(THD *thd,
                MYSQL_ERROR::enum_warning_level level, const char *str_val,
                uint str_length, timestamp_type time_type,
                const char *field_name)
{
  const ErrConvString str(str_val, str_length, &my_charset_bin);
  make_truncated_value_warning(thd, level, &str, time_type, field_name);
}

extern DATE_TIME_FORMAT *date_time_format_make(timestamp_type format_type,
					       const char *format_str,
					       uint format_length);
extern DATE_TIME_FORMAT *date_time_format_copy(THD *thd,
					       DATE_TIME_FORMAT *format);
const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				     timestamp_type type);
/* MYSQL_TIME operations */
bool date_add_interval(MYSQL_TIME *ltime, interval_type int_type,
                       INTERVAL interval);
bool calc_time_diff(MYSQL_TIME *l_time1, MYSQL_TIME *l_time2, int l_sign,
                    longlong *seconds_out, long *microseconds_out);
int my_time_compare(MYSQL_TIME *a, MYSQL_TIME *b);
void localtime_to_TIME(MYSQL_TIME *to, struct tm *from);
void calc_time_from_sec(MYSQL_TIME *to, long seconds, long microseconds);
uint calc_week(MYSQL_TIME *l_time, uint week_behaviour, uint *year);

int calc_weekday(long daynr,bool sunday_first_day_of_week);
bool parse_date_time_format(timestamp_type format_type, 
                            const char *format, uint format_length,
                            DATE_TIME_FORMAT *date_time_format);
/* Character set-aware version of str_to_time() */
timestamp_type str_to_time(CHARSET_INFO *cs, const char *str,uint length,
                 MYSQL_TIME *l_time, ulonglong fuzzydate, int *warning);
/* Character set-aware version of str_to_datetime() */
timestamp_type str_to_datetime(CHARSET_INFO *cs,
                               const char *str, uint length,
                               MYSQL_TIME *l_time, ulonglong flags, int *was_cut);

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


static inline bool
non_zero_date(const MYSQL_TIME *ltime)
{
  return ltime->year || ltime->month || ltime->day;
}
static inline bool
check_date(const MYSQL_TIME *ltime, ulonglong flags, int *was_cut)
{
 return check_date(ltime, non_zero_date(ltime), flags, was_cut);
}
bool check_date_with_warn(const MYSQL_TIME *ltime, ulonglong fuzzy_date,
                          timestamp_type ts_type);
bool adjust_time_range_with_warn(MYSQL_TIME *ltime, uint dec);

#endif /* SQL_TIME_INCLUDED */

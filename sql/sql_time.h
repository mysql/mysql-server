/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_TIME_INCLUDED
#define SQL_TIME_INCLUDED

#include "my_config.h"

#include <stddef.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_time.h"
#include "mysql_time.h"    /* timestamp_type */
#include "sql/sql_error.h" /* Sql_condition */
#include "sql_string.h"

class THD;
class my_decimal;

struct Date_time_format {
  uchar positions[8];
  LEX_STRING format;
};

struct Interval {
  ulong year, month, day, hour;
  ulonglong minute, second, second_part;
  bool neg;
};

struct Known_date_time_format {
  const char *format_name;
  const char *date_format;
  const char *datetime_format;
  const char *time_format;
};

/* Flags for calc_week() function.  */
#define WEEK_MONDAY_FIRST 1
#define WEEK_YEAR 2
#define WEEK_FIRST_WEEKDAY 4

bool valid_period(ulong period);
ulong convert_period_to_month(ulong period);
ulong convert_month_to_period(ulong month);
void mix_date_and_time(MYSQL_TIME *ldate, const MYSQL_TIME *ltime);
void get_date_from_daynr(long daynr, uint *year, uint *month, uint *day);
my_time_t TIME_to_timestamp(THD *thd, const MYSQL_TIME *t, bool *not_exist);
bool datetime_with_no_zero_in_date_to_timeval(THD *thd, const MYSQL_TIME *t,
                                              struct timeval *tm,
                                              int *warnings);
bool datetime_to_timeval(THD *thd, const MYSQL_TIME *t, struct timeval *tm,
                         int *warnings);
bool str_to_datetime_with_warn(String *str, MYSQL_TIME *l_time,
                               my_time_flags_t flags);
bool my_decimal_to_datetime_with_warn(const my_decimal *decimal,
                                      MYSQL_TIME *ltime, my_time_flags_t flags);
bool my_double_to_datetime_with_warn(double nr, MYSQL_TIME *ltime,
                                     my_time_flags_t flags);
bool my_longlong_to_datetime_with_warn(longlong nr, MYSQL_TIME *ltime,
                                       my_time_flags_t flags);
bool my_decimal_to_time_with_warn(const my_decimal *decimal, MYSQL_TIME *ltime);
bool my_double_to_time_with_warn(double nr, MYSQL_TIME *ltime);
bool my_longlong_to_time_with_warn(longlong nr, MYSQL_TIME *ltime);
bool str_to_time_with_warn(String *str, MYSQL_TIME *l_time);
void time_to_datetime(THD *thd, const MYSQL_TIME *tm, MYSQL_TIME *dt);
inline void datetime_to_time(MYSQL_TIME *ltime) {
  ltime->year = ltime->month = ltime->day = 0;
  ltime->time_type = MYSQL_TIMESTAMP_TIME;
}
inline void datetime_to_date(MYSQL_TIME *ltime) {
  ltime->hour = ltime->minute = ltime->second = ltime->second_part = 0;
  ltime->time_type = MYSQL_TIMESTAMP_DATE;
}
inline void date_to_datetime(MYSQL_TIME *ltime) {
  ltime->time_type = MYSQL_TIMESTAMP_DATETIME;
}
bool make_truncated_value_warning(THD *thd,
                                  Sql_condition::enum_severity_level level,
                                  ErrConvString val, timestamp_type time_type,
                                  const char *field_name);

const char *get_date_time_format_str(Known_date_time_format *format,
                                     timestamp_type type);
void make_date(const Date_time_format *format, const MYSQL_TIME *l_time,
               String *str);
void make_time(const Date_time_format *format, const MYSQL_TIME *l_time,
               String *str, uint dec);
void make_datetime(const Date_time_format *format, const MYSQL_TIME *l_time,
                   String *str, uint dec);
bool my_TIME_to_str(const MYSQL_TIME *ltime, String *str, uint dec);

/* MYSQL_TIME operations */
bool date_add_interval(MYSQL_TIME *ltime, interval_type int_type,
                       Interval interval);
bool calc_time_diff(const MYSQL_TIME *l_time1, const MYSQL_TIME *l_time2,
                    int l_sign, longlong *seconds_out, long *microseconds_out);
int my_time_compare(const MYSQL_TIME *a, const MYSQL_TIME *b);
void localtime_to_TIME(MYSQL_TIME *to, struct tm *from);
void calc_time_from_sec(MYSQL_TIME *to, longlong seconds, long microseconds);
uint calc_week(MYSQL_TIME *l_time, uint week_behaviour, uint *year);

int calc_weekday(long daynr, bool sunday_first_day_of_week);

/* Character set-aware version of str_to_time() */
bool str_to_time(const CHARSET_INFO *cs, const char *str, size_t length,
                 MYSQL_TIME *l_time, my_time_flags_t flags,
                 MYSQL_TIME_STATUS *status);
static inline bool str_to_time(const String *str, MYSQL_TIME *ltime,
                               my_time_flags_t flags,
                               MYSQL_TIME_STATUS *status) {
  return str_to_time(str->charset(), str->ptr(), str->length(), ltime, flags,
                     status);
}

bool time_add_nanoseconds_with_round(MYSQL_TIME *ltime, uint nanoseconds,
                                     int *warnings);
/* Character set-aware version of str_to_datetime() */
bool str_to_datetime(const CHARSET_INFO *cs, const char *str, size_t length,
                     MYSQL_TIME *l_time, my_time_flags_t flags,
                     MYSQL_TIME_STATUS *status);
static inline bool str_to_datetime(const String *str, MYSQL_TIME *ltime,
                                   my_time_flags_t flags,
                                   MYSQL_TIME_STATUS *status) {
  return str_to_datetime(str->charset(), str->ptr(), str->length(), ltime,
                         flags, status);
}

bool datetime_add_nanoseconds_with_round(MYSQL_TIME *ltime, uint nanoseconds,
                                         int *warnings);

bool parse_date_time_format(timestamp_type format_type,
                            Date_time_format *date_time_format);

extern Known_date_time_format known_date_time_formats[];
extern LEX_STRING interval_type_to_name[];

/* Date/time rounding and truncation functions */
inline long my_time_fraction_remainder(long nr, uint decimals) {
  DBUG_ASSERT(decimals <= DATETIME_MAX_DECIMALS);
  return nr % (long)log_10_int[DATETIME_MAX_DECIMALS - decimals];
}
inline void my_time_trunc(MYSQL_TIME *ltime, uint decimals) {
  ltime->second_part -=
      my_time_fraction_remainder(ltime->second_part, decimals);
}
inline void my_datetime_trunc(MYSQL_TIME *ltime, uint decimals) {
  return my_time_trunc(ltime, decimals);
}
inline void my_timeval_trunc(struct timeval *tv, uint decimals) {
  tv->tv_usec -= my_time_fraction_remainder(tv->tv_usec, decimals);
}
bool my_time_adjust_frac(MYSQL_TIME *ltime, uint decimals, bool truncate);
bool my_datetime_adjust_frac(MYSQL_TIME *ltime, uint decimals, int *warnings,
                             bool truncate);

bool my_timeval_round(struct timeval *tv, uint decimals);
ulonglong TIME_to_ulonglong_datetime_round(const MYSQL_TIME *ltime);
ulonglong TIME_to_ulonglong_time_round(const MYSQL_TIME *ltime);

bool time_add_nanoseconds_with_truncate(MYSQL_TIME *ltime, uint nanoseconds,
                                        int *warnings);
bool datetime_add_nanoseconds_with_truncate(MYSQL_TIME *ltime,
                                            uint nanoseconds);
bool time_add_nanoseconds_adjust_frac(MYSQL_TIME *ltime, uint nanoseconds,
                                      int *warnings, bool truncate);
bool datetime_add_nanoseconds_adjust_frac(MYSQL_TIME *ltime, uint nanoseconds,
                                          int *warnings, bool truncate);

inline ulonglong TIME_to_ulonglong_round(const MYSQL_TIME *ltime) {
  switch (ltime->time_type) {
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

inline double TIME_microseconds(const MYSQL_TIME *ltime) {
  return (double)ltime->second_part / 1000000;
}

inline double TIME_to_double_datetime(const MYSQL_TIME *ltime) {
  return (double)TIME_to_ulonglong_datetime(ltime) + TIME_microseconds(ltime);
}

inline double TIME_to_double_time(const MYSQL_TIME *ltime) {
  return (double)TIME_to_ulonglong_time(ltime) + TIME_microseconds(ltime);
}

inline double TIME_to_double(const MYSQL_TIME *ltime) {
  return (double)TIME_to_ulonglong(ltime) + TIME_microseconds(ltime);
}

static inline bool check_fuzzy_date(const MYSQL_TIME *ltime,
                                    my_time_flags_t fuzzydate) {
  return !(fuzzydate & TIME_FUZZY_DATE) && (!ltime->month || !ltime->day);
}

static inline bool non_zero_date(const MYSQL_TIME *ltime) {
  return ltime->year || ltime->month || ltime->day;
}

static inline bool non_zero_time(const MYSQL_TIME *ltime) {
  return ltime->hour || ltime->minute || ltime->second || ltime->second_part;
}

longlong TIME_to_longlong_packed(const MYSQL_TIME *tm,
                                 enum enum_field_types type);
void TIME_from_longlong_packed(MYSQL_TIME *ltime, enum enum_field_types type,
                               longlong packed_value);
my_decimal *my_decimal_from_datetime_packed(my_decimal *dec,
                                            enum enum_field_types type,
                                            longlong packed_value);

longlong longlong_from_datetime_packed(enum enum_field_types type,
                                       longlong packed_value);

double double_from_datetime_packed(enum enum_field_types type,
                                   longlong packed_value);

static inline timestamp_type field_type_to_timestamp_type(
    enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TIME:
      return MYSQL_TIMESTAMP_TIME;
    case MYSQL_TYPE_DATE:
      return MYSQL_TIMESTAMP_DATE;
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
      return MYSQL_TIMESTAMP_DATETIME;
    default:
      return MYSQL_TIMESTAMP_NONE;
  }
}

/**
  This function gets GMT time and adds value of time_zone to get
  the local time. This function is used when server wants a timestamp
  value from dictionary system.
*/

ulonglong gmt_time_to_local_time(ulonglong time);

#endif /* SQL_TIME_INCLUDED */

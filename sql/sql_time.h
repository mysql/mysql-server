<<<<<<< HEAD
/* Copyright (c) 2006, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2006, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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

/**
  @ingroup SQL_TIME
  @{

  @file sql/sql_time.h

  Interface for server time utilities.
*/

#include "my_config.h"

#include <stddef.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>

#include "field_types.h"  // enum_field_types
#include "lex_string.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_time.h"
#include "mysql_time.h"    /* enum_mysql_timestamp_type */
#include "sql/sql_error.h" /* Sql_condition */
#include "sql_string.h"

class THD;
class Time_zone;
class my_decimal;

/**
   Representation of time formats.
 */
struct Date_time_format {
  uchar positions[8];
  LEX_CSTRING format;
};

/**
   Collection of strings describing date/time formats.
 */
struct Known_date_time_format {
  const char *format_name;
  const char *date_format;
  const char *datetime_format;
  const char *time_format;
};

my_time_t TIME_to_timestamp(const MYSQL_TIME *t, const Time_zone &tz,
                            bool *not_exist);
bool datetime_with_no_zero_in_date_to_timeval(const MYSQL_TIME *t,
                                              const Time_zone &tz,
                                              my_timeval *tm, int *warnings);
bool datetime_to_timeval(const MYSQL_TIME *t, const Time_zone &tz,
                         my_timeval *tm, int *warnings);
bool str_to_datetime_with_warn(String *str, MYSQL_TIME *l_time,
                               my_time_flags_t flags);
bool decimal_to_datetime(const my_decimal *decimal, MYSQL_TIME *ltime,
                         my_time_flags_t flags);
bool decimal_to_time(const my_decimal *decimal, MYSQL_TIME *ltime);
bool double_to_datetime(double nr, MYSQL_TIME *ltime, my_time_flags_t flags);
bool double_to_time(double nr, MYSQL_TIME *ltime);

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
void time_to_datetime(THD *thd, const MYSQL_TIME *tm, const Time_zone &tz,
                      MYSQL_TIME *dt);
void time_to_datetime(THD *thd, const MYSQL_TIME *in, MYSQL_TIME *out);

bool make_truncated_value_warning(THD *thd,
                                  Sql_condition::enum_severity_level level,
                                  const ErrConvString &val,
                                  enum_mysql_timestamp_type time_type,
                                  const char *field_name);

const char *get_date_time_format_str(const Known_date_time_format *format,
                                     enum_mysql_timestamp_type type);
void make_date(const Date_time_format *format, const MYSQL_TIME *l_time,
               String *str);
void make_time(const Date_time_format *format, const MYSQL_TIME *l_time,
               String *str, uint dec);
void make_datetime(const Date_time_format *format, const MYSQL_TIME *l_time,
                   String *str, uint dec);
bool my_TIME_to_str(const MYSQL_TIME *ltime, String *str, uint dec);

void propagate_datetime_overflow_helper(THD *thd, int *warnings);

/**
   Wrapper function which will propagate any DATETIME_OVERFLOW warnings
   to the THD. Overload for the case where a warnings bitset already
   exists which can be inspected before the value of the mysys
   function is passed on.

   @param thd Thread context
   @param warnings bitset used mysys function
   @param t value mysys function which is passed on
   @returns t
 */
template <class T>
inline T propagate_datetime_overflow(THD *thd, int *warnings, T t) {
  propagate_datetime_overflow_helper(thd, warnings);
  return t;
}

/**
   Wrapper function which will propagate any DATETIME_OVERFLOW warnings
   to the THD. Overload for the case when a warnings bitset must be
   created and inspected to call a mysys function. The mysys function
   must be invoked by the closure argument which invoked with a
   pointer to the warnings bitset as argument.

   @param thd Thread context
   @param clos closure to call with empty warnings bitset
   @return return value of closure
 */
template <class CLOS>
inline auto propagate_datetime_overflow(THD *thd, CLOS &&clos) {
  int warnings = 0;
  return propagate_datetime_overflow(thd, &warnings, clos(&warnings));
}

bool date_add_interval_with_warn(THD *, MYSQL_TIME *ltime,
                                 interval_type int_type, Interval interval);

bool str_to_time(const CHARSET_INFO *cs, const char *str, size_t length,
                 MYSQL_TIME *l_time, my_time_flags_t flags,
                 MYSQL_TIME_STATUS *status);

/**
   Converts a time String value to MYSQL_TIME. Forwards to the version
   taking an explicit charset, c-string and length.

   @return False on success, true on error.
 */
inline bool str_to_time(const String *str, MYSQL_TIME *ltime,
                        my_time_flags_t flags, MYSQL_TIME_STATUS *status) {
  return str_to_time(str->charset(), str->ptr(), str->length(), ltime, flags,
                     status);
}

bool str_to_datetime(const CHARSET_INFO *cs, const char *str, size_t length,
                     MYSQL_TIME *l_time, my_time_flags_t flags,
                     MYSQL_TIME_STATUS *status);

/**
   Converts a datetime String value to MYSQL_TIME. Forwards to the
   version taking an explicit charset, c-string and length.

   @return False on success, true on error.
*/
inline bool str_to_datetime(const String *str, MYSQL_TIME *ltime,
                            my_time_flags_t flags, MYSQL_TIME_STATUS *status) {
  return str_to_datetime(str->charset(), str->ptr(), str->length(), ltime,
                         flags, status);
}

extern const LEX_CSTRING interval_type_to_name[];

<<<<<<< HEAD
=======
bool parse_date_time_format(timestamp_type format_type,
                            Date_time_format *date_time_format);

extern Known_date_time_format known_date_time_formats[];
extern LEX_STRING interval_type_to_name[];

/* Date/time rounding and truncation functions */
<<<<<<< HEAD
inline long my_time_fraction_remainder(long nr, uint decimals) {
  DBUG_ASSERT(decimals <= DATETIME_MAX_DECIMALS);
  return nr % (long)log_10_int[DATETIME_MAX_DECIMALS - decimals];
=======
inline long my_time_fraction_remainder(long nr, uint decimals)
{
  assert(decimals <= DATETIME_MAX_DECIMALS);
  return nr % (long) log_10_int[DATETIME_MAX_DECIMALS - decimals];
>>>>>>> upstream/cluster-7.6
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

<<<<<<< HEAD
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
=======
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
    assert(0);
    return 0;
>>>>>>> upstream/cluster-7.6
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
>>>>>>> pr/231
my_decimal *my_decimal_from_datetime_packed(my_decimal *dec,
                                            enum enum_field_types type,
                                            longlong packed_value);

/**
   Return the timstamp value corresponding the field type passed as argument.

   @param type field type
   @return corresponding timestamp type
 */
inline enum_mysql_timestamp_type field_type_to_timestamp_type(
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

  @return time converted to local time
*/

ulonglong gmt_time_to_local_time(ulonglong time);

/**
  In lieu of a proper constructor for the C struct MYSQL_TIME, this method
  initializes the struct.
*/
MYSQL_TIME my_time_set(uint y, uint m, uint d, uint h, uint mi, uint s,
                       unsigned long ms, bool negative,
                       enum_mysql_timestamp_type type);
/**
  Return the number of significant second fraction decimals in ts, e.g.
  for ts->second_part == 120300, return 4.

  @param ts the time value for which we want the number of decmals
  @return the number of decimals
*/
uint actual_decimals(const MYSQL_TIME *ts);

/**
  For a time fraction with a given number of decimals, return maximum fraction,
  if any, can be are present in a time value. For example, if 2 decimals are
  specified, return 990000. If none, 0.

  @param decimals the number of decimals
  @return the maximum fraction
*/
size_t max_fraction(uint decimals);

/**
   @} (end of ingroup SQL_TIME)
*/

#endif /* SQL_TIME_INCLUDED */

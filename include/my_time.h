/* Copyright (C) 2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/*
  This is a private header of sql-common library, containing
  declarations for my_time.c
*/

#ifndef _my_time_h_
#define _my_time_h_
#include "my_global.h"
#include "mysql_time.h"

C_MODE_START

extern ulonglong log_10_int[20];
extern uchar days_in_month[];

/*
  Portable time_t replacement.
  Should be signed and hold seconds for 1902-2038 range.
*/
#if defined(_WIN64) || defined(WIN64)
/* on Win64 long is still 4 bytes (not 8!) */
typedef LONG64 my_time_t;
#else
typedef long my_time_t;
#endif

#define MY_TIME_T_MAX LONG_MAX
#define MY_TIME_T_MIN LONG_MIN

/* two-digit years < this are 20..; >= this are 19.. */
#define YY_PART_YEAR	   70
/* apply above magic to years < this */
#define YY_MAGIC_BELOW     200

/* Flags to str_to_datetime */
#define TIME_FUZZY_DATE		1
#define TIME_DATETIME_ONLY	2
/* Must be same as MODE_NO_ZERO_IN_DATE */
#define TIME_NO_ZERO_IN_DATE    (65536L*2*2*2*2*2*2*2)
/* Must be same as MODE_NO_ZERO_DATE */
#define TIME_NO_ZERO_DATE	(TIME_NO_ZERO_IN_DATE*2)
#define TIME_INVALID_DATES	(TIME_NO_ZERO_DATE*2)

#define MYSQL_TIME_WARN_TRUNCATED    1
#define MYSQL_TIME_WARN_OUT_OF_RANGE 2

/* Limits for the TIME data type */
#define TIME_MAX_HOUR 838
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59
#define TIME_MAX_VALUE (TIME_MAX_HOUR*10000 + TIME_MAX_MINUTE*100 + \
                        TIME_MAX_SECOND)

enum enum_mysql_timestamp_type
str_to_datetime(const char *str, uint length, MYSQL_TIME *l_time,
                uint flags, int *was_cut);
longlong number_to_datetime(longlong nr, MYSQL_TIME *time_res,
                            uint flags, int *was_cut);
ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *time);
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *time);
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *time);
ulonglong TIME_to_ulonglong(const MYSQL_TIME *time);


my_bool str_to_time(const char *str,uint length, MYSQL_TIME *l_time,
                    int *warning);

int check_time_range(struct st_mysql_time *time, int *warning);

long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);

void init_time(void);

my_time_t 
my_system_gmt_sec(const MYSQL_TIME *t, long *my_timezone,
                  my_bool *in_dst_time_gap);

void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type);

/*
  Required buffer length for my_time_to_str, my_date_to_str,
  my_datetime_to_str and TIME_to_string functions. Note, that the
  caller is still responsible to check that given TIME structure
  has values in valid ranges, otherwise size of the buffer could
  be not enough. We also rely on the fact that even wrong values
  sent using binary protocol fit in this buffer.
*/
#define MAX_DATE_STRING_REP_LENGTH 30

int my_time_to_str(const MYSQL_TIME *l_time, char *to);
int my_date_to_str(const MYSQL_TIME *l_time, char *to);
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to);
int my_TIME_to_str(const MYSQL_TIME *l_time, char *to);

/* 
  Available interval types used in any statement.

  'interval_type' must be sorted so that simple intervals comes first,
  ie year, quarter, month, week, day, hour, etc. The order based on
  interval size is also important and the intervals should be kept in a
  large to smaller order. (get_interval_value() depends on this)
 
  Note: If you change the order of elements in this enum you should fix 
  order of elements in 'interval_type_to_name' and 'interval_names' 
  arrays 
  
  See also interval_type_to_name, get_interval_value, interval_names
*/

enum interval_type
{
  INTERVAL_YEAR, INTERVAL_QUARTER, INTERVAL_MONTH, INTERVAL_WEEK, INTERVAL_DAY,
  INTERVAL_HOUR, INTERVAL_MINUTE, INTERVAL_SECOND, INTERVAL_MICROSECOND,
  INTERVAL_YEAR_MONTH, INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE,
  INTERVAL_DAY_SECOND, INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND,
  INTERVAL_MINUTE_SECOND, INTERVAL_DAY_MICROSECOND, INTERVAL_HOUR_MICROSECOND,
  INTERVAL_MINUTE_MICROSECOND, INTERVAL_SECOND_MICROSECOND, INTERVAL_LAST
};

C_MODE_END

#endif /* _my_time_h_ */

/* Copyright (c) 2004, 2014, Oracle and/or its affiliates. All rights reserved.

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
extern const char my_zero_datetime6[]; /* "0000-00-00 00:00:00.000000" */

/*
  Portable time_t replacement.
  Should be signed and hold seconds for 1902 -- 2038-01-19 range
  i.e at least a 32bit variable

  Using the system built in time_t is not an option as
  we rely on the above requirements in the time functions
*/
typedef long my_time_t;

#define MY_TIME_T_MAX LONG_MAX
#define MY_TIME_T_MIN LONG_MIN

#define DATETIME_MAX_DECIMALS 6

/* Time handling defaults */
#define TIMESTAMP_MAX_YEAR 2038
#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
#define TIMESTAMP_MAX_VALUE INT_MAX32
#define TIMESTAMP_MIN_VALUE 1

/* two-digit years < this are 20..; >= this are 19.. */
#define YY_PART_YEAR	   70

/*
  check for valid times only if the range of time_t is greater than
  the range of my_time_t
*/
#if SIZEOF_TIME_T > 4
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
    ((x) <= TIMESTAMP_MAX_VALUE && \
     (x) >= TIMESTAMP_MIN_VALUE)
#else
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
    ((x) >= TIMESTAMP_MIN_VALUE)
#endif

/* Flags to str_to_datetime and number_to_datetime */
typedef uint my_time_flags_t;
static const my_time_flags_t TIME_FUZZY_DATE=         1;
static const my_time_flags_t TIME_DATETIME_ONLY=      2;
static const my_time_flags_t TIME_NO_NSEC_ROUNDING=   4;
static const my_time_flags_t TIME_NO_DATE_FRAC_WARN=  8;
static const my_time_flags_t TIME_NO_ZERO_IN_DATE=   16;
static const my_time_flags_t TIME_NO_ZERO_DATE=      32;
static const my_time_flags_t TIME_INVALID_DATES=     64;

/* Conversion warnings */
#define MYSQL_TIME_WARN_TRUNCATED         1
#define MYSQL_TIME_WARN_OUT_OF_RANGE      2
#define MYSQL_TIME_WARN_INVALID_TIMESTAMP 4
#define MYSQL_TIME_WARN_ZERO_DATE         8
#define MYSQL_TIME_NOTE_TRUNCATED        16
#define MYSQL_TIME_WARN_ZERO_IN_DATE     32

/* Usefull constants */
#define SECONDS_IN_24H 86400L

/* Limits for the TIME data type */
#define TIME_MAX_HOUR 838
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59
#define TIME_MAX_VALUE (TIME_MAX_HOUR*10000 + TIME_MAX_MINUTE*100 + \
                        TIME_MAX_SECOND)
#define TIME_MAX_VALUE_SECONDS (TIME_MAX_HOUR * 3600L + \
                                TIME_MAX_MINUTE * 60L + TIME_MAX_SECOND)

/*
  Structure to return status from
    str_to_datetime(), str_to_time(), number_to_datetime(), number_to_time()
*/
typedef struct st_mysql_time_status
{
  int warnings;
  uint fractional_digits;
  uint nanoseconds;
} MYSQL_TIME_STATUS;

static inline void my_time_status_init(MYSQL_TIME_STATUS *status)
{
  status->warnings= status->fractional_digits= status->nanoseconds= 0;
}


my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   my_time_flags_t flags, int *was_cut);
my_bool str_to_datetime(const char *str, size_t length, MYSQL_TIME *l_time,
                        my_time_flags_t flags, MYSQL_TIME_STATUS *status);
longlong number_to_datetime(longlong nr, MYSQL_TIME *time_res,
                            my_time_flags_t flags, int *was_cut);
my_bool number_to_time(longlong nr, MYSQL_TIME *ltime, int *warnings);
ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong(const MYSQL_TIME *);

#define MY_PACKED_TIME_GET_INT_PART(x)     ((x) >> 24)
#define MY_PACKED_TIME_GET_FRAC_PART(x)    ((x) % (1LL << 24))
#define MY_PACKED_TIME_MAKE(i, f)          ((((longlong) (i)) << 24) + (f))
#define MY_PACKED_TIME_MAKE_INT(i)         ((((longlong) (i)) << 24))

longlong year_to_longlong_datetime_packed(long year);
longlong TIME_to_longlong_datetime_packed(const MYSQL_TIME *);
longlong TIME_to_longlong_date_packed(const MYSQL_TIME *);
longlong TIME_to_longlong_time_packed(const MYSQL_TIME *);
longlong TIME_to_longlong_packed(const MYSQL_TIME *);

void TIME_from_longlong_datetime_packed(MYSQL_TIME *ltime, longlong nr);
void TIME_from_longlong_time_packed(MYSQL_TIME *ltime, longlong nr);
void TIME_from_longlong_date_packed(MYSQL_TIME *ltime, longlong nr);
void TIME_set_yymmdd(MYSQL_TIME *ltime, uint yymmdd);
void TIME_set_hhmmss(MYSQL_TIME *ltime, uint hhmmss);

void my_datetime_packed_to_binary(longlong nr, uchar *ptr, uint dec);
longlong my_datetime_packed_from_binary(const uchar *ptr, uint dec);
uint my_datetime_binary_length(uint dec);

void my_time_packed_to_binary(longlong nr, uchar *ptr, uint dec);
longlong my_time_packed_from_binary(const uchar *ptr, uint dec);
uint my_time_binary_length(uint dec);

void my_timestamp_to_binary(const struct timeval *tm, uchar *ptr, uint dec);
void my_timestamp_from_binary(struct timeval *tm, const uchar *ptr, uint dec);
uint my_timestamp_binary_length(uint dec);

my_bool str_to_time(const char *str, size_t length, MYSQL_TIME *l_time,
                    MYSQL_TIME_STATUS *status);

my_bool check_time_mmssff_range(const MYSQL_TIME *ltime);
my_bool check_time_range_quick(const MYSQL_TIME *ltime);
my_bool check_datetime_range(const MYSQL_TIME *ltime);
void adjust_time_range(struct st_mysql_time *, int *warning);

long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);
uint year_2000_handling(uint year);

void my_init_time(void);


/*
  Function to check sanity of a TIMESTAMP value

  DESCRIPTION
    Check if a given MYSQL_TIME value fits in TIMESTAMP range.
    This function doesn't make precise check, but rather a rough
    estimate.

  RETURN VALUES
    TRUE    The value seems sane
    FALSE   The MYSQL_TIME value is definitely out of range
*/

static inline my_bool validate_timestamp_range(const MYSQL_TIME *t)
{
  if ((t->year > TIMESTAMP_MAX_YEAR || t->year < TIMESTAMP_MIN_YEAR) ||
      (t->year == TIMESTAMP_MAX_YEAR && (t->month > 1 || t->day > 19)) ||
      (t->year == TIMESTAMP_MIN_YEAR && (t->month < 12 || t->day < 31)))
    return FALSE;

  return TRUE;
}

my_time_t 
my_system_gmt_sec(const MYSQL_TIME *t, long *my_timezone,
                  my_bool *in_dst_time_gap);

void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type);
void set_max_time(MYSQL_TIME *tm, my_bool neg);
void set_max_hhmmss(MYSQL_TIME *tm);

/*
  Required buffer length for my_time_to_str, my_date_to_str,
  my_datetime_to_str and TIME_to_string functions. Note, that the
  caller is still responsible to check that given TIME structure
  has values in valid ranges, otherwise size of the buffer could
  be not enough. We also rely on the fact that even wrong values
  sent using binary protocol fit in this buffer.
*/
#define MAX_DATE_STRING_REP_LENGTH 30

int my_time_to_str(const MYSQL_TIME *l_time, char *to, uint dec);
int my_date_to_str(const MYSQL_TIME *l_time, char *to);
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to, uint dec);
int my_TIME_to_str(const MYSQL_TIME *l_time, char *to, uint dec);

int my_timeval_to_str(const struct timeval *tm, char *to, uint dec);

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

/*
   Copyright (c) 2004, 2011, Oracle and/or its affiliates.

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

/* Time handling defaults */
#define TIMESTAMP_MAX_YEAR 2038
#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
#define TIMESTAMP_MAX_VALUE INT_MAX32
#define TIMESTAMP_MIN_VALUE 0

/* two-digit years < this are 20..; >= this are 19.. */
#define YY_PART_YEAR	   70

/*
  check for valid times only if the range of time_t is greater than
  the range of my_time_t
*/
#if SIZEOF_TIME_T > 4 || defined(TIME_T_UNSIGNED)
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
    ((x) <= TIMESTAMP_MAX_VALUE && \
     (x) >= TIMESTAMP_MIN_VALUE)
#else
# define IS_TIME_T_VALID_FOR_TIMESTAMP(x) \
    ((x) >= TIMESTAMP_MIN_VALUE)
#endif

/* Flags to str_to_datetime */

/*
  TIME_FUZZY_DATES is used for the result will only be used for comparison
  purposes. Conversion is as relaxed as possible.
*/
#define TIME_FUZZY_DATES        1
#define TIME_DATETIME_ONLY	2
#define TIME_TIME_ONLY	        4
#define TIME_NO_ZERO_IN_DATE    (1UL << 23) /* == MODE_NO_ZERO_IN_DATE */
#define TIME_NO_ZERO_DATE	(1UL << 24) /* == MODE_NO_ZERO_DATE    */
#define TIME_INVALID_DATES	(1UL << 25) /* == MODE_INVALID_DATES   */

#define MYSQL_TIME_WARN_TRUNCATED    1
#define MYSQL_TIME_WARN_OUT_OF_RANGE 2
#define MYSQL_TIME_NOTE_TRUNCATED    16

#define MYSQL_TIME_WARN_WARNINGS (MYSQL_TIME_WARN_TRUNCATED|MYSQL_TIME_WARN_OUT_OF_RANGE)
#define MYSQL_TIME_WARN_NOTES    (MYSQL_TIME_NOTE_TRUNCATED)

#define MYSQL_TIME_WARN_HAVE_WARNINGS(x) MY_TEST((x) & MYSQL_TIME_WARN_WARNINGS)
#define MYSQL_TIME_WARN_HAVE_NOTES(x) MY_TEST((x) & MYSQL_TIME_WARN_NOTES)

/* Limits for the TIME data type */
#define TIME_MAX_HOUR 838
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59
#define TIME_MAX_SECOND_PART 999999
#define TIME_SECOND_PART_FACTOR (TIME_MAX_SECOND_PART+1)
#define TIME_SECOND_PART_DIGITS 6
#define TIME_MAX_VALUE (TIME_MAX_HOUR*10000 + TIME_MAX_MINUTE*100 + TIME_MAX_SECOND)
#define TIME_MAX_VALUE_SECONDS (TIME_MAX_HOUR * 3600L + \
                                TIME_MAX_MINUTE * 60L + TIME_MAX_SECOND)

my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   ulonglong flags, int *was_cut);
enum enum_mysql_timestamp_type
str_to_time(const char *str, uint length, MYSQL_TIME *l_time, 
            ulonglong flag, int *warning);
enum enum_mysql_timestamp_type
str_to_datetime(const char *str, uint length, MYSQL_TIME *l_time,
                ulonglong flags, int *was_cut);
longlong number_to_datetime(longlong nr, ulong sec_part, MYSQL_TIME *time_res,
                            ulonglong flags, int *was_cut);

static inline
longlong double_to_datetime(double nr, MYSQL_TIME *ltime, uint flags, int *cut)
{
  if (nr < 0 || nr > LONGLONG_MAX)
    nr= (double)LONGLONG_MAX;
  return number_to_datetime((longlong) floor(nr),
                            (ulong)((nr-floor(nr))*TIME_SECOND_PART_FACTOR),
                            ltime, flags, cut);
}

int number_to_time(my_bool neg, ulonglong nr, ulong sec_part,
                   MYSQL_TIME *ltime, int *was_cut);
ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong(const MYSQL_TIME *);
double TIME_to_double(const MYSQL_TIME *my_time);

longlong pack_time(MYSQL_TIME *my_time);
MYSQL_TIME *unpack_time(longlong packed, MYSQL_TIME *my_time);

int check_time_range(struct st_mysql_time *my_time, uint dec, int *warning);

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
    FALSE   The value seems sane
    TRUE    The MYSQL_TIME value is definitely out of range
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
my_system_gmt_sec(const MYSQL_TIME *t, long *my_timezone, uint *error_code);

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
#define AUTO_SEC_PART_DIGITS 31 /* same as NOT_FIXED_DEC */

int my_time_to_str(const MYSQL_TIME *l_time, char *to, uint digits);
int my_date_to_str(const MYSQL_TIME *l_time, char *to);
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to, uint digits);
int my_TIME_to_str(const MYSQL_TIME *l_time, char *to, uint digits);

static inline longlong sec_part_shift(longlong second_part, uint digits)
{
  return second_part / (longlong)log_10_int[TIME_SECOND_PART_DIGITS - digits];
}
static inline longlong sec_part_unshift(longlong second_part, uint digits)
{
  return second_part * (longlong)log_10_int[TIME_SECOND_PART_DIGITS - digits];
}
static inline ulong sec_part_truncate(ulong second_part, uint digits)
{
  /* the cast here should be unnecessary! */
  return second_part - second_part % (ulong)log_10_int[TIME_SECOND_PART_DIGITS - digits];
}

#define hrtime_to_my_time(X) ((my_time_t)hrtime_to_time(X))

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

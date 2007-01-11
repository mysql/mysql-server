/* Copyright (C) 2004-2005 MySQL AB

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
typedef long my_time_t;

#define MY_TIME_T_MAX LONG_MAX
#define MY_TIME_T_MIN LONG_MIN


/* Time handling defaults */
#define TIMESTAMP_MAX_YEAR 2038
#define YY_PART_YEAR	   70
#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
#define TIMESTAMP_MAX_VALUE INT_MAX32
#define TIMESTAMP_MIN_VALUE 1

#define YY_PART_YEAR	   70

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

my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   ulong flags, int *was_cut);
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

static inline bool validate_timestamp_range(const MYSQL_TIME *t)
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

C_MODE_END

#endif /* _my_time_h_ */

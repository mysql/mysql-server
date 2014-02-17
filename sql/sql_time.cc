/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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


/* Functions to handle date and time */

#include "sql_priv.h"
#include "unireg.h"                      // REQUIRED by other includes
#include "sql_time.h"
#include "tztime.h"                             // struct Time_zone
#include "sql_class.h"   // THD, MODE_INVALID_DATES, MODE_NO_ZERO_DATE
#include <m_ctype.h>


	/* Some functions to calculate dates */

#ifndef TESTTIME

/*
  Name description of interval names used in statements.

  'interval_type_to_name' is ordered and sorted on interval size and
  interval complexity.
  Order of elements in 'interval_type_to_name' should correspond to 
  the order of elements in 'interval_type' enum
  
  See also interval_type, interval_names
*/

LEX_STRING interval_type_to_name[INTERVAL_LAST] = {
  { C_STRING_WITH_LEN("YEAR")},
  { C_STRING_WITH_LEN("QUARTER")},
  { C_STRING_WITH_LEN("MONTH")},
  { C_STRING_WITH_LEN("WEEK")},
  { C_STRING_WITH_LEN("DAY")},
  { C_STRING_WITH_LEN("HOUR")},
  { C_STRING_WITH_LEN("MINUTE")},
  { C_STRING_WITH_LEN("SECOND")},
  { C_STRING_WITH_LEN("MICROSECOND")},
  { C_STRING_WITH_LEN("YEAR_MONTH")},
  { C_STRING_WITH_LEN("DAY_HOUR")},
  { C_STRING_WITH_LEN("DAY_MINUTE")},
  { C_STRING_WITH_LEN("DAY_SECOND")},
  { C_STRING_WITH_LEN("HOUR_MINUTE")},
  { C_STRING_WITH_LEN("HOUR_SECOND")},
  { C_STRING_WITH_LEN("MINUTE_SECOND")},
  { C_STRING_WITH_LEN("DAY_MICROSECOND")},
  { C_STRING_WITH_LEN("HOUR_MICROSECOND")},
  { C_STRING_WITH_LEN("MINUTE_MICROSECOND")},
  { C_STRING_WITH_LEN("SECOND_MICROSECOND")}
}; 

	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  DBUG_ENTER("calc_weekday");
  DBUG_RETURN ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}

/*
  The bits in week_format has the following meaning:
   WEEK_MONDAY_FIRST (0)  If not set	Sunday is first day of week
      		   	  If set	Monday is first day of week
   WEEK_YEAR (1)	  If not set	Week is in range 0-53

   	Week 0 is returned for the the last week of the previous year (for
	a date at start of january) In this case one can get 53 for the
	first week of next year.  This flag ensures that the week is
	relevant for the given year. Note that this flag is only
	releveant if WEEK_JANUARY is not set.

			  If set	 Week is in range 1-53.

	In this case one may get week 53 for a date in January (when
	the week is that last week of previous year) and week 1 for a
	date in December.

  WEEK_FIRST_WEEKDAY (2)  If not set	Weeks are numbered according
			   		to ISO 8601:1988
			  If set	The week that contains the first
					'first-day-of-week' is week 1.
	
	ISO 8601:1988 means that if the week containing January 1 has
	four or more days in the new year, then it is week 1;
	Otherwise it is the last week of the previous year, and the
	next week is week 1.
*/

uint calc_week(MYSQL_TIME *l_time, uint week_behaviour, uint *year)
{
  uint days;
  ulong daynr=calc_daynr(l_time->year,l_time->month,l_time->day);
  ulong first_daynr=calc_daynr(l_time->year,1,1);
  bool monday_first= MY_TEST(week_behaviour & WEEK_MONDAY_FIRST);
  bool week_year= MY_TEST(week_behaviour & WEEK_YEAR);
  bool first_weekday= MY_TEST(week_behaviour & WEEK_FIRST_WEEKDAY);

  uint weekday=calc_weekday(first_daynr, !monday_first);
  *year=l_time->year;

  if (l_time->month == 1 && l_time->day <= 7-weekday)
  {
    if (!week_year && 
	((first_weekday && weekday != 0) ||
	 (!first_weekday && weekday >= 4)))
      return 0;
    week_year= 1;
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }

  if ((first_weekday && weekday != 0) ||
      (!first_weekday && weekday >= 4))
    days= daynr - (first_daynr+ (7-weekday));
  else
    days= daynr - (first_daynr - weekday);

  if (week_year && days >= 52*7)
  {
    weekday= (weekday + calc_days_in_year(*year)) % 7;
    if ((!first_weekday && weekday < 4) ||
	(first_weekday && weekday == 0))
    {
      (*year)++;
      return 1;
    }
  }
  return days/7+1;
}

	/* Change a daynr to year, month and day */
	/* Daynr 0 is returned as date 00.00.00 */

void get_date_from_daynr(long daynr,uint *ret_year,uint *ret_month,
			 uint *ret_day)
{
  uint year,temp,leap_day,day_of_year,days_in_year;
  uchar *month_pos;
  DBUG_ENTER("get_date_from_daynr");

  if (daynr <= 365L || daynr >= 3652500)
  {						/* Fix if wrong daynr */
    *ret_year= *ret_month = *ret_day =0;
  }
  else
  {
    year= (uint) (daynr*100 / 36525L);
    temp=(((year-1)/100+1)*3)/4;
    day_of_year=(uint) (daynr - (long) year * 365L) - (year-1)/4 +temp;
    while (day_of_year > (days_in_year= calc_days_in_year(year)))
    {
      day_of_year-=days_in_year;
      (year)++;
    }
    leap_day=0;
    if (days_in_year == 366)
    {
      if (day_of_year > 31+28)
      {
	day_of_year--;
	if (day_of_year == 31+28)
	  leap_day=1;		/* Handle leapyears leapday */
      }
    }
    *ret_month=1;
    for (month_pos= days_in_month ;
	 day_of_year > (uint) *month_pos ;
	 day_of_year-= *(month_pos++), (*ret_month)++)
      ;
    *ret_year=year;
    *ret_day=day_of_year+leap_day;
  }
  DBUG_VOID_RETURN;
}

	/* Functions to handle periods */

ulong convert_period_to_month(ulong period)
{
  ulong a,b;
  if (period == 0)
    return 0L;
  if ((a=period/100) < YY_PART_YEAR)
    a+=2000;
  else if (a < 100)
    a+=1900;
  b=period%100;
  return a*12+b-1;
}


ulong convert_month_to_period(ulong month)
{
  ulong year;
  if (month == 0L)
    return 0L;
  if ((year=month/12) < 100)
  {
    year+=(year < YY_PART_YEAR) ? 2000 : 1900;
  }
  return year*100+month%12+1;
}


/*
  Convert a string to 8-bit representation,
  for use in str_to_time/str_to_date/str_to_date.
  
  In the future to_ascii() can be extended to convert
  non-ASCII digits to ASCII digits
  (for example, ARABIC-INDIC, DEVANAGARI, BENGALI, and so on)
  so DATE/TIME/DATETIME values understand digits in the
  respected scripts.
*/
static uint
to_ascii(const CHARSET_INFO *cs,
         const char *src, uint src_length,
         char *dst, uint dst_length)
                     
{
  int cnvres;
  my_wc_t wc;
  const char *srcend= src + src_length;
  char *dst0= dst, *dstend= dst + dst_length - 1;
  while (dst < dstend &&
         (cnvres= (cs->cset->mb_wc)(cs, &wc,
                                    (const uchar*) src,
                                    (const uchar*) srcend)) > 0 &&
         wc < 128)
  {
    src+= cnvres;
    *dst++= wc;
  }
  *dst= '\0';
  return dst - dst0;
}


/* Character set-aware version of str_to_time() */
bool str_to_time(const CHARSET_INFO *cs, const char *str,uint length,
                 MYSQL_TIME *l_time, uint flags, MYSQL_TIME_STATUS *status)
{
  char cnv[MAX_TIME_FULL_WIDTH + 3]; // +3 for nanoseconds (for rounding)
  if ((cs->state & MY_CS_NONASCII) != 0)
  {
    length= to_ascii(cs, str, length, cnv, sizeof(cnv));
    str= cnv;
  }
  return str_to_time(str, length, l_time, status) ||
         (!(flags & TIME_NO_NSEC_ROUNDING) &&
          time_add_nanoseconds_with_round(l_time, status->nanoseconds,
                                          &status->warnings));
}


/* Character set-aware version of str_to_datetime() */
bool str_to_datetime(const CHARSET_INFO *cs,
                     const char *str, uint length,
                     MYSQL_TIME *l_time, uint flags,
                     MYSQL_TIME_STATUS *status)
{
  char cnv[MAX_DATETIME_FULL_WIDTH + 3]; // +3 for nanoseconds (for rounding)
  if ((cs->state & MY_CS_NONASCII) != 0)
  {
    length= to_ascii(cs, str, length, cnv, sizeof(cnv));
    str= cnv;
  }
  return str_to_datetime(str, length, l_time, flags, status) ||
         (!(flags & TIME_NO_NSEC_ROUNDING) &&
          datetime_add_nanoseconds_with_round(l_time,
                                              status->nanoseconds,
                                              &status->warnings));
}


/**
  Add nanoseconds to a time value with rounding.

  @param IN/OUT ltime       MYSQL_TIME variable to add to.
  @param        nanosecons  Nanosecons value.
  @param IN/OUT warnings    Warning flag vector.
  @retval                   False on success, true on error.
*/
bool time_add_nanoseconds_with_round(MYSQL_TIME *ltime,
                                     uint nanoseconds, int *warnings)
{
  /* We expect correct input data */
  DBUG_ASSERT(nanoseconds < 1000000000);
  DBUG_ASSERT(!check_time_mmssff_range(ltime));

  if (nanoseconds < 500)
    return false;

  ltime->second_part+= (nanoseconds + 500) / 1000;
  if (ltime->second_part < 1000000)
    goto ret;

  ltime->second_part%= 1000000;
  if (ltime->second < 59)
  {
    ltime->second++;
    goto ret;
  }

  ltime->second= 0;
  if (ltime->minute < 59)
  {
    ltime->minute++;
    goto ret;
  }
  ltime->minute= 0;
  ltime->hour++;

ret:
  /*
    We can get '838:59:59.000001' at this point, which
    is bigger than the maximum possible value '838:59:59.000000'.
    Checking only "hour > 838" is not enough.
    Do full adjust_time_range().
  */
  adjust_time_range(ltime, warnings);
  return false;
}


/**
  Add nanoseconds to a datetime value with rounding.

  @param IN/OUT ltime       MYSQL_TIME variable to add to.
  @param        nanosecons  Nanosecons value.
  @param IN/OUT warnings    Warning flag vector.
  @retval                   False on success, true on error.
*/
bool datetime_add_nanoseconds_with_round(MYSQL_TIME *ltime,
                                         uint nanoseconds, int *warnings)
{
  DBUG_ASSERT(nanoseconds < 1000000000);
  if (nanoseconds < 500)
    return false;

  ltime->second_part+= (nanoseconds + 500) / 1000;
  if (ltime->second_part < 1000000)
    return false;

  ltime->second_part%= 1000000;
  INTERVAL interval;
  memset(&interval, 0, sizeof(interval));
  interval.second= 1;
  /* date_add_interval cannot handle bad dates */
  if (check_date(ltime, non_zero_date(ltime),
                 (TIME_NO_ZERO_IN_DATE | TIME_NO_ZERO_DATE), warnings))
    return true;

  if (date_add_interval(ltime, INTERVAL_SECOND, interval))
  {
    *warnings|= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  return false;
}


/*
  Convert a timestamp string to a MYSQL_TIME value and produce a warning 
  if string was truncated during conversion.

  NOTE
    See description of str_to_datetime() for more information.
*/
bool
str_to_datetime_with_warn(String *str, MYSQL_TIME *l_time, uint flags)
{
  MYSQL_TIME_STATUS status;
  THD *thd= current_thd;
  bool ret_val= str_to_datetime(str, l_time,
                                (flags | (thd->variables.sql_mode &
                                 (MODE_INVALID_DATES | MODE_NO_ZERO_DATE))),
                                &status);
  if (ret_val || status.warnings)
    make_truncated_value_warning(ErrConvString(str), l_time->time_type);
  return ret_val;
}


/*
  Convert lldiv_t to datetime.

  @param        lld      The value to convert from.
  @param OUT    ltime    The variable to convert to.
  @param        flags    Conversion flags.
  @param IN/OUT warnings Warning flags.
  @return                False on success, true on error.
*/
static bool
lldiv_t_to_datetime(lldiv_t lld, MYSQL_TIME *ltime, uint flags, int *warnings)
{
  if (lld.rem < 0 || // Catch negative numbers with zero int part, e.g: -0.1
      number_to_datetime(lld.quot, ltime, flags, warnings) == LL(-1))
  {
    /* number_to_datetime does not clear ltime in case of ZERO DATE */
    set_zero_time(ltime, MYSQL_TIMESTAMP_ERROR);
    if (!*warnings) /* Neither sets warnings in case of ZERO DATE */
      *warnings|= MYSQL_TIME_WARN_TRUNCATED;
    return true;
  }
  else if (ltime->time_type == MYSQL_TIMESTAMP_DATE)
  {
    /*
      Generate a warning in case of DATE with fractional part:
        20011231.1234 -> '2001-12-31'
      unless the caller does not want the warning: for example, CAST does.
    */
    if (lld.rem && !(flags & TIME_NO_DATE_FRAC_WARN))
      *warnings|= MYSQL_TIME_WARN_TRUNCATED;
  }
  else if (!(flags & TIME_NO_NSEC_ROUNDING))
  {
    ltime->second_part= lld.rem / 1000;
    return datetime_add_nanoseconds_with_round(ltime, lld.rem % 1000, warnings);
  }
  return false;
}


/**
  Convert decimal value to datetime value with a warning.
  @param       decimal The value to convert from.
  @param OUT   ltime   The variable to convert to.
  @param       flags   Conversion flags.
  @return              False on success, true on error.
*/
bool
my_decimal_to_datetime_with_warn(const my_decimal *decimal,
                                 MYSQL_TIME *ltime, uint flags)
{
  lldiv_t lld;
  int warnings= 0;
  bool rc;

  if ((rc= my_decimal2lldiv_t(0, decimal, &lld)))
  {
    warnings|= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_NONE);
  }
  else
    rc= lldiv_t_to_datetime(lld, ltime, flags, &warnings);

  if (warnings)
    make_truncated_value_warning(ErrConvString(decimal), ltime->time_type);
  return rc;
}


/**
  Convert double value to datetime value with a warning.
  @param       nr      The value to convert from.
  @param OUT   ltime   The variable to convert to.
  @param       flags   Conversion flags.
  @return              False on success, true on error.
*/
bool
my_double_to_datetime_with_warn(double nr, MYSQL_TIME *ltime, uint flags)
{
  lldiv_t lld;
  int warnings= 0;
  bool rc;

  if ((rc= (double2lldiv_t(nr, &lld) != E_DEC_OK)))
  {
    warnings|= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_NONE);
  }
  else
    rc= lldiv_t_to_datetime(lld, ltime, flags, &warnings);

  if (warnings)
    make_truncated_value_warning(ErrConvString(nr), ltime->time_type);  
  return rc;
}


/**
  Convert longlong value to datetime value with a warning.
  @param       nr      The value to convert from.
  @param OUT   ltime   The variable to convert to.
  @return              False on success, true on error.
*/
bool
my_longlong_to_datetime_with_warn(longlong nr, MYSQL_TIME *ltime, uint flags)
{
  int warnings= 0;
  bool rc= number_to_datetime(nr, ltime, flags, &warnings) == LL(-1);
  if (warnings)
    make_truncated_value_warning(ErrConvString(nr),  MYSQL_TIMESTAMP_NONE);
  return rc;
}


/*
  Convert lldiv_t value to time with nanosecond rounding.

  @param        lld      The value to convert from.
  @param OUT    ltime    The variable to convert to,
  @param        flags    Conversion flags.
  @param IN/OUT warnings Warning flags.
  @return                False on success, true on error.
*/
static bool lldiv_t_to_time(lldiv_t lld, MYSQL_TIME *ltime, int *warnings)
{
  if (number_to_time(lld.quot, ltime, warnings))
    return true;
  /*
    Both lld.quot and lld.rem can give negative result value,
    thus combine them using "|=".
  */
  if ((ltime->neg|= (lld.rem < 0)))
    lld.rem= -lld.rem;
  ltime->second_part= lld.rem / 1000;
  return time_add_nanoseconds_with_round(ltime, lld.rem % 1000, warnings);
}


/*
  Convert decimal number to TIME
  @param     decimal_value  The number to convert from.
  @param OUT ltime          The variable to convert to.
  @param     flags          Conversion flags.
  @return    False on success, true on error.
*/
bool my_decimal_to_time_with_warn(const my_decimal *decimal, MYSQL_TIME *ltime)
{
  lldiv_t lld;
  int warnings= 0;
  bool rc;

  if ((rc= my_decimal2lldiv_t(0, decimal, &lld)))
  {
    warnings|= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  }
  else 
    rc= lldiv_t_to_time(lld, ltime, &warnings);

  if (warnings)
    make_truncated_value_warning(ErrConvString(decimal), MYSQL_TIMESTAMP_TIME);
  return rc;
}


/*
  Convert double number to TIME

  @param     nr      The number to convert from.
  @param OUT ltime   The variable to convert to.
  @param     flags   Conversion flags.
  @return    False on success, true on error.
*/
bool my_double_to_time_with_warn(double nr, MYSQL_TIME *ltime)
{
  lldiv_t lld;
  int warnings= 0;
  bool rc;

  if ((rc= (double2lldiv_t(nr, &lld) != E_DEC_OK)))
  {
    warnings|= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  }
  else
    rc= lldiv_t_to_time(lld, ltime, &warnings);

  if (warnings)
    make_truncated_value_warning(ErrConvString(nr), MYSQL_TIMESTAMP_TIME);
  return rc;
}



/*
  Convert longlong number to TIME
  @param     nr     The number to convert from.
  @param OUT ltime  The variable to convert to.
  @param     flags  Conversion flags.
  @return    False on success, true on error.
*/
bool my_longlong_to_time_with_warn(longlong nr, MYSQL_TIME *ltime)
{
  int warnings= 0;
  bool rc= number_to_time(nr, ltime, &warnings);
  if (warnings)
    make_truncated_value_warning(ErrConvString(nr), MYSQL_TIMESTAMP_TIME);
  return rc;
}


/**
  Convert a datetime from broken-down MYSQL_TIME representation
  to corresponding TIMESTAMP value.

  @param  thd             - current thread
  @param  t               - datetime in broken-down representation, 
  @param  in_dst_time_gap - pointer to bool which is set to true if t represents
                            value which doesn't exists (falls into the spring 
                            time-gap) or to false otherwise.
  @return
  @retval  Number seconds in UTC since start of Unix Epoch corresponding to t.
  @retval  0 - t contains datetime value which is out of TIMESTAMP range.     
*/
my_time_t TIME_to_timestamp(THD *thd, const MYSQL_TIME *t, my_bool *in_dst_time_gap)
{
  my_time_t timestamp;

  *in_dst_time_gap= 0;

  timestamp= thd->time_zone()->TIME_to_gmt_sec(t, in_dst_time_gap);
  if (timestamp)
  {
    return timestamp;
  }

  /* If we are here we have range error. */
  return(0);
}


/**
  Convert a datetime MYSQL_TIME representation
  to corresponding "struct timeval" value.

  ltime must previously be checked for TIME_NO_ZERO_IN_DATE.
  Things like '0000-01-01', '2000-00-01', '2000-01-00' are not allowed
  and asserted.

  Things like '0000-00-00 10:30:30' or '0000-00-00 00:00:00.123456'
  (i.e. empty date with non-empty time) return error.

  Zero datetime '0000-00-00 00:00:00.000000'
  is allowed and is mapper to {tv_sec=0, tv_usec=0}.

  Note: In case of error, tm value is not initialized.

  Note: "warnings" is not initialized to zero,
  so new warnings are added to the old ones.
  Caller must make sure to initialize "warnings".

  @param IN  thd       current thd
  @param IN  ltime     datetime value
  @param OUT tm        timeval value
  @param OUT warnings  pointer to warnings vector
  @return
  @retval      false on success
  @retval      true on error
*/
bool datetime_with_no_zero_in_date_to_timeval(THD *thd,
                                              const MYSQL_TIME *ltime,
                                              struct timeval *tm,
                                              int *warnings)
{
  if (!ltime->month) /* Zero date */
  {
    DBUG_ASSERT(!ltime->year && !ltime->day);
    if (non_zero_time(ltime))
    {
      /*
        Return error for zero date with non-zero time, e.g.:
        '0000-00-00 10:20:30' or '0000-00-00 00:00:00.123456'
      */
      *warnings|= MYSQL_TIME_WARN_TRUNCATED;
      return true;
    }
    tm->tv_sec= tm->tv_usec= 0; // '0000-00-00 00:00:00.000000'
    return false;
  }

  my_bool in_dst_time_gap;
  if (!(tm->tv_sec= TIME_to_timestamp(current_thd, ltime, &in_dst_time_gap)))
  {
    /*
      Date was outside of the supported timestamp range.
      For example: '3001-01-01 00:00:00' or '1000-01-01 00:00:00'
    */
    *warnings|= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  else if (in_dst_time_gap)
  {
    /*
      Set MYSQL_TIME_WARN_INVALID_TIMESTAMP warning to indicate
      that date was fine but pointed to winter/summer time switch gap.
      In this case tm is set to the fist second after gap.
      For example: '2003-03-30 02:30:00 MSK' -> '2003-03-30 03:00:00 MSK'
    */
    *warnings|= MYSQL_TIME_WARN_INVALID_TIMESTAMP;
  }
  tm->tv_usec= ltime->second_part;
  return false;
}


/**
  Convert a datetime MYSQL_TIME representation
  to corresponding "struct timeval" value.

  Things like '0000-01-01', '2000-00-01', '2000-01-00'
  (i.e. incomplete date) return error.

  Things like '0000-00-00 10:30:30' or '0000-00-00 00:00:00.123456'
  (i.e. empty date with non-empty time) return error.

  Zero datetime '0000-00-00 00:00:00.000000'
  is allowed and is mapper to {tv_sec=0, tv_usec=0}.

  Note: In case of error, tm value is not initialized.

  Note: "warnings" is not initialized to zero,
  so new warnings are added to the old ones.
  Caller must make sure to initialize "warnings".

  @param IN  thd       current thd
  @param IN  ltime     datetime value
  @param OUT tm        timeval value
  @param OUT warnings  pointer to warnings vector
  @return
  @retval      false on success
  @retval      true on error
*/
bool datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                         struct timeval *tm, int *warnings)
{
  return
    check_date(ltime, non_zero_date(ltime), TIME_NO_ZERO_IN_DATE, warnings) ||
    datetime_with_no_zero_in_date_to_timeval(current_thd, ltime, tm, warnings);
}


/*
  Convert a time string to a MYSQL_TIME struct and produce a warning
  if string was cut during conversion.

  NOTE
    See str_to_time() for more info.
*/
bool
str_to_time_with_warn(String *str, MYSQL_TIME *l_time)
{
  MYSQL_TIME_STATUS status;
  bool ret_val= str_to_time(str, l_time, 0, &status);
  if (ret_val || status.warnings)
    make_truncated_value_warning(ErrConvString(str), MYSQL_TIMESTAMP_TIME);
  return ret_val;
}


/**
  Convert time to datetime.

  The time value is added to the current datetime value.
  @param  IN  ltime    Time value to convert from.
  @param  OUT ltime2   Datetime value to convert to.
*/
void time_to_datetime(THD *thd, const MYSQL_TIME *ltime, MYSQL_TIME *ltime2)
{
  thd->variables.time_zone->gmt_sec_to_TIME(ltime2, thd->query_start());
  ltime2->hour= ltime2->minute= ltime2->second= ltime2->second_part= 0;
  ltime2->time_type= MYSQL_TIMESTAMP_DATE;
  mix_date_and_time(ltime2, ltime);
}


/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(MYSQL_TIME *to, struct tm *from)
{
  to->neg=0;
  to->second_part=0;
  to->year=	(int) ((from->tm_year+1900) % 10000);
  to->month=	(int) from->tm_mon+1;
  to->day=	(int) from->tm_mday;
  to->hour=	(int) from->tm_hour;
  to->minute=	(int) from->tm_min;
  to->second=   (int) from->tm_sec;
}

void calc_time_from_sec(MYSQL_TIME *to, longlong seconds, long microseconds)
{
  long t_seconds;
  // to->neg is not cleared, it may already be set to a useful value
  to->time_type= MYSQL_TIMESTAMP_TIME;
  to->year= 0;
  to->month= 0;
  to->day= 0;
  DBUG_ASSERT(seconds < (0xFFFFFFFFLL * 3600LL));
  to->hour=  (long) (seconds / 3600L);
  t_seconds= (long) (seconds % 3600L);
  to->minute= t_seconds/60L;
  to->second= t_seconds%60L;
  to->second_part= microseconds;
}


/*
  Parse a format string specification

  SYNOPSIS
    parse_date_time_format()
    format_type		Format of string (time, date or datetime)
    format_str		String to parse
    format_length	Length of string
    date_time_format	Format to fill in

  NOTES
    Fills in date_time_format->positions for all date time parts.

    positions marks the position for a datetime element in the format string.
    The position array elements are in the following order:
    YYYY-DD-MM HH-MM-DD.FFFFFF AM
    0    1  2  3  4  5  6      7

    If positions[0]= 5, it means that year will be the forth element to
    read from the parsed date string.

  RETURN
    0	ok
    1	error
*/

bool parse_date_time_format(timestamp_type format_type, 
			    const char *format, uint format_length,
			    DATE_TIME_FORMAT *date_time_format)
{
  uint offset= 0, separators= 0;
  const char *ptr= format, *format_str;
  const char *end= ptr+format_length;
  uchar *dt_pos= date_time_format->positions;
  /* need_p is set if we are using AM/PM format */
  bool need_p= 0, allow_separator= 0;
  ulong part_map= 0, separator_map= 0;
  const char *parts[16];

  date_time_format->time_separator= 0;
  date_time_format->flag= 0;			// For future

  /*
    Fill position with 'dummy' arguments to found out if a format tag is
    used twice (This limit's the format to 255 characters, but this is ok)
  */
  dt_pos[0]= dt_pos[1]= dt_pos[2]= dt_pos[3]=
    dt_pos[4]= dt_pos[5]= dt_pos[6]= dt_pos[7]= 255;

  for (; ptr != end; ptr++)
  {
    if (*ptr == '%' && ptr+1 != end)
    {
      uint position;
      LINT_INIT(position);
      switch (*++ptr) {
      case 'y':					// Year
      case 'Y':
	position= 0;
	break;
      case 'c':					// Month
      case 'm':
	position= 1;
	break;
      case 'd':
      case 'e':
	position= 2;
	break;
      case 'h':
      case 'I':
      case 'l':
	need_p= 1;				// Need AM/PM
	/* Fall through */
      case 'k':
      case 'H':
	position= 3;
	break;
      case 'i':
	position= 4;
	break;
      case 's':
      case 'S':
	position= 5;
	break;
      case 'f':
	position= 6;
	if (dt_pos[5] != offset-1 || ptr[-2] != '.')
	  return 1;				// Wrong usage of %f
	break;
      case 'p':					// AM/PM
	if (offset == 0)			// Can't be first
	  return 0;
	position= 7;
	break;
      default:
	return 1;				// Unknown controll char
      }
      if (dt_pos[position] != 255)		// Don't allow same tag twice
	return 1;
      parts[position]= ptr-1;

      /*
	If switching from time to date, ensure that all time parts
	are used
      */
      if (part_map && position <= 2 && !(part_map & (1 | 2 | 4)))
	offset=5;
      part_map|= (ulong) 1 << position;
      dt_pos[position]= offset++;
      allow_separator= 1;
    }
    else
    {
      /*
	Don't allow any characters in format as this could easily confuse
	the date reader
      */
      if (!allow_separator)
	return 1;				// No separator here
      allow_separator= 0;			// Don't allow two separators
      separators++;
      /* Store in separator_map which parts are punct characters */
      if (my_ispunct(&my_charset_latin1, *ptr))
	separator_map|= (ulong) 1 << (offset-1);
      else if (!my_isspace(&my_charset_latin1, *ptr))
	return 1;
    }
  }

  /* If no %f, specify it after seconds.  Move %p up, if necessary */
  if ((part_map & 32) && !(part_map & 64))
  {
    dt_pos[6]= dt_pos[5] +1;
    parts[6]= parts[5];				// For later test in (need_p)
    if (dt_pos[6] == dt_pos[7])			// Move %p one step up if used
      dt_pos[7]++;
  }

  /*
    Check that we have not used a non legal format specifier and that all
    format specifiers have been used

    The last test is to ensure that %p is used if and only if
    it's needed.
  */
  if ((format_type == MYSQL_TIMESTAMP_DATETIME &&
       !test_all_bits(part_map, (1 | 2 | 4 | 8 | 16 | 32))) ||
      (format_type == MYSQL_TIMESTAMP_DATE && part_map != (1 | 2 | 4)) ||
      (format_type == MYSQL_TIMESTAMP_TIME &&
       !test_all_bits(part_map, 8 | 16 | 32)) ||
      !allow_separator ||			// %option should be last
      (need_p && dt_pos[6] +1 != dt_pos[7]) ||
      (need_p ^ (dt_pos[7] != 255)))
    return 1;

  if (dt_pos[6] != 255)				// If fractional seconds
  {
    /* remove fractional seconds from later tests */
    uint pos= dt_pos[6] -1;
    /* Remove separator before %f from sep map */
    separator_map= ((separator_map & ((ulong) (1 << pos)-1)) |
		    ((separator_map & ~((ulong) (1 << pos)-1)) >> 1));
    if (part_map & 64)			      
    {
      separators--;				// There is always a separator
      need_p= 1;				// force use of separators
    }
  }

  /*
    Remove possible separator before %p from sep_map
    (This can either be at position 3, 4, 6 or 7) h.m.d.%f %p
  */
  if (dt_pos[7] != 255)
  {
    if (need_p && parts[7] != parts[6]+2)
      separators--;
  }     
  /*
    Calculate if %p is in first or last part of the datetime field

    At this point we have either %H-%i-%s %p 'year parts' or
    'year parts' &H-%i-%s %p" as %f was removed above
  */
  offset= dt_pos[6] <= 3 ? 3 : 6;
  /* Remove separator before %p from sep map */
  separator_map= ((separator_map & ((ulong) (1 << offset)-1)) |
		  ((separator_map & ~((ulong) (1 << offset)-1)) >> 1));

  format_str= 0;
  switch (format_type) {
  case MYSQL_TIMESTAMP_DATE:
    format_str= known_date_time_formats[INTERNAL_FORMAT].date_format;
    /* fall through */
  case MYSQL_TIMESTAMP_TIME:
    if (!format_str)
      format_str=known_date_time_formats[INTERNAL_FORMAT].time_format;

    /*
      If there is no separators, allow the internal format as we can read
      this.  If separators are used, they must be between each part
    */
    if (format_length == 6 && !need_p &&
	!my_strnncoll(&my_charset_bin,
		      (const uchar *) format, 6, 
		      (const uchar *) format_str, 6))
      return 0;
    if (separator_map == (1 | 2))
    {
      if (format_type == MYSQL_TIMESTAMP_TIME)
      {
	if (*(format+2) != *(format+5))
	  break;				// Error
	/* Store the character used for time formats */
	date_time_format->time_separator= *(format+2);
      }
      return 0;
    }
    break;
  case MYSQL_TIMESTAMP_DATETIME:
    /*
      If there is no separators, allow the internal format as we can read
      this.  If separators are used, they must be between each part.
      Between DATE and TIME we also allow space as separator
    */
    if ((format_length == 12 && !need_p &&
	 !my_strnncoll(&my_charset_bin, 
		       (const uchar *) format, 12,
		       (const uchar*) known_date_time_formats[INTERNAL_FORMAT].datetime_format,
		       12)) ||
	(separators == 5 && separator_map == (1 | 2 | 8 | 16)))
      return 0;
    break;
  default:
    DBUG_ASSERT(1);
    break;
  }
  return 1;					// Error
}


/*
  Create a DATE_TIME_FORMAT object from a format string specification

  SYNOPSIS
    date_time_format_make()
    format_type		Format to parse (time, date or datetime)
    format_str		String to parse
    format_length	Length of string

  NOTES
    The returned object should be freed with my_free()

  RETURN
    NULL ponter:	Error
    new object
*/

DATE_TIME_FORMAT
*date_time_format_make(timestamp_type format_type,
		       const char *format_str, uint format_length)
{
  DATE_TIME_FORMAT tmp;

  if (format_length && format_length < 255 &&
      !parse_date_time_format(format_type, format_str,
			      format_length, &tmp))
  {
    tmp.format.str=    (char*) format_str;
    tmp.format.length= format_length;
    return date_time_format_copy((THD *)0, &tmp);
  }
  return 0;
}


/*
  Create a copy of a DATE_TIME_FORMAT object

  SYNOPSIS
    date_and_time_format_copy()
    thd			Set if variable should be allocated in thread mem
    format		format to copy

  NOTES
    The returned object should be freed with my_free()

  RETURN
    NULL ponter:	Error
    new object
*/

DATE_TIME_FORMAT *date_time_format_copy(THD *thd, DATE_TIME_FORMAT *format)
{
  DATE_TIME_FORMAT *new_format;
  ulong length= sizeof(*format) + format->format.length + 1;

  if (thd)
    new_format= (DATE_TIME_FORMAT *) thd->alloc(length);
  else
    new_format=  (DATE_TIME_FORMAT *) my_malloc(length, MYF(MY_WME));
  if (new_format)
  {
    /* Put format string after current pos */
    new_format->format.str= (char*) (new_format+1);
    memcpy((char*) new_format->positions, (char*) format->positions,
	   sizeof(format->positions));
    new_format->time_separator= format->time_separator;
    /* We make the string null terminated for easy printf in SHOW VARIABLES */
    memcpy((char*) new_format->format.str, format->format.str,
	   format->format.length);
    new_format->format.str[format->format.length]= 0;
    new_format->format.length= format->format.length;
  }
  return new_format;
}


KNOWN_DATE_TIME_FORMAT known_date_time_formats[6]=
{
  {"USA", "%m.%d.%Y", "%Y-%m-%d %H.%i.%s", "%h:%i:%s %p" },
  {"JIS", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s" },
  {"ISO", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s" },
  {"EUR", "%d.%m.%Y", "%Y-%m-%d %H.%i.%s", "%H.%i.%s" },
  {"INTERNAL", "%Y%m%d",   "%Y%m%d%H%i%s", "%H%i%s" },
  { 0, 0, 0, 0 }
};


/*
   Return format string according format name.
   If name is unknown, result is NULL
*/

const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				     timestamp_type type)
{
  switch (type) {
  case MYSQL_TIMESTAMP_DATE:
    return format->date_format;
  case MYSQL_TIMESTAMP_DATETIME:
    return format->datetime_format;
  case MYSQL_TIMESTAMP_TIME:
    return format->time_format;
  default:
    DBUG_ASSERT(0);				// Impossible
    return 0;
  }
}

/****************************************************************************
  Functions to create default time/date/datetime strings
 
  NOTE:
    For the moment the DATE_TIME_FORMAT argument is ignored becasue
    MySQL doesn't support comparing of date/time/datetime strings that
    are not in arbutary order as dates are compared as strings in some
    context)
    This functions don't check that given MYSQL_TIME structure members are
    in valid range. If they are not, return value won't reflect any 
    valid date either. Additionally, make_time doesn't take into
    account time->day member: it's assumed that days have been converted
    to hours already.
****************************************************************************/

/**
  Convert TIME value to String.
  @param format   Format (unused, see comments above)
  @param l_time   TIME value
  @param OUT str  String to conver to
  @param dec      Number of fractional digits.
*/
void make_time(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const MYSQL_TIME *l_time, String *str, uint dec)
{
  uint length= (uint) my_time_to_str(l_time, (char*) str->ptr(), dec);
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


/**
  Convert DATE value to String.
  @param format   Format (unused, see comments above)
  @param l_time   DATE value
  @param OUT str  String to conver to
*/
void make_date(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const MYSQL_TIME *l_time, String *str)
{
  uint length= (uint) my_date_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


/**
  Convert DATETIME value to String.
  @param format   Format (unused, see comments above)
  @param l_time   DATE value
  @param OUT str  String to conver to
  @param dec      Number of fractional digits.
*/
void make_datetime(const DATE_TIME_FORMAT *format __attribute__((unused)),
                   const MYSQL_TIME *l_time, String *str, uint dec)
{
  uint length= (uint) my_datetime_to_str(l_time, (char*) str->ptr(), dec);
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


/**
  Convert TIME/DATE/DATETIME value to String.
  @param l_time   DATE value
  @param OUT str  String to conver to
  @param dec      Number of fractional digits.
*/
bool my_TIME_to_str(const MYSQL_TIME *ltime, String *str, uint dec)
{
  if (str->alloc(MAX_DATE_STRING_REP_LENGTH))
    return true;
  str->set_charset(&my_charset_numeric);
  str->length(my_TIME_to_str(ltime, const_cast<char*>(str->ptr()), dec));
  return false;
}


void make_truncated_value_warning(THD *thd,
                                  Sql_condition::enum_warning_level level,
                                  ErrConvString val, timestamp_type time_type,
                                  const char *field_name)
{
  char warn_buff[MYSQL_ERRMSG_SIZE];
  const char *type_str;
  CHARSET_INFO *cs= system_charset_info;

  switch (time_type) {
    case MYSQL_TIMESTAMP_DATE: 
      type_str= "date";
      break;
    case MYSQL_TIMESTAMP_TIME:
      type_str= "time";
      break;
    case MYSQL_TIMESTAMP_DATETIME:  // FALLTHROUGH
    default:
      type_str= "datetime";
      break;
  }
  if (field_name)
    cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                       ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                       type_str, val.ptr(), field_name,
                       (ulong) thd->get_stmt_da()->current_row_for_warning());
  else
  {
    if (time_type > MYSQL_TIMESTAMP_ERROR)
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_TRUNCATED_WRONG_VALUE),
                         type_str, val.ptr());
    else
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_WRONG_VALUE), type_str, val.ptr());
  }
  push_warning(thd, level, ER_TRUNCATED_WRONG_VALUE, warn_buff);
}


/* Daynumber from year 0 to 9999-12-31 */
#define MAX_DAY_NUMBER 3652424L

bool date_add_interval(MYSQL_TIME *ltime, interval_type int_type, INTERVAL interval)
{
  long period, sign;

  ltime->neg= 0;

  sign= (interval.neg ? -1 : 1);

  switch (int_type) {
  case INTERVAL_SECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
  case INTERVAL_MINUTE:
  case INTERVAL_HOUR:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_DAY_SECOND:
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_DAY_HOUR:
  {
    longlong sec, days, daynr, microseconds, extra_sec;
    ltime->time_type= MYSQL_TIMESTAMP_DATETIME; // Return full date
    microseconds= ltime->second_part + sign*interval.second_part;
    extra_sec= microseconds/1000000L;
    microseconds= microseconds%1000000L;

    sec=((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
	 ltime->second +
	 sign* (longlong) (interval.day*3600*24L +
                           interval.hour*LL(3600)+interval.minute*LL(60)+
                           interval.second))+ extra_sec;
    if (microseconds < 0)
    {
      microseconds+= LL(1000000);
      sec--;
    }
    days= sec/(3600*LL(24));
    sec-= days*3600*LL(24);
    if (sec < 0)
    {
      days--;
      sec+= 3600*LL(24);
    }
    ltime->second_part= (uint) microseconds;
    ltime->second= (uint) (sec % 60);
    ltime->minute= (uint) (sec/60 % 60);
    ltime->hour=   (uint) (sec/3600);
    daynr= calc_daynr(ltime->year,ltime->month,1) + days;
    /* Day number from year 0 to 9999-12-31 */
    if ((ulonglong) daynr > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) daynr, &ltime->year, &ltime->month,
                        &ltime->day);
    break;
  }
  case INTERVAL_DAY:
  case INTERVAL_WEEK:
    period= (calc_daynr(ltime->year,ltime->month,ltime->day) +
             sign * (long) interval.day);
    /* Daynumber from year 0 to 9999-12-31 */
    if ((ulong) period > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year+= sign * (long) interval.year;
    if ((ulong) ltime->year >= 10000L)
      goto invalid_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign * (long) interval.year*12 +
	     ltime->month-1 + sign * (long) interval.month);
    if ((ulong) period >= 120000L)
      goto invalid_date;
    ltime->year= (uint) (period / 12);
    ltime->month= (uint) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day = days_in_month[ltime->month-1];
      if (ltime->month == 2 && calc_days_in_year(ltime->year) == 366)
	ltime->day++;				// Leap-year
    }
    break;
  default:
    goto null_date;
  }

  return 0;					// Ok

invalid_date:
  push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                      ER_DATETIME_FUNCTION_OVERFLOW,
                      ER(ER_DATETIME_FUNCTION_OVERFLOW),
                      "datetime");
null_date:
  return 1;
}


/*
  Calculate difference between two datetime values as seconds + microseconds.

  SYNOPSIS
    calc_time_diff()
      l_time1         - TIME/DATE/DATETIME value
      l_time2         - TIME/DATE/DATETIME value
      l_sign          - 1 absolute values are substracted,
                        -1 absolute values are added.
      seconds_out     - Out parameter where difference between
                        l_time1 and l_time2 in seconds is stored.
      microseconds_out- Out parameter where microsecond part of difference
                        between l_time1 and l_time2 is stored.

  NOTE
    This function calculates difference between l_time1 and l_time2 absolute
    values. So one should set l_sign and correct result if he want to take
    signs into account (i.e. for MYSQL_TIME values).

  RETURN VALUES
    Returns sign of difference.
    1 means negative result
    0 means positive result

*/

bool
calc_time_diff(const MYSQL_TIME *l_time1, const MYSQL_TIME *l_time2,
               int l_sign, longlong *seconds_out, long *microseconds_out)
{
  long days;
  bool neg;
  longlong microseconds;

  /*
    We suppose that if first argument is MYSQL_TIMESTAMP_TIME
    the second argument should be TIMESTAMP_TIME also.
    We should check it before calc_time_diff call.
  */
  if (l_time1->time_type == MYSQL_TIMESTAMP_TIME)  // Time value
    days= (long)l_time1->day - l_sign * (long)l_time2->day;
  else
  {
    days= calc_daynr((uint) l_time1->year,
		     (uint) l_time1->month,
		     (uint) l_time1->day);
    if (l_time2->time_type == MYSQL_TIMESTAMP_TIME)
      days-= l_sign * (long)l_time2->day;
    else
      days-= l_sign*calc_daynr((uint) l_time2->year,
			       (uint) l_time2->month,
			       (uint) l_time2->day);
  }

  microseconds= ((longlong)days * SECONDS_IN_24H +
                 (longlong)(l_time1->hour*3600L +
                            l_time1->minute*60L +
                            l_time1->second) -
                 l_sign*(longlong)(l_time2->hour*3600L +
                                   l_time2->minute*60L +
                                   l_time2->second)) * LL(1000000) +
                (longlong)l_time1->second_part -
                l_sign*(longlong)l_time2->second_part;

  neg= 0;
  if (microseconds < 0)
  {
    microseconds= -microseconds;
    neg= 1;
  }
  *seconds_out= microseconds/1000000L;
  *microseconds_out= (long) (microseconds%1000000L);
  return neg;
}


/*
  Compares 2 MYSQL_TIME structures

  SYNOPSIS
    my_time_compare()

      a - first time
      b - second time

  RETURN VALUE
   -1   - a < b
    0   - a == b
    1   - a > b

*/

int my_time_compare(MYSQL_TIME *a, MYSQL_TIME *b)
{
  ulonglong a_t= TIME_to_ulonglong_datetime(a);
  ulonglong b_t= TIME_to_ulonglong_datetime(b);

  if (a_t < b_t)
    return -1;
  if (a_t > b_t)
    return 1;

  if (a->second_part < b->second_part)
    return -1;
  if (a->second_part > b->second_part)
    return 1;

  return 0;
}


/* Rounding functions */
static uint msec_round_add[7]=
{
  500000000,
  50000000,
  5000000,
  500000,
  50000,
  5000,
  0
};


/**
  Round time value to the given precision.

  @param IN/OUT  ltime    The value to round.
  @param         dec      Precision.
  @return        False on success, true on error.
*/
bool my_time_round(MYSQL_TIME *ltime, uint dec)
{
  int warnings= 0;
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* Add half away from zero */
  bool rc= time_add_nanoseconds_with_round(ltime,
                                           msec_round_add[dec], &warnings);
  /* Truncate non-significant digits */
  my_time_trunc(ltime, dec);
  return rc;
}


/**
  Round datetime value to the given precision.

  @param IN/OUT  ltime    The value to round.
  @param         dec      Precision.
  @return        False on success, true on error.
*/
bool my_datetime_round(MYSQL_TIME *ltime, uint dec, int *warnings)
{
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* Add half away from zero */
  bool rc= datetime_add_nanoseconds_with_round(ltime,
                                               msec_round_add[dec], warnings);
  /* Truncate non-significant digits */
  my_time_trunc(ltime, dec);
  return rc;
}


/**
  Round timeval value to the given precision.

  @param IN/OUT  ts       The value to round.
  @param         dec      Precision.
  @return        False on success, true on error.
*/
bool my_timeval_round(struct timeval *tv, uint decimals)
{
  DBUG_ASSERT(decimals <= DATETIME_MAX_DECIMALS);
  uint nanoseconds= msec_round_add[decimals];
  tv->tv_usec+= (nanoseconds + 500) / 1000;
  if (tv->tv_usec < 1000000)
    goto ret;

  tv->tv_usec= 0;
  tv->tv_sec++;
  if (!IS_TIME_T_VALID_FOR_TIMESTAMP(tv->tv_sec))
  {
    tv->tv_sec= TIMESTAMP_MAX_VALUE;
    return true;
  }

ret:
  my_timeval_trunc(tv, decimals);
  return false;
}


/**
  Mix a date value and a time value.

  @param  IN/OUT  ldate  Date value.
  @param          ltime  Time value.
*/
void mix_date_and_time(MYSQL_TIME *ldate, const MYSQL_TIME *ltime)
{
  DBUG_ASSERT(ldate->time_type == MYSQL_TIMESTAMP_DATE ||
              ldate->time_type == MYSQL_TIMESTAMP_DATETIME);

  if (!ltime->neg && ltime->hour < 24)
  {
    /*
      Simple case: TIME is within normal 24 hours internal.
      Mix DATE part of ltime2 and TIME part of ltime together.
    */
    ldate->hour= ltime->hour;
    ldate->minute= ltime->minute;
    ldate->second= ltime->second;
    ldate->second_part= ltime->second_part;
  }
  else
  {
    /* Complex case: TIME is negative or outside of 24 hours internal. */
    longlong seconds;
    long days, useconds;
    int sign= ltime->neg ? 1 : -1;
    ldate->neg= calc_time_diff(ldate, ltime, sign, &seconds, &useconds);
    DBUG_ASSERT(!ldate->neg);

    /*
      We pass current date to mix_date_and_time. If we want to use
      this function with arbitrary dates, this code will need
      to cover cases when ltime is negative and "ldate < -ltime".
    */
    DBUG_ASSERT(ldate->year > 0);

    days= (long) (seconds / SECONDS_IN_24H);
    calc_time_from_sec(ldate, seconds % SECONDS_IN_24H, useconds);
    get_date_from_daynr(days, &ldate->year, &ldate->month, &ldate->day);
  }
  ldate->time_type= MYSQL_TIMESTAMP_DATETIME;
}


/**
  Convert MYSQL_TIME value to its packed numeric representation,
  using field type.
  @param ltime  The value to convert.
  @param type   MySQL field type.
  @retval       Packed numeric representation.
*/
longlong TIME_to_longlong_packed(const MYSQL_TIME *ltime,
                                 enum enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME:
    return TIME_to_longlong_time_packed(ltime);
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return TIME_to_longlong_datetime_packed(ltime);
  case MYSQL_TYPE_DATE:
    return TIME_to_longlong_date_packed(ltime);
  default:
    return TIME_to_longlong_packed(ltime);
  }
}


/**
  Convert packed numeric temporal representation to time, date or datetime,
  using field type.
  @param OUT  ltime        The variable to write to.
  @param      type         MySQL field type.
  @param      packed_value Numeric datetype representation.
*/
void TIME_from_longlong_packed(MYSQL_TIME *ltime,
                               enum enum_field_types type,
                               longlong packed_value)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME:
    TIME_from_longlong_time_packed(ltime, packed_value);
    break;
  case MYSQL_TYPE_DATE:
    TIME_from_longlong_date_packed(ltime, packed_value);
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    TIME_from_longlong_datetime_packed(ltime, packed_value);
    break;
  default:
    DBUG_ASSERT(0);
    set_zero_time(ltime, MYSQL_TIMESTAMP_ERROR);
    break;
  }
}


/**
  Unpack packed numeric temporal value to date/time value
  and then convert to decimal representation.

  @param  OUT dec          The variable to write to.
  @param      type         MySQL field type.
  @param      packed_value Packed numeric temporal representation.
  @return     A decimal value in on of the following formats, depending
              on type: YYYYMMDD, hhmmss.ffffff or YYMMDDhhmmss.ffffff.
*/
my_decimal *my_decimal_from_datetime_packed(my_decimal *dec,
                                            enum enum_field_types type,
                                            longlong packed_value)
{
  MYSQL_TIME ltime;
  switch (type)
  {
    case MYSQL_TYPE_TIME:
      TIME_from_longlong_time_packed(&ltime, packed_value);
      return time2my_decimal(&ltime, dec);
    case MYSQL_TYPE_DATE:
      TIME_from_longlong_date_packed(&ltime, packed_value);
      ulonglong2decimal(TIME_to_ulonglong_date(&ltime), dec);
      return dec;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      TIME_from_longlong_datetime_packed(&ltime, packed_value);
      return date2my_decimal(&ltime, dec);
    default:
      DBUG_ASSERT(0);
      ulonglong2decimal(0, dec);
      return dec;
  }
}


/**
  Convert packed numeric representation to
  unpacked numeric representation.
  @param type           MySQL field type.
  @param paacked_value  Packed numeric temporal value.
  @return               Number in one of the following formats,
                        depending on type: YYMMDD, YYMMDDhhmmss, hhmmss.
*/
longlong longlong_from_datetime_packed(enum enum_field_types type,
                                       longlong packed_value)
{
  MYSQL_TIME ltime;
  switch (type)
  {
    case MYSQL_TYPE_TIME:
      TIME_from_longlong_time_packed(&ltime, packed_value);
      return TIME_to_ulonglong_time(&ltime);
    case MYSQL_TYPE_DATE:
      TIME_from_longlong_date_packed(&ltime, packed_value);
      return TIME_to_ulonglong_date(&ltime);
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      TIME_from_longlong_datetime_packed(&ltime, packed_value);
      return TIME_to_ulonglong_datetime(&ltime);
    default:
      DBUG_ASSERT(0);
      return 0;
  }
}


/**
  Convert packed numeric temporal representation to unpacked numeric
  representation.
  @param type           MySQL field type.
  @param packed_value   Numeric packed temporal representation.
  @return               A double value in on of the following formats,
                        depending  on type:
                        YYYYMMDD, hhmmss.ffffff or YYMMDDhhmmss.ffffff.                        
*/
double double_from_datetime_packed(enum enum_field_types type,
                                   longlong packed_value)
{
  longlong result= longlong_from_datetime_packed(type, packed_value);
  return result +
        ((double) MY_PACKED_TIME_GET_FRAC_PART(packed_value)) / 1000000;
}

#endif

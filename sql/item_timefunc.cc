/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  This file defines all time functions

  @todo
    Move month and days to language files
*/

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // set_var.h: THD
#include "set_var.h"
#include "sql_locale.h"          // MY_LOCALE my_locale_en_US
#include "strfunc.h"             // check_word
#include "sql_time.h"            // make_truncated_value_warning,
                                 // make_time, get_date_from_daynr,
                                 // calc_weekday, calc_week,
                                 // convert_month_to_period,
                                 // convert_period_to_month,
                                 // TIME_to_timestamp, make_date,
                                 // calc_time_diff,
                                 // calc_time_from_sec,
                                 // known_date_time_format,
                                 // get_date_time_format_str
#include "tztime.h"              // struct Time_zone
#include "sql_class.h"           // THD
#include <m_ctype.h>
#include <time.h>

using std::min;
using std::max;

/** Day number for Dec 31st, 9999. */
#define MAX_DAY_NUMBER 3652424L


/**
  Check and adjust a time value with a warning.

  @param ltime    Time variable.
  @param decimals Precision. 
  @retval         True on error, false of success.
*/
static void
adjust_time_range_with_warn(MYSQL_TIME *ltime, uint8 decimals)
{
  /* Fatally bad value should not come here */
  if (check_time_range_quick(ltime))
  {
    int warning= 0;
    make_truncated_value_warning(ErrConvString(ltime, decimals),
                                 MYSQL_TIMESTAMP_TIME);
    adjust_time_range(ltime, &warning);
  }
}


/*
  Convert seconds to MYSQL_TIME value with overflow checking.

  SYNOPSIS:
    sec_to_time()
    seconds          number of seconds
    ltime            output MYSQL_TIME value

  DESCRIPTION
    If the 'seconds' argument is inside MYSQL_TIME data range, convert it to a
    corresponding value.
    Otherwise, truncate the resulting value to the nearest endpoint.

  RETURN
    1                if the value was truncated during conversion
    0                otherwise
*/

static bool sec_to_time(lldiv_t seconds, MYSQL_TIME *ltime)
{
  int warning= 0;

  set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  
  if (seconds.quot < 0 || seconds.rem < 0)
  {
    ltime->neg= 1;
    seconds.quot= -seconds.quot;
    seconds.rem= -seconds.rem;
  }
  
  if (seconds.quot > TIME_MAX_VALUE_SECONDS)
  {
    set_max_hhmmss(ltime);
    return true;
  }

  ltime->hour= (uint) (seconds.quot / 3600);
  uint sec= (uint) (seconds.quot % 3600);
  ltime->minute= sec / 60;
  ltime->second= sec % 60;
  ltime->second_part= (uint) (seconds.rem / 1000);
  
  adjust_time_range(ltime, &warning);

  return warning ? true : false;
}


/*
  Date formats corresponding to compound %r and %T conversion specifiers

  Note: We should init at least first element of "positions" array
        (first member) or hpux11 compiler will die horribly.
*/
static DATE_TIME_FORMAT time_ampm_format= {{0}, '\0', 0,
                                           {(char *)"%I:%i:%S %p", 11}};
static DATE_TIME_FORMAT time_24hrs_format= {{0}, '\0', 0,
                                            {(char *)"%H:%i:%S", 8}};

/**
  Extract datetime value to MYSQL_TIME struct from string value
  according to format string.

  @param format		date/time format specification
  @param val			String to decode
  @param length		Length of string
  @param l_time		Store result here
  @param cached_timestamp_type  It uses to get an appropriate warning
                                in the case when the value is truncated.
  @param sub_pattern_end    if non-zero then we are parsing string which
                            should correspond compound specifier (like %T or
                            %r) and this parameter is pointer to place where
                            pointer to end of string matching this specifier
                            should be stored.

  @note
    Possibility to parse strings matching to patterns equivalent to compound
    specifiers is mainly intended for use from inside of this function in
    order to understand %T and %r conversion specifiers, so number of
    conversion specifiers that can be used in such sub-patterns is limited.
    Also most of checks are skipped in this case.

  @note
    If one adds new format specifiers to this function he should also
    consider adding them to Item_func_str_to_date::fix_from_format().

  @retval
    0	ok
  @retval
    1	error
*/

static bool extract_date_time(DATE_TIME_FORMAT *format,
			      const char *val, uint length, MYSQL_TIME *l_time,
                              timestamp_type cached_timestamp_type,
                              const char **sub_pattern_end,
                              const char *date_time_type)
{
  int weekday= 0, yearday= 0, daypart= 0;
  int week_number= -1;
  int error= 0;
  int  strict_week_number_year= -1;
  int frac_part;
  bool usa_time= 0;
  bool UNINIT_VAR(sunday_first_n_first_week_non_iso);
  bool UNINIT_VAR(strict_week_number);
  bool UNINIT_VAR(strict_week_number_year_type);
  const char *val_begin= val;
  const char *val_end= val + length;
  const char *ptr= format->format.str;
  const char *end= ptr + format->format.length;
  const CHARSET_INFO *cs= &my_charset_bin;
  DBUG_ENTER("extract_date_time");

  if (!sub_pattern_end)
    memset(l_time, 0, sizeof(*l_time));

  for (; ptr != end && val != val_end; ptr++)
  {
    /* Skip pre-space between each argument */
    if ((val+= cs->cset->scan(cs, val, val_end, MY_SEQ_SPACES)) >= val_end)
      break;

    if (*ptr == '%' && ptr+1 != end)
    {
      int val_len;
      char *tmp;

      error= 0;

      val_len= (uint) (val_end - val);
      switch (*++ptr) {
	/* Year */
      case 'Y':
	tmp= (char*) val + MY_MIN(4, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
        if ((int) (tmp-val) <= 2)
          l_time->year= year_2000_handling(l_time->year);
	val= tmp;
	break;
      case 'y':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
        l_time->year= year_2000_handling(l_time->year);
	break;

	/* Month */
      case 'm':
      case 'c':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->month= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'M':
	if ((l_time->month= check_word(my_locale_en_US.month_names,
				       val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'b':
	if ((l_time->month= check_word(my_locale_en_US.ab_month_names,
				       val, val_end, &val)) <= 0)
	  goto err;
	break;
	/* Day */
      case 'd':
      case 'e':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'D':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	/* Skip 'st, 'nd, 'th .. */
	val= tmp + MY_MIN((int) (val_end-tmp), 2);
	break;

	/* Hour */
      case 'h':
      case 'I':
      case 'l':
	usa_time= 1;
	/* fall through */
      case 'k':
      case 'H':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->hour= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Minute */
      case 'i':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->minute= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second */
      case 's':
      case 'S':
	tmp= (char*) val + MY_MIN(2, val_len);
	l_time->second= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second part */
      case 'f':
	tmp= (char*) val_end;
	if (tmp - val > 6)
	  tmp= (char*) val + 6;
	l_time->second_part= (int) my_strtoll10(val, &tmp, &error);
	frac_part= 6 - (int) (tmp - val);
	if (frac_part > 0)
	  l_time->second_part*= (ulong) log_10_int[frac_part];
	val= tmp;
	break;

	/* AM / PM */
      case 'p':
	if (val_len < 2 || ! usa_time)
	  goto err;
	if (!my_strnncoll(&my_charset_latin1,
			  (const uchar *) val, 2, 
			  (const uchar *) "PM", 2))
	  daypart= 12;
	else if (my_strnncoll(&my_charset_latin1,
			      (const uchar *) val, 2, 
			      (const uchar *) "AM", 2))
	  goto err;
	val+= 2;
	break;

	/* Exotic things */
      case 'W':
	if ((weekday= check_word(my_locale_en_US.day_names, val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'a':
	if ((weekday= check_word(my_locale_en_US.ab_day_names, val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'w':
	tmp= (char*) val + 1;
	if ((weekday= (int) my_strtoll10(val, &tmp, &error)) < 0 ||
	    weekday >= 7)
	  goto err;
        /* We should use the same 1 - 7 scale for %w as for %W */
        if (!weekday)
          weekday= 7;
	val= tmp;
	break;
      case 'j':
	tmp= (char*) val + MY_MIN(val_len, 3);
	yearday= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

        /* Week numbers */
      case 'V':
      case 'U':
      case 'v':
      case 'u':
        sunday_first_n_first_week_non_iso= (*ptr=='U' || *ptr== 'V');
        strict_week_number= (*ptr=='V' || *ptr=='v');
	tmp= (char*) val + MY_MIN(val_len, 2);
	if ((week_number= (int) my_strtoll10(val, &tmp, &error)) < 0 ||
            (strict_week_number && !week_number) ||
            week_number > 53)
          goto err;
	val= tmp;
	break;

        /* Year used with 'strict' %V and %v week numbers */
      case 'X':
      case 'x':
        strict_week_number_year_type= (*ptr=='X');
        tmp= (char*) val + MY_MIN(4, val_len);
        strict_week_number_year= (int) my_strtoll10(val, &tmp, &error);
        val= tmp;
        break;

        /* Time in AM/PM notation */
      case 'r':
        /*
          We can't just set error here, as we don't want to generate two
          warnings in case of errors
        */
        if (extract_date_time(&time_ampm_format, val,
                              (uint)(val_end - val), l_time,
                              cached_timestamp_type, &val, "time"))
          DBUG_RETURN(1);
        break;

        /* Time in 24-hour notation */
      case 'T':
        if (extract_date_time(&time_24hrs_format, val,
                              (uint)(val_end - val), l_time,
                              cached_timestamp_type, &val, "time"))
          DBUG_RETURN(1);
        break;

        /* Conversion specifiers that match classes of characters */
      case '.':
	while (my_ispunct(cs, *val) && val != val_end)
	  val++;
	break;
      case '@':
	while (my_isalpha(cs, *val) && val != val_end)
	  val++;
	break;
      case '#':
	while (my_isdigit(cs, *val) && val != val_end)
	  val++;
	break;
      default:
	goto err;
      }
      if (error)				// Error from my_strtoll10
	goto err;
    }
    else if (!my_isspace(cs, *ptr))
    {
      if (*val != *ptr)
	goto err;
      val++;
    }
  }
  if (usa_time)
  {
    if (l_time->hour > 12 || l_time->hour < 1)
      goto err;
    l_time->hour= l_time->hour%12+daypart;
  }

  /*
    If we are recursively called for parsing string matching compound
    specifiers we are already done.
  */
  if (sub_pattern_end)
  {
    *sub_pattern_end= val;
    DBUG_RETURN(0);
  }

  if (yearday > 0)
  {
    uint days;
    days= calc_daynr(l_time->year,1,1) +  yearday - 1;
    if (days <= 0 || days > MAX_DAY_NUMBER)
      goto err;
    get_date_from_daynr(days,&l_time->year,&l_time->month,&l_time->day);
  }

  if (week_number >= 0 && weekday)
  {
    int days;
    uint weekday_b;

    /*
      %V,%v require %X,%x resprectively,
      %U,%u should be used with %Y and not %X or %x
    */
    if ((strict_week_number &&
        (strict_week_number_year < 0 ||
         strict_week_number_year_type != sunday_first_n_first_week_non_iso)) ||
        (!strict_week_number && strict_week_number_year >= 0))
      goto err;

    /* Number of days since year 0 till 1st Jan of this year */
    days= calc_daynr((strict_week_number ? strict_week_number_year :
                                           l_time->year),
                     1, 1);
    /* Which day of week is 1st Jan of this year */
    weekday_b= calc_weekday(days, sunday_first_n_first_week_non_iso);

    /*
      Below we are going to sum:
      1) number of days since year 0 till 1st day of 1st week of this year
      2) number of days between 1st week and our week
      3) and position of our day in the week
    */
    if (sunday_first_n_first_week_non_iso)
    {
      days+= ((weekday_b == 0) ? 0 : 7) - weekday_b +
             (week_number - 1) * 7 +
             weekday % 7;
    }
    else
    {
      days+= ((weekday_b <= 3) ? 0 : 7) - weekday_b +
             (week_number - 1) * 7 +
             (weekday - 1);
    }

    if (days <= 0 || days > MAX_DAY_NUMBER)
      goto err;
    get_date_from_daynr(days,&l_time->year,&l_time->month,&l_time->day);
  }

  if (l_time->month > 12 || l_time->day > 31 || l_time->hour > 23 || 
      l_time->minute > 59 || l_time->second > 59)
    goto err;

  if (val != val_end)
  {
    do
    {
      if (!my_isspace(&my_charset_latin1,*val))
      {
        // TS-TODO: extract_date_time is not UCS2 safe
        make_truncated_value_warning(ErrConvString(val_begin, length),
                                     cached_timestamp_type);
	break;
      }
    } while (++val != val_end);
  }
  DBUG_RETURN(0);

err:
  {
    char buff[128];
    strmake(buff, val_begin, min<size_t>(length, sizeof(buff)-1));
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WRONG_VALUE_FOR_TYPE, ER(ER_WRONG_VALUE_FOR_TYPE),
                        date_time_type, buff, "str_to_date");
  }
  DBUG_RETURN(1);
}


/**
  Create a formated date/time value in a string.
*/

bool make_date_time(DATE_TIME_FORMAT *format, MYSQL_TIME *l_time,
		    timestamp_type type, String *str)
{
  char intbuff[15];
  uint hours_i;
  uint weekday;
  ulong length;
  const char *ptr, *end;
  THD *thd= current_thd;
  MY_LOCALE *locale= thd->variables.lc_time_names;

  str->length(0);

  if (l_time->neg)
    str->append('-');
  
  end= (ptr= format->format.str) + format->format.length;
  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr+1 == end)
      str->append(*ptr);
    else
    {
      switch (*++ptr) {
      case 'M':
        if (!l_time->month)
          return 1;
        str->append(locale->month_names->type_names[l_time->month-1],
                    (uint) strlen(locale->month_names->type_names[l_time->month-1]),
                    system_charset_info);
        break;
      case 'b':
        if (!l_time->month)
          return 1;
        str->append(locale->ab_month_names->type_names[l_time->month-1],
                    (uint) strlen(locale->ab_month_names->type_names[l_time->month-1]),
                    system_charset_info);
        break;
      case 'W':
        if (type == MYSQL_TIMESTAMP_TIME || !(l_time->month || l_time->year))
          return 1;
        weekday= calc_weekday(calc_daynr(l_time->year,l_time->month,
                              l_time->day),0);
        str->append(locale->day_names->type_names[weekday],
                    (uint) strlen(locale->day_names->type_names[weekday]),
                    system_charset_info);
        break;
      case 'a':
        if (type == MYSQL_TIMESTAMP_TIME || !(l_time->month || l_time->year))
          return 1;
        weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
                             l_time->day),0);
        str->append(locale->ab_day_names->type_names[weekday],
                    (uint) strlen(locale->ab_day_names->type_names[weekday]),
                    system_charset_info);
        break;
      case 'D':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= (uint) (int10_to_str(l_time->day, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	if (l_time->day >= 10 &&  l_time->day <= 19)
	  str->append(STRING_WITH_LEN("th"));
	else
	{
	  switch (l_time->day %10) {
	  case 1:
	    str->append(STRING_WITH_LEN("st"));
	    break;
	  case 2:
	    str->append(STRING_WITH_LEN("nd"));
	    break;
	  case 3:
	    str->append(STRING_WITH_LEN("rd"));
	    break;
	  default:
	    str->append(STRING_WITH_LEN("th"));
	    break;
	  }
	}
	break;
      case 'Y':
	length= (uint) (int10_to_str(l_time->year, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 4, '0');
	break;
      case 'y':
	length= (uint) (int10_to_str(l_time->year%100, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'm':
	length= (uint) (int10_to_str(l_time->month, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'c':
	length= (uint) (int10_to_str(l_time->month, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'd':
	length= (uint) (int10_to_str(l_time->day, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'e':
	length= (uint) (int10_to_str(l_time->day, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'f':
	length= (uint) (int10_to_str(l_time->second_part, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 6, '0');
	break;
      case 'H':
	length= (uint) (int10_to_str(l_time->hour, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'h':
      case 'I':
	hours_i= (l_time->hour%24 + 11)%12+1;
	length= (uint) (int10_to_str(hours_i, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'i':					/* minutes */
	length= (uint) (int10_to_str(l_time->minute, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'j':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= (uint) (int10_to_str(calc_daynr(l_time->year,l_time->month,
					l_time->day) - 
		     calc_daynr(l_time->year,1,1) + 1, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 3, '0');
	break;
      case 'k':
	length= (uint) (int10_to_str(l_time->hour, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'l':
	hours_i= (l_time->hour%24 + 11)%12+1;
	length= (uint) (int10_to_str(hours_i, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'p':
	hours_i= l_time->hour%24;
	str->append(hours_i < 12 ? "AM" : "PM",2);
	break;
      case 'r':
	length= sprintf(intbuff, ((l_time->hour % 24) < 12) ?
                        "%02d:%02d:%02d AM" : "%02d:%02d:%02d PM",
		        (l_time->hour+11)%12+1,
		        l_time->minute,
		        l_time->second);
	str->append(intbuff, length);
	break;
      case 'S':
      case 's':
	length= (uint) (int10_to_str(l_time->second, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'T':
	length= sprintf(intbuff,  "%02d:%02d:%02d",
                        l_time->hour, l_time->minute, l_time->second);
	str->append(intbuff, length);
	break;
      case 'U':
      case 'u':
      {
	uint year;
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= (uint) (int10_to_str(calc_week(l_time,
				       (*ptr) == 'U' ?
				       WEEK_FIRST_WEEKDAY : WEEK_MONDAY_FIRST,
				       &year),
			     intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'v':
      case 'V':
      {
	uint year;
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= (uint) (int10_to_str(calc_week(l_time,
				       ((*ptr) == 'V' ?
					(WEEK_YEAR | WEEK_FIRST_WEEKDAY) :
					(WEEK_YEAR | WEEK_MONDAY_FIRST)),
				       &year),
			     intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'x':
      case 'X':
      {
	uint year;
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	(void) calc_week(l_time,
			 ((*ptr) == 'X' ?
			  WEEK_YEAR | WEEK_FIRST_WEEKDAY :
			  WEEK_YEAR | WEEK_MONDAY_FIRST),
			 &year);
	length= (uint) (int10_to_str(year, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 4, '0');
      }
      break;
      case 'w':
	if (type == MYSQL_TIMESTAMP_TIME || !(l_time->month || l_time->year))
	  return 1;
	weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
					l_time->day),1);
	length= (uint) (int10_to_str(weekday, intbuff, 10) - intbuff);
	str->append_with_prefill(intbuff, length, 1, '0');
	break;

      default:
	str->append(*ptr);
	break;
      }
    }
  }
  return 0;
}


/**
  @details
  Get a array of positive numbers from a string object.
  Each number is separated by 1 non digit character
  Return error if there is too many numbers.
  If there is too few numbers, assume that the numbers are left out
  from the high end. This allows one to give:
  DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.

  @param args            item expression which we convert to an ASCII string
  @param str_value       string buffer
  @param is_negative     set to true if interval is prefixed by '-'
  @param count:          count of elements in result array
  @param values:         array of results
  @param transform_msec: if value is true we suppose
                         that the last part of string value is microseconds
                         and we should transform value to six digit value.
                         For example, '1.1' -> '1.100000'
*/

static bool get_interval_info(Item *args,
                              String *str_value,
                              bool *is_negative,
                              uint count, ulonglong *values,
                              bool transform_msec)
{
  String *res;
  if (!(res= args->val_str_ascii(str_value)))
    return true;

  const CHARSET_INFO *cs= res->charset();
  const char *str= res->ptr();
  const char *end= str + res->length();

  str+= cs->cset->scan(cs, str, end, MY_SEQ_SPACES);
  if (str < end && *str == '-')
  {
    *is_negative= true;
    str++;
  }

  while (str < end && !my_isdigit(cs,*str))
    str++;

  long msec_length= 0;
  for (uint i=0 ; i < count ; i++)
  {
    longlong value;
    const char *start= str;
    for (value=0; str != end && my_isdigit(cs,*str) ; str++)
      value= value*LL(10) + (longlong) (*str - '0');
    msec_length= 6 - (str - start);
    values[i]= value;
    while (str != end && !my_isdigit(cs,*str))
      str++;
    if (str == end && i != count-1)
    {
      i++;
      /* Change values[0...i-1] -> values[0...count-1] */
      bmove_upp((uchar*) (values+count), (uchar*) (values+i),
		sizeof(*values)*i);
      memset(values, 0, sizeof(*values)*(count-i));
      break;
    }
  }

  if (transform_msec && msec_length > 0)
    values[count - 1] *= (long) log_10_int[msec_length];

  return (str != end);
}


/*** Abstract classes ****************************************/

bool Item_temporal_func::check_precision()
{
  if (decimals > DATETIME_MAX_DECIMALS)
  {
    my_error(ER_TOO_BIG_PRECISION, MYF(0),
             (int) decimals, func_name(), DATETIME_MAX_DECIMALS);
    return true;
  }
  return false;
}


type_conversion_status
Item_temporal_hybrid_func::save_in_field(Field *field, bool no_conversions)
{
  if (cached_field_type == MYSQL_TYPE_TIME)
    return save_time_in_field(field);
  if (is_temporal_type_with_date(cached_field_type))
    return save_date_in_field(field);
  return Item_str_func::save_in_field(field, no_conversions);
}


my_decimal *Item_temporal_hybrid_func::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  if (cached_field_type == MYSQL_TYPE_TIME)
    return val_decimal_from_time(decimal_value);
  else if (cached_field_type == MYSQL_TYPE_DATETIME)
    return val_decimal_from_date(decimal_value);
  else
  {
    MYSQL_TIME ltime;
    val_datetime(&ltime, TIME_FUZZY_DATE | sql_mode);
    return null_value ? 0:
           ltime.time_type == MYSQL_TIMESTAMP_TIME ? 
           time2my_decimal(&ltime, decimal_value) :
           date2my_decimal(&ltime, decimal_value);
  }
}


bool Item_temporal_hybrid_func::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  MYSQL_TIME tm;
  if (val_datetime(&tm, fuzzy_date))
  {
    DBUG_ASSERT(null_value == true);
    return true;
  }
  if (cached_field_type == MYSQL_TYPE_TIME ||
      tm.time_type == MYSQL_TIMESTAMP_TIME)
    time_to_datetime(current_thd, &tm, ltime);
  else
    *ltime= tm;
  return false;
}


bool Item_temporal_hybrid_func::get_time(MYSQL_TIME *ltime)
{
  if (val_datetime(ltime, TIME_FUZZY_DATE))
  {
    DBUG_ASSERT(null_value == true);
    return true;
  }
  if (cached_field_type == MYSQL_TYPE_TIME &&
      ltime->time_type != MYSQL_TIMESTAMP_TIME)
    datetime_to_time(ltime);
  return false;
}


String *Item_temporal_hybrid_func::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;

  if (val_datetime(&ltime, TIME_FUZZY_DATE) ||
      (null_value= my_TIME_to_str(&ltime, str,
                                  cached_field_type == MYSQL_TYPE_STRING ?
                                  (ltime.second_part ?
                                   DATETIME_MAX_DECIMALS : 0) :
                                  decimals)))
    return (String *) 0;

  /* Check that the returned timestamp type matches to the function type */
  DBUG_ASSERT((cached_field_type == MYSQL_TYPE_TIME &&
               ltime.time_type == MYSQL_TIMESTAMP_TIME) ||
              (cached_field_type == MYSQL_TYPE_DATE &&
               ltime.time_type == MYSQL_TIMESTAMP_DATE) ||
              (cached_field_type == MYSQL_TYPE_DATETIME &&
               ltime.time_type == MYSQL_TIMESTAMP_DATETIME) ||
               cached_field_type == MYSQL_TYPE_STRING ||
               ltime.time_type == MYSQL_TIMESTAMP_NONE);
  return str;
}


longlong Item_time_func::val_time_temporal()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_time(&ltime) ? 0LL : TIME_to_longlong_time_packed(&ltime);
}


longlong Item_date_func::val_date_temporal()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_date(&ltime, TIME_FUZZY_DATE) ?
         0LL : TIME_to_longlong_date_packed(&ltime);
}


longlong Item_datetime_func::val_date_temporal()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_date(&ltime, TIME_FUZZY_DATE) ?
         0LL : TIME_to_longlong_datetime_packed(&ltime);
}


bool Item_date_literal::eq(const Item *item, bool binary_cmp) const
{
  return
    item->basic_const_item() && type() == item->type() &&
    func_name() == ((Item_func *) item)->func_name() &&
    cached_time.eq(((Item_date_literal *) item)->cached_time);
}


void Item_date_literal::print(String *str, enum_query_type query_type)
{
  str->append("DATE'");
  str->append(cached_time.cptr());
  str->append('\'');
}


bool Item_datetime_literal::eq(const Item *item, bool binary_cmp) const
{
  return
    item->basic_const_item() && type() == item->type() &&
    func_name() == ((Item_func *) item)->func_name() &&
    cached_time.eq(((Item_datetime_literal *) item)->cached_time);
}


void Item_datetime_literal::print(String *str, enum_query_type query_type)
{
  str->append("TIMESTAMP'");
  str->append(cached_time.cptr());
  str->append('\'');
}


bool Item_time_literal::eq(const Item *item, bool binary_cmp) const
{
  return
    item->basic_const_item() && type() == item->type() &&
    func_name() == ((Item_func *) item)->func_name() &&
    cached_time.eq(((Item_time_literal *) item)->cached_time);
}


void Item_time_literal::print(String *str, enum_query_type query_type)
{
  str->append("TIME'");
  str->append(cached_time.cptr());
  str->append('\'');
}


longlong Item_func_period_add::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulong period=(ulong) args[0]->val_int();
  int months=(int) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value) ||
      period == 0L)
    return 0; /* purecov: inspected */
  return (longlong)
    convert_month_to_period((uint) ((int) convert_period_to_month(period)+
				    months));
}


longlong Item_func_period_diff::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulong period1=(ulong) args[0]->val_int();
  ulong period2=(ulong) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return (longlong) ((long) convert_period_to_month(period1)-
		     (long) convert_period_to_month(period2));
}



longlong Item_func_to_days::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day);
}


longlong Item_func_to_seconds::val_int_endpoint(bool left_endp,
                                                bool *incl_endp)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  longlong seconds;
  longlong days;
  int dummy;                                /* unused */
  if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
  {
    /* got NULL, leave the incl_endp intact */
    return LONGLONG_MIN;
  }
  seconds= ltime.hour * 3600L + ltime.minute * 60 + ltime.second;
  seconds= ltime.neg ? -seconds : seconds;
  days= (longlong) calc_daynr(ltime.year, ltime.month, ltime.day);
  seconds+= days * 24L * 3600L;
  /* Set to NULL if invalid date, but keep the value */
  null_value= check_date(&ltime, non_zero_date(&ltime),
                         (TIME_NO_ZERO_IN_DATE | TIME_NO_ZERO_DATE),
                         &dummy);
  /*
    Even if the evaluation return NULL, seconds is useful for pruning
  */
  return seconds;
}

longlong Item_func_to_seconds::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  longlong seconds;
  longlong days;
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;
  seconds= ltime.hour * 3600L + ltime.minute * 60 + ltime.second;
  seconds=ltime.neg ? -seconds : seconds;
  days= (longlong) calc_daynr(ltime.year, ltime.month, ltime.day);
  return seconds + days * 24L * 3600L;
}

/*
  Get information about this Item tree monotonicity

  SYNOPSIS
    Item_func_to_days::get_monotonicity_info()

  DESCRIPTION
  Get information about monotonicity of the function represented by this item
  tree.

  RETURN
    See enum_monotonicity_info.
*/

enum_monotonicity_info Item_func_to_days::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM)
  {
    if (args[0]->field_type() == MYSQL_TYPE_DATE)
      return MONOTONIC_STRICT_INCREASING_NOT_NULL;
    if (args[0]->field_type() == MYSQL_TYPE_DATETIME)
      return MONOTONIC_INCREASING_NOT_NULL;
  }
  return NON_MONOTONIC;
}

enum_monotonicity_info Item_func_to_seconds::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM)
  {
    if (args[0]->field_type() == MYSQL_TYPE_DATE ||
        args[0]->field_type() == MYSQL_TYPE_DATETIME)
      return MONOTONIC_STRICT_INCREASING_NOT_NULL;
  }
  return NON_MONOTONIC;
}


longlong Item_func_to_days::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  longlong res;
  int dummy;                                /* unused */
  if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
  {
    /* got NULL, leave the incl_endp intact */
    return LONGLONG_MIN;
  }
  res=(longlong) calc_daynr(ltime.year,ltime.month,ltime.day);
  /* Set to NULL if invalid date, but keep the value */
  null_value= check_date(&ltime, non_zero_date(&ltime),
                         (TIME_NO_ZERO_IN_DATE | TIME_NO_ZERO_DATE),
                         &dummy);
  if (null_value)
  {
    /*
      Even if the evaluation return NULL, the calc_daynr is useful for pruning
    */
    if (args[0]->field_type() != MYSQL_TYPE_DATE)
      *incl_endp= TRUE;
    return res;
  }
  
  if (args[0]->field_type() == MYSQL_TYPE_DATE)
  {
    // TO_DAYS() is strictly monotonic for dates, leave incl_endp intact
    return res;
  }
 
  /*
    Handle the special but practically useful case of datetime values that
    point to day bound ("strictly less" comparison stays intact):

      col < '2007-09-15 00:00:00'  -> TO_DAYS(col) <  TO_DAYS('2007-09-15')
      col > '2007-09-15 23:59:59'  -> TO_DAYS(col) >  TO_DAYS('2007-09-15')

    which is different from the general case ("strictly less" changes to
    "less or equal"):

      col < '2007-09-15 12:34:56'  -> TO_DAYS(col) <= TO_DAYS('2007-09-15')
  */
  if ((!left_endp && !(ltime.hour || ltime.minute || ltime.second ||
                       ltime.second_part)) ||
       (left_endp && ltime.hour == 23 && ltime.minute == 59 &&
        ltime.second == 59))
    /* do nothing */
    ;
  else
    *incl_endp= TRUE;
  return res;
}


longlong Item_func_dayofyear::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime,TIME_NO_ZERO_DATE))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day) -
    calc_daynr(ltime.year,1,1) + 1;
}

longlong Item_func_dayofmonth::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_date(&ltime, TIME_FUZZY_DATE) ? 0 : (longlong) ltime.day;
}

longlong Item_func_month::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_date(&ltime, TIME_FUZZY_DATE) ? 0 : (longlong) ltime.month;
}


void Item_func_monthname::fix_length_and_dec()
{
  THD* thd= current_thd;
  const CHARSET_INFO *cs= thd->variables.collation_connection;
  uint32 repertoire= my_charset_repertoire(cs);
  locale= thd->variables.lc_time_names;  
  collation.set(cs, DERIVATION_COERCIBLE, repertoire);
  decimals=0;
  max_length= locale->max_month_name_length * collation.collation->mbmaxlen;
  maybe_null=1; 
}


String* Item_func_monthname::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  const char *month_name;
  uint err;
  MYSQL_TIME ltime;

  if ((null_value= (get_arg0_date(&ltime, TIME_FUZZY_DATE) || !ltime.month)))
    return (String *) 0;

  month_name= locale->month_names->type_names[ltime.month - 1];
  str->copy(month_name, (uint) strlen(month_name), &my_charset_utf8_bin,
	    collation.collation, &err);
  return str;
}


/**
  Returns the quarter of the year.
*/

longlong Item_func_quarter::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
    return 0;
  return (longlong) ((ltime.month+2)/3);
}

longlong Item_func_hour::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_time(&ltime) ? 0 : ltime.hour;
}

longlong Item_func_minute::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_time(&ltime) ? 0 : ltime.minute;
}

/**
  Returns the second in time_exp in the range of 0 - 59.
*/
longlong Item_func_second::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_time(&ltime) ? 0 : ltime.second;
}


uint week_mode(uint mode)
{
  uint week_format= (mode & 7);
  if (!(week_format & WEEK_MONDAY_FIRST))
    week_format^= WEEK_FIRST_WEEKDAY;
  return week_format;
}

/**
 @verbatim
  The bits in week_format(for calc_week() function) has the following meaning:
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
 @endverbatim
*/

longlong Item_func_week::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint year;
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;
  return (longlong) calc_week(&ltime,
			      week_mode((uint) args[1]->val_int()),
			      &year);
}


longlong Item_func_yearweek::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint year,week;
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;
  week= calc_week(&ltime, 
		  (week_mode((uint) args[1]->val_int()) | WEEK_YEAR),
		  &year);
  return week+year*100;
}


longlong Item_func_weekday::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;

  return (longlong) calc_weekday(calc_daynr(ltime.year, ltime.month,
                                            ltime.day),
                                 odbc_type) + test(odbc_type);
}

void Item_func_dayname::fix_length_and_dec()
{
  THD* thd= current_thd;
  const CHARSET_INFO *cs= thd->variables.collation_connection;
  uint32 repertoire= my_charset_repertoire(cs);
  locale= thd->variables.lc_time_names;  
  collation.set(cs, DERIVATION_COERCIBLE, repertoire);
  decimals=0;
  max_length= locale->max_day_name_length * collation.collation->mbmaxlen;
  maybe_null=1; 
}


String* Item_func_dayname::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  uint weekday=(uint) val_int();		// Always Item_func_daynr()
  const char *day_name;
  uint err;

  if (null_value)
    return (String*) 0;
  
  day_name= locale->day_names->type_names[weekday];
  str->copy(day_name, (uint) strlen(day_name), &my_charset_utf8_bin,
	    collation.collation, &err);
  return str;
}


longlong Item_func_year::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_date(&ltime, TIME_FUZZY_DATE) ? 0 : (longlong) ltime.year;
}


/*
  Get information about this Item tree monotonicity

  SYNOPSIS
    Item_func_year::get_monotonicity_info()

  DESCRIPTION
  Get information about monotonicity of the function represented by this item
  tree.

  RETURN
    See enum_monotonicity_info.
*/

enum_monotonicity_info Item_func_year::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM &&
      (args[0]->field_type() == MYSQL_TYPE_DATE ||
       args[0]->field_type() == MYSQL_TYPE_DATETIME))
    return MONOTONIC_INCREASING;
  return NON_MONOTONIC;
}


longlong Item_func_year::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
  {
    /* got NULL, leave the incl_endp intact */
    return LONGLONG_MIN;
  }

  /*
    Handle the special but practically useful case of datetime values that
    point to year bound ("strictly less" comparison stays intact) :

      col < '2007-01-01 00:00:00'  -> YEAR(col) <  2007

    which is different from the general case ("strictly less" changes to
    "less or equal"):

      col < '2007-09-15 23:00:00'  -> YEAR(col) <= 2007
  */
  if (!left_endp && ltime.day == 1 && ltime.month == 1 && 
      !(ltime.hour || ltime.minute || ltime.second || ltime.second_part))
    ; /* do nothing */
  else
    *incl_endp= TRUE;
  return ltime.year;
}


longlong Item_timeval_func::val_int()
{
  struct timeval tm;
  return val_timeval(&tm) ? 0 : tm.tv_sec;
}


my_decimal *Item_timeval_func::val_decimal(my_decimal *decimal_value)
{
  struct timeval tm;
  return val_timeval(&tm) ? NULL : timeval2my_decimal(&tm, decimal_value);
}


double Item_timeval_func::val_real()
{  
  struct timeval tm;
  return val_timeval(&tm) ? 0 :
         (double) tm.tv_sec + (double) tm.tv_usec / (double) 1000000;
}


String *Item_timeval_func::val_str(String *str)
{
  struct timeval tm;
  if (val_timeval(&tm) || (null_value= str->alloc(MAX_DATE_STRING_REP_LENGTH)))
    return (String*) 0;
  str->length(my_timeval_to_str(&tm, (char*) str->ptr(), decimals));
  str->set_charset(collation.collation);
  return str;
}


bool Item_func_unix_timestamp::val_timeval(struct timeval *tm)
{
  DBUG_ASSERT(fixed == 1);
  if (arg_count == 0)
  {
    tm->tv_sec= current_thd->query_start();
    tm->tv_usec= 0;
    return false; // no args: null_value is set in constructor and is always 0.
  }
  int warnings= 0;
  return (null_value= args[0]->get_timeval(tm, &warnings));
}


enum_monotonicity_info Item_func_unix_timestamp::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM &&
      (args[0]->field_type() == MYSQL_TYPE_TIMESTAMP))
    return MONOTONIC_INCREASING;
  return NON_MONOTONIC;
}


longlong Item_func_unix_timestamp::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ASSERT(arg_count == 1 &&
              args[0]->type() == Item::FIELD_ITEM &&
              args[0]->field_type() == MYSQL_TYPE_TIMESTAMP);
  /* Leave the incl_endp intact */
  struct timeval tm;
  return val_timeval(&tm) ?  0 : tm.tv_sec;
}


longlong Item_func_time_to_sec::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  longlong seconds;
  if (get_arg0_time(&ltime))
    return 0;
  seconds= ltime.hour * 3600L + ltime.minute * 60 + ltime.second;
  return ltime.neg ? -seconds : seconds;
}


/**
  Convert a string to a interval value.

  To make code easy, allow interval objects without separators.
*/

bool get_interval_value(Item *args, interval_type int_type,
                        String *str_value, INTERVAL *interval)
{
  ulonglong array[5];
  longlong UNINIT_VAR(value);

  memset(interval, 0, sizeof(*interval));
  if (int_type == INTERVAL_SECOND && args->decimals)
  {
    my_decimal decimal_value, *val;
    lldiv_t tmp;
    if (!(val= args->val_decimal(&decimal_value)) ||
        my_decimal2lldiv_t(E_DEC_FATAL_ERROR, val, &tmp))
      return false;
    
    if (tmp.quot >= 0 && tmp.rem >= 0)
    {
      interval->neg= false;
      interval->second= tmp.quot;
      interval->second_part= tmp.rem / 1000;
    }
    else
    {
      interval->neg= true;
      interval->second= -tmp.quot;
      interval->second_part= -tmp.rem / 1000;
    }
    return false;
  }
  else if (int_type <= INTERVAL_MICROSECOND)
  {
    value= args->val_int();
    if (args->null_value)
      return true;
    if (value < 0)
    {
      interval->neg= true;
      value= -value;
    }
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    interval->year= (ulong) value;
    break;
  case INTERVAL_QUARTER:
    interval->month= (ulong)(value*3);
    break;
  case INTERVAL_MONTH:
    interval->month= (ulong) value;
    break;
  case INTERVAL_WEEK:
    interval->day= (ulong)(value*7);
    break;
  case INTERVAL_DAY:
    interval->day= (ulong) value;
    break;
  case INTERVAL_HOUR:
    interval->hour= (ulong) value;
    break;
  case INTERVAL_MINUTE:
    interval->minute=value;
    break;
  case INTERVAL_SECOND:
    interval->second=value;
    break;
  case INTERVAL_MICROSECOND:
    interval->second_part=value;
    break;
  case INTERVAL_YEAR_MONTH:			// Allow YEAR-MONTH YYYYYMM
    if (get_interval_info(args, str_value, &interval->neg, 2, array, false))
      return true;
    interval->year=  (ulong) array[0];
    interval->month= (ulong) array[1];
    break;
  case INTERVAL_DAY_HOUR:
    if (get_interval_info(args, str_value, &interval->neg, 2, array, false))
      return true;
    interval->day=  (ulong) array[0];
    interval->hour= (ulong) array[1];
    break;
  case INTERVAL_DAY_MINUTE:
    if (get_interval_info(args, str_value, &interval->neg, 3, array, false))
      return true;
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    break;
  case INTERVAL_DAY_SECOND:
    if (get_interval_info(args, str_value, &interval->neg, 4, array, false))
      return true;
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    interval->second= array[3];
    break;
  case INTERVAL_HOUR_MINUTE:
    if (get_interval_info(args, str_value, &interval->neg, 2, array, false))
      return true;
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    break;
  case INTERVAL_HOUR_SECOND:
    if (get_interval_info(args, str_value, &interval->neg, 3, array, false))
      return true;
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    interval->second= array[2];
    break;
  case INTERVAL_MINUTE_SECOND:
    if (get_interval_info(args, str_value, &interval->neg, 2, array, false))
      return true;
    interval->minute= array[0];
    interval->second= array[1];
    break;
  case INTERVAL_DAY_MICROSECOND:
    if (get_interval_info(args, str_value, &interval->neg, 5, array, true))
      return true;
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    interval->second= array[3];
    interval->second_part= array[4];
    break;
  case INTERVAL_HOUR_MICROSECOND:
    if (get_interval_info(args, str_value, &interval->neg, 4, array, true))
      return true;
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    interval->second= array[2];
    interval->second_part= array[3];
    break;
  case INTERVAL_MINUTE_MICROSECOND:
    if (get_interval_info(args, str_value, &interval->neg, 3, array, true))
      return true;
    interval->minute= array[0];
    interval->second= array[1];
    interval->second_part= array[2];
    break;
  case INTERVAL_SECOND_MICROSECOND:
    if (get_interval_info(args, str_value, &interval->neg, 2, array, true))
      return true;
    interval->second= array[0];
    interval->second_part= array[1];
    break;
  case INTERVAL_LAST: /* purecov: begin deadcode */
    DBUG_ASSERT(0); 
    break;            /* purecov: end */
  }
  return false;
}


bool Item_func_from_days::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  longlong value=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 1;
  memset(ltime, 0, sizeof(MYSQL_TIME));
  get_date_from_daynr((long) value, &ltime->year, &ltime->month, &ltime->day);

  if ((null_value= (fuzzy_date & TIME_NO_ZERO_DATE) &&
       (ltime->year == 0 || ltime->month == 0 || ltime->day == 0)))
    return TRUE;

  ltime->time_type= MYSQL_TIMESTAMP_DATE;
  return 0;
}


void MYSQL_TIME_cache::set_time(MYSQL_TIME *ltime, uint8 dec_arg)
{
  DBUG_ASSERT(ltime->time_type == MYSQL_TIMESTAMP_TIME);
  time= *ltime;
  time_packed= TIME_to_longlong_time_packed(&time);
  dec= dec_arg;
  reset_string();
}


void MYSQL_TIME_cache::set_date(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(ltime->time_type == MYSQL_TIMESTAMP_DATE);
  time= *ltime;
  time_packed= TIME_to_longlong_date_packed(&time);
  dec= 0;
  reset_string();
}


void MYSQL_TIME_cache::set_datetime(MYSQL_TIME *ltime, uint8 dec_arg)
{
  DBUG_ASSERT(ltime->time_type == MYSQL_TIMESTAMP_DATETIME);
  time= *ltime;
  time_packed= TIME_to_longlong_datetime_packed(&time);
  dec= dec_arg;
  reset_string();
}


void MYSQL_TIME_cache::set_datetime(struct timeval tv, uint8 dec_arg,
                                    Time_zone *tz)
{
  tz->gmt_sec_to_TIME(&time, tv);
  time_packed= TIME_to_longlong_datetime_packed(&time);
  dec= dec_arg;
  reset_string();
}


void MYSQL_TIME_cache::set_date(struct timeval tv, Time_zone *tz)
{
  tz->gmt_sec_to_TIME(&time, (my_time_t) tv.tv_sec);
  time.time_type= MYSQL_TIMESTAMP_DATE;
  /* We don't need to set second_part and neg because they are already 0 */
  time.hour= time.minute= time.second= 0;
  time_packed= TIME_to_longlong_date_packed(&time);
  dec= 0;
  reset_string();
}


void MYSQL_TIME_cache::set_time(struct timeval tv, uint8 dec_arg,
                                Time_zone *tz)
{
  tz->gmt_sec_to_TIME(&time, tv);
  datetime_to_time(&time);
  time_packed= TIME_to_longlong_time_packed(&time);
  dec= dec_arg;
  reset_string();
}


void MYSQL_TIME_cache::cache_string()
{
  DBUG_ASSERT(time.time_type != MYSQL_TIMESTAMP_NONE);
  if (string_length == 0)
    string_length= my_TIME_to_str(&time, string_buff, decimals());
}


const char *MYSQL_TIME_cache::cptr()
{
  cache_string();
  return string_buff;
}


bool MYSQL_TIME_cache::get_date(MYSQL_TIME *ltime, uint fuzzydate) const
{
  int warnings;
  get_TIME(ltime);
  return check_date(ltime, non_zero_date(ltime), fuzzydate, &warnings);
}


String *MYSQL_TIME_cache::val_str(String *str)
{
  cache_string();
  str->set(string_buff, string_length, &my_charset_latin1);
  return str;
}


/* CURDATE() and UTC_DATE() */
void Item_func_curdate::fix_length_and_dec()
{
  THD *thd= current_thd;
  Item_date_func::fix_length_and_dec();
  cached_time.set_date(thd->query_start_timeval_trunc(decimals), time_zone());
}


Time_zone *Item_func_curdate_local::time_zone()
{
  return current_thd->time_zone();
}


Time_zone *Item_func_curdate_utc::time_zone()
{
  return my_tz_UTC;
}


/* CURTIME() and UTC_TIME() */
void Item_func_curtime::fix_length_and_dec()
{
  if (check_precision())
    return;
  THD *thd= current_thd;
  cached_time.set_time(thd->query_start_timeval_trunc(decimals), decimals,
                       time_zone());
  /*
    We use 8 instead of MAX_TIME_WIDTH (which is 10) because:
    - there is no sign 
    - hour is in the 2-digit range
  */
  fix_length_and_dec_and_charset_datetime(8, decimals);
}


Time_zone *Item_func_curtime_local::time_zone()
{
  return current_thd->time_zone();
}


Time_zone *Item_func_curtime_utc::time_zone()
{
  return my_tz_UTC;
}


/* NOW() and UTC_TIMESTAMP () */

void Item_func_now::fix_length_and_dec()
{
  if (check_precision())
    return;
  THD *thd= current_thd;
  cached_time.set_datetime(thd->query_start_timeval_trunc(decimals), decimals,
                           time_zone());
  fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, decimals);
}


void Item_func_now_local::store_in(Field *field)
{
  THD *thd= field->table != NULL ? field->table->in_use : current_thd;
  const timeval tm= thd->query_start_timeval_trunc(field->decimals());
  field->set_notnull();
  return field->store_timestamp(&tm);
}


Time_zone *Item_func_now_local::time_zone()
{
  return current_thd->time_zone();
}


Time_zone *Item_func_now_utc::time_zone()
{
  return my_tz_UTC;
}


type_conversion_status
Item_func_now::save_in_field(Field *to, bool no_conversions)
{
  to->set_notnull();
  return to->store_time(cached_time.get_TIME_ptr(), decimals);
}


/**
    Converts current time in my_time_t to MYSQL_TIME represenatation for local
    time zone. Defines time zone (local) used for whole SYSDATE function.
*/
bool Item_func_sysdate_local::get_date(MYSQL_TIME *now_time,
                                       uint fuzzy_date __attribute__((unused)))
{
  THD *thd= current_thd;
  ulonglong tmp= my_micro_time();
  thd->time_zone()->gmt_sec_to_TIME(now_time,
                                    (my_time_t) (tmp / 1000000));
  if (decimals)
  {
    now_time->second_part= tmp % 1000000;
    my_datetime_trunc(now_time, decimals);
  }
  return false;
}


void Item_func_sysdate_local::fix_length_and_dec()
{
  if (check_precision())
    return;
  fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, decimals);
}


bool Item_func_sec_to_time::get_time(MYSQL_TIME *ltime)
{
  my_decimal tmp, *val= args[0]->val_decimal(&tmp);
  lldiv_t seconds;
  if ((null_value= args[0]->null_value))
    return true;
  if (my_decimal2lldiv_t(0, val, &seconds))
  {
    set_max_time(ltime, val->sign());
    make_truncated_value_warning(ErrConvString(val), MYSQL_TIMESTAMP_TIME);
    return false;
  }
  if (sec_to_time(seconds, ltime))
    make_truncated_value_warning(ErrConvString(val),
                                 MYSQL_TIMESTAMP_TIME);
  return false;
}


void Item_func_date_format::fix_length_and_dec()
{
  THD* thd= current_thd;
  /*
    Must use this_item() in case it's a local SP variable
    (for ->max_length and ->str_value)
  */
  Item *arg1= args[1]->this_item();

  decimals=0;
  const CHARSET_INFO *cs= thd->variables.collation_connection;
  uint32 repertoire= arg1->collation.repertoire;
  if (!thd->variables.lc_time_names->is_ascii)
    repertoire|= MY_REPERTOIRE_EXTENDED;
  collation.set(cs, arg1->collation.derivation, repertoire);
  if (arg1->type() == STRING_ITEM)
  {						// Optimize the normal case
    fixed_length=1;
    max_length= format_length(&arg1->str_value) *
                collation.collation->mbmaxlen;
  }
  else
  {
    fixed_length=0;
    max_length= min<uint32>(arg1->max_length, MAX_BLOB_WIDTH) * 10 *
      collation.collation->mbmaxlen;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null=1;					// If wrong date
}


bool Item_func_date_format::eq(const Item *item, bool binary_cmp) const
{
  Item_func_date_format *item_func;

  if (item->type() != FUNC_ITEM)
    return 0;
  if (func_name() != ((Item_func*) item)->func_name())
    return 0;
  if (this == item)
    return 1;
  item_func= (Item_func_date_format*) item;
  if (!args[0]->eq(item_func->args[0], binary_cmp))
    return 0;
  /*
    We must compare format string case sensitive.
    This needed because format modifiers with different case,
    for example %m and %M, have different meaning.
  */
  if (!args[1]->eq(item_func->args[1], 1))
    return 0;
  return 1;
}



uint Item_func_date_format::format_length(const String *format)
{
  uint size=0;
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();

  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr == end-1)
      size++;
    else
    {
      switch(*++ptr) {
      case 'M': /* month, textual */
      case 'W': /* day (of the week), textual */
	size += 64; /* large for UTF8 locale data */
	break;
      case 'D': /* day (of the month), numeric plus english suffix */
      case 'Y': /* year, numeric, 4 digits */
      case 'x': /* Year, used with 'v' */
      case 'X': /* Year, used with 'v, where week starts with Monday' */
	size += 4;
	break;
      case 'a': /* locale's abbreviated weekday name (Sun..Sat) */
      case 'b': /* locale's abbreviated month name (Jan.Dec) */
	size += 32; /* large for UTF8 locale data */
	break;
      case 'j': /* day of year (001..366) */
	size += 3;
	break;
      case 'U': /* week (00..52) */
      case 'u': /* week (00..52), where week starts with Monday */
      case 'V': /* week 1..53 used with 'x' */
      case 'v': /* week 1..53 used with 'x', where week starts with Monday */
      case 'y': /* year, numeric, 2 digits */
      case 'm': /* month, numeric */
      case 'd': /* day (of the month), numeric */
      case 'h': /* hour (01..12) */
      case 'I': /* --||-- */
      case 'i': /* minutes, numeric */
      case 'l': /* hour ( 1..12) */
      case 'p': /* locale's AM or PM */
      case 'S': /* second (00..61) */
      case 's': /* seconds, numeric */
      case 'c': /* month (0..12) */
      case 'e': /* day (0..31) */
	size += 2;
	break;
      case 'k': /* hour ( 0..23) */
      case 'H': /* hour (00..23; value > 23 OK, padding always 2-digit) */
	size += 7; /* docs allow > 23, range depends on sizeof(unsigned int) */
	break;
      case 'r': /* time, 12-hour (hh:mm:ss [AP]M) */
	size += 11;
	break;
      case 'T': /* time, 24-hour (hh:mm:ss) */
	size += 8;
	break;
      case 'f': /* microseconds */
	size += 6;
	break;
      case 'w': /* day (of the week), numeric */
      case '%':
      default:
	size++;
	break;
      }
    }
  }
  return size;
}


String *Item_func_date_format::val_str(String *str)
{
  String *format;
  MYSQL_TIME l_time;
  uint size;
  DBUG_ASSERT(fixed == 1);

  if (!is_time_format)
  {
    if (get_arg0_date(&l_time, TIME_FUZZY_DATE))
      return 0;
  }
  else
  {
    if (get_arg0_time(&l_time))
      return 0;
    l_time.year=l_time.month=l_time.day=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
    goto null_date;

  if (fixed_length)
    size=max_length;
  else
    size=format_length(format);

  if (size < MAX_DATE_STRING_REP_LENGTH)
    size= MAX_DATE_STRING_REP_LENGTH;

  if (format == str)
    str= &value;				// Save result here
  if (str->alloc(size))
    goto null_date;

  DATE_TIME_FORMAT date_time_format;
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length(); 

  /* Create the result string */
  str->set_charset(collation.collation);
  if (!make_date_time(&date_time_format, &l_time,
                      is_time_format ? MYSQL_TIMESTAMP_TIME :
                                       MYSQL_TIMESTAMP_DATE,
                      str))
    return str;

null_date:
  null_value=1;
  return 0;
}


void Item_func_from_unixtime::fix_length_and_dec()
{ 
  thd= current_thd;
  uint8 dec= MY_MIN(args[0]->decimals, DATETIME_MAX_DECIMALS);
  fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, dec);
  maybe_null= 1;
  thd->time_zone_used= 1;
}


bool Item_func_from_unixtime::get_date(MYSQL_TIME *ltime,
				       uint fuzzy_date __attribute__((unused)))
{
  lldiv_t lld;
  if (decimals)
  {
    my_decimal *val, decimal_value;
    if (!(val= args[0]->val_decimal(&decimal_value)) ||
        my_decimal2lldiv_t(E_DEC_FATAL_ERROR, val, &lld))
    {
      null_value= 1;
      return true;
    }
  }
  else
  {
    lld.quot= args[0]->val_int();
    lld.rem= 0;
  }

  if ((null_value= (args[0]->null_value ||
      lld.quot > TIMESTAMP_MAX_VALUE) || lld.quot < 0 || lld.rem < 0))
    return 1;

  thd->variables.time_zone->gmt_sec_to_TIME(ltime, (my_time_t) lld.quot);
  int warnings= 0;
  ltime->second_part= decimals ? lld.rem / 1000 : 0;
  return datetime_add_nanoseconds_with_round(ltime, lld.rem % 1000, &warnings);
}


void Item_func_convert_tz::fix_length_and_dec()
{
  fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, 
                                          args[0]->datetime_precision());
  maybe_null= 1;
}


bool Item_func_convert_tz::get_date(MYSQL_TIME *ltime,
                                    uint fuzzy_date __attribute__((unused)))
{
  my_time_t my_time_tmp;
  String str;
  THD *thd= current_thd;

  if (!from_tz_cached)
  {
    from_tz= my_tz_find(thd, args[1]->val_str_ascii(&str));
    from_tz_cached= args[1]->const_item();
  }

  if (!to_tz_cached)
  {
    to_tz= my_tz_find(thd, args[2]->val_str_ascii(&str));
    to_tz_cached= args[2]->const_item();
  }

  if (from_tz==0 || to_tz==0 || get_arg0_date(ltime, TIME_NO_ZERO_DATE))
  {
    null_value= 1;
    return 1;
  }

  {
    my_bool not_used;
    uint second_part= ltime->second_part;
    my_time_tmp= from_tz->TIME_to_gmt_sec(ltime, &not_used);
    /* my_time_tmp is guranteed to be in the allowed range */
    if (my_time_tmp)
    {
      to_tz->gmt_sec_to_TIME(ltime, my_time_tmp);
      ltime->second_part= second_part;
    }
  }

  null_value= 0;
  return 0;
}


void Item_func_convert_tz::cleanup()
{
  from_tz_cached= to_tz_cached= 0;
  Item_datetime_func::cleanup();
}


void Item_date_add_interval::fix_length_and_dec()
{
  enum_field_types arg0_field_type;

  maybe_null=1;

  /*
    The field type for the result of an Item_date function is defined as
    follows:

    - If first arg is a MYSQL_TYPE_DATETIME result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_DATE and the interval type uses hours,
      minutes or seconds then type is MYSQL_TYPE_DATETIME.
    - Otherwise the result is MYSQL_TYPE_STRING
      (This is because you can't know if the string contains a DATE, MYSQL_TIME or
      DATETIME argument)
  */
  arg0_field_type= args[0]->field_type();
  uint8 interval_dec= 0;
  if (int_type == INTERVAL_MICROSECOND ||
      (int_type >= INTERVAL_DAY_MICROSECOND &&
       int_type <= INTERVAL_SECOND_MICROSECOND))
    interval_dec= DATETIME_MAX_DECIMALS;
  else if (int_type == INTERVAL_SECOND && args[1]->decimals > 0)
    interval_dec= MY_MIN(args[1]->decimals, DATETIME_MAX_DECIMALS);

  if (arg0_field_type == MYSQL_TYPE_DATETIME ||
      arg0_field_type == MYSQL_TYPE_TIMESTAMP)
  {
    uint8 dec= MY_MAX(args[0]->datetime_precision(), interval_dec);
    fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, dec);
    cached_field_type= MYSQL_TYPE_DATETIME;
  }
  else if (arg0_field_type == MYSQL_TYPE_DATE)
  {
    if (int_type <= INTERVAL_DAY || int_type == INTERVAL_YEAR_MONTH)
    {
      cached_field_type= MYSQL_TYPE_DATE;
      fix_length_and_dec_and_charset_datetime(MAX_DATE_WIDTH, 0);
    }
    else
    {
      cached_field_type= MYSQL_TYPE_DATETIME;
      fix_length_and_dec_and_charset_datetime(MAX_DATE_WIDTH, interval_dec);
    }
  }
  else if (arg0_field_type == MYSQL_TYPE_TIME)
  {
    uint8 dec= MY_MAX(args[0]->time_precision(), interval_dec);
    cached_field_type= MYSQL_TYPE_TIME;
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH, dec);
  }
  else
  {
    cached_field_type= MYSQL_TYPE_STRING;
    /* Behave as a usual string function when return type is VARCHAR. */
    fix_length_and_charset(MAX_DATETIME_FULL_WIDTH, default_charset());
  }
  value.alloc(max_length);
}


/* Here arg[1] is a Item_interval object */
bool Item_date_add_interval::get_date_internal(MYSQL_TIME *ltime,
                                               uint fuzzy_date)
{
  INTERVAL interval;

  if (args[0]->get_date(ltime, TIME_NO_ZERO_DATE) ||
      get_interval_value(args[1], int_type, &value, &interval))
    return (null_value= true);

  if (date_sub_interval)
    interval.neg = !interval.neg;

  /*
    Make sure we return proper time_type.
    It's important for val_str().
  */
  if (cached_field_type == MYSQL_TYPE_DATE &&
      ltime->time_type == MYSQL_TIMESTAMP_DATETIME)
    datetime_to_date(ltime);
  else if (cached_field_type == MYSQL_TYPE_DATETIME &&
           ltime->time_type == MYSQL_TIMESTAMP_DATE)
    date_to_datetime(ltime);

  if ((null_value= date_add_interval(ltime, int_type, interval)))
    return true;
  return false;
}


bool Item_date_add_interval::get_time_internal(MYSQL_TIME *ltime)
{
  INTERVAL interval;

  if ((null_value= args[0]->get_time(ltime) ||
                   get_interval_value(args[1], int_type, &value, &interval)))
    return true;

  if (date_sub_interval)
    interval.neg= !interval.neg;

  DBUG_ASSERT(!check_time_range_quick(ltime));

  longlong usec1= ((((ltime->day * 24 + ltime->hour) * 60 +
                      ltime->minute) * 60 + ltime->second) * 1000000LL +
                      ltime->second_part) *
                      (ltime->neg ? -1 : 1);
  longlong usec2= ((((interval.day * 24 + interval.hour) * 60 +
                      interval.minute) * 60 + interval.second) * 1000000LL +
                      interval.second_part) *
                      (interval.neg ? -1 : 1);
  longlong diff= usec1 + usec2;
  lldiv_t seconds;
  seconds.quot= diff / 1000000;
  seconds.rem= diff % 1000000 * 1000; /* time->second_part= lldiv.rem / 1000 */
  if ((null_value= (interval.year || interval.month ||
                    sec_to_time(seconds, ltime))))
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_DATETIME_FUNCTION_OVERFLOW,
                        ER(ER_DATETIME_FUNCTION_OVERFLOW),
                        "time");
    return true;
  }  
  return false;
}


bool Item_date_add_interval::val_datetime(MYSQL_TIME *ltime, uint fuzzy_date)
{
  if (cached_field_type != MYSQL_TYPE_TIME)
    return get_date_internal(ltime, fuzzy_date | TIME_NO_ZERO_DATE);
  return get_time_internal(ltime);
}


bool Item_date_add_interval::eq(const Item *item, bool binary_cmp) const
{
  Item_date_add_interval *other= (Item_date_add_interval*) item;
  if (!Item_func::eq(item, binary_cmp))
    return 0;
  return ((int_type == other->int_type) &&
          (date_sub_interval == other->date_sub_interval));
}

/*
   'interval_names' reflects the order of the enumeration interval_type.
   See item_timefunc.h
 */

static const char *interval_names[]=
{
  "year", "quarter", "month", "week", "day",  
  "hour", "minute", "second", "microsecond",
  "year_month", "day_hour", "day_minute", 
  "day_second", "hour_minute", "hour_second",
  "minute_second", "day_microsecond",
  "hour_microsecond", "minute_microsecond",
  "second_microsecond"
};

void Item_date_add_interval::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(date_sub_interval?" - interval ":" + interval ");
  args[1]->print(str, query_type);
  str->append(' ');
  str->append(interval_names[int_type]);
  str->append(')');
}

void Item_extract::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("extract("));
  str->append(interval_names[int_type]);
  str->append(STRING_WITH_LEN(" from "));
  args[0]->print(str, query_type);
  str->append(')');
}

void Item_extract::fix_length_and_dec()
{
  maybe_null=1;					// If wrong date
  switch (int_type) {
  case INTERVAL_YEAR:		max_length=4; date_value=1; break;
  case INTERVAL_YEAR_MONTH:	max_length=6; date_value=1; break;
  case INTERVAL_QUARTER:        max_length=2; date_value=1; break;
  case INTERVAL_MONTH:		max_length=2; date_value=1; break;
  case INTERVAL_WEEK:		max_length=2; date_value=1; break;
  case INTERVAL_DAY:		max_length=2; date_value=1; break;
  case INTERVAL_DAY_HOUR:	max_length=9; date_value=0; break;
  case INTERVAL_DAY_MINUTE:	max_length=11; date_value=0; break;
  case INTERVAL_DAY_SECOND:	max_length=13; date_value=0; break;
  case INTERVAL_HOUR:		max_length=2; date_value=0; break;
  case INTERVAL_HOUR_MINUTE:	max_length=4; date_value=0; break;
  case INTERVAL_HOUR_SECOND:	max_length=6; date_value=0; break;
  case INTERVAL_MINUTE:		max_length=2; date_value=0; break;
  case INTERVAL_MINUTE_SECOND:	max_length=4; date_value=0; break;
  case INTERVAL_SECOND:		max_length=2; date_value=0; break;
  case INTERVAL_MICROSECOND:	max_length=2; date_value=0; break;
  case INTERVAL_DAY_MICROSECOND: max_length=20; date_value=0; break;
  case INTERVAL_HOUR_MICROSECOND: max_length=13; date_value=0; break;
  case INTERVAL_MINUTE_MICROSECOND: max_length=11; date_value=0; break;
  case INTERVAL_SECOND_MICROSECOND: max_length=9; date_value=0; break;
  case INTERVAL_LAST: DBUG_ASSERT(0); break; /* purecov: deadcode */
  }
}


longlong Item_extract::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  uint year;
  ulong week_format;
  long neg;
  if (date_value)
  {
    if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
      return 0;
    neg=1;
  }
  else
  {
    if (get_arg0_time(&ltime))
      return 0;
    neg= ltime.neg ? -1 : 1;
  }
  switch (int_type) {
  case INTERVAL_YEAR:		return ltime.year;
  case INTERVAL_YEAR_MONTH:	return ltime.year*100L+ltime.month;
  case INTERVAL_QUARTER:	return (ltime.month+2)/3;
  case INTERVAL_MONTH:		return ltime.month;
  case INTERVAL_WEEK:
  {
    week_format= current_thd->variables.default_week_format;
    return calc_week(&ltime, week_mode(week_format), &year);
  }
  case INTERVAL_DAY:		return ltime.day;
  case INTERVAL_DAY_HOUR:	return (long) (ltime.day*100L+ltime.hour)*neg;
  case INTERVAL_DAY_MINUTE:	return (long) (ltime.day*10000L+
					       ltime.hour*100L+
					       ltime.minute)*neg;
  case INTERVAL_DAY_SECOND:	 return ((longlong) ltime.day*1000000L+
					 (longlong) (ltime.hour*10000L+
						     ltime.minute*100+
						     ltime.second))*neg;
  case INTERVAL_HOUR:		return (long) ltime.hour*neg;
  case INTERVAL_HOUR_MINUTE:	return (long) (ltime.hour*100+ltime.minute)*neg;
  case INTERVAL_HOUR_SECOND:	return (long) (ltime.hour*10000+ltime.minute*100+
					       ltime.second)*neg;
  case INTERVAL_MINUTE:		return (long) ltime.minute*neg;
  case INTERVAL_MINUTE_SECOND:	return (long) (ltime.minute*100+ltime.second)*neg;
  case INTERVAL_SECOND:		return (long) ltime.second*neg;
  case INTERVAL_MICROSECOND:	return (long) ltime.second_part*neg;
  case INTERVAL_DAY_MICROSECOND: return (((longlong)ltime.day*1000000L +
					  (longlong)ltime.hour*10000L +
					  ltime.minute*100 +
					  ltime.second)*1000000L +
					 ltime.second_part)*neg;
  case INTERVAL_HOUR_MICROSECOND: return (((longlong)ltime.hour*10000L +
					   ltime.minute*100 +
					   ltime.second)*1000000L +
					  ltime.second_part)*neg;
  case INTERVAL_MINUTE_MICROSECOND: return (((longlong)(ltime.minute*100+
							ltime.second))*1000000L+
					    ltime.second_part)*neg;
  case INTERVAL_SECOND_MICROSECOND: return ((longlong)ltime.second*1000000L+
					    ltime.second_part)*neg;
  case INTERVAL_LAST: DBUG_ASSERT(0); break;  /* purecov: deadcode */
  }
  return 0;					// Impossible
}

bool Item_extract::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      functype() != ((Item_func*)item)->functype())
    return 0;

  Item_extract* ie= (Item_extract*)item;
  if (ie->int_type != int_type)
    return 0;

  if (!args[0]->eq(ie->args[0], binary_cmp))
      return 0;
  return 1;
}


void Item_datetime_typecast::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  if (decimals)
    str->append_parenthesized(decimals);
  str->append(')');
}


bool Item_datetime_typecast::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  if ((null_value= args[0]->get_date(ltime, fuzzy_date | TIME_NO_DATE_FRAC_WARN)))
    return true;
  DBUG_ASSERT(ltime->time_type != MYSQL_TIMESTAMP_TIME);
  ltime->time_type= MYSQL_TIMESTAMP_DATETIME; // In case it was DATE
  int warnings= 0;
  return (null_value= my_datetime_round(ltime, decimals, &warnings));
}


void Item_time_typecast::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  if (decimals)
    str->append_parenthesized(decimals);
  str->append(')');
}


bool Item_time_typecast::get_time(MYSQL_TIME *ltime)
{
  if (get_arg0_time(ltime))
    return true;
  my_time_round(ltime, decimals);
  /*
    For MYSQL_TIMESTAMP_TIME value we can have non-zero day part,
    which we should not lose.
  */
  if (ltime->time_type != MYSQL_TIMESTAMP_TIME)
    datetime_to_time(ltime);
  return false;
}


void Item_date_typecast::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  str->append(')');
}


bool Item_date_typecast::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  bool res= get_arg0_date(ltime, fuzzy_date | TIME_NO_DATE_FRAC_WARN);
  ltime->hour= ltime->minute= ltime->second= ltime->second_part= 0;
  ltime->time_type= MYSQL_TIMESTAMP_DATE;
  return res;
}


/**
  MAKEDATE(a,b) is a date function that creates a date value 
  from a year and day value.

  NOTES:
    As arguments are integers, we can't know if the year is a 2 digit or 4 digit year.
    In this case we treat all years < 100 as 2 digit years. Ie, this is not safe
    for dates between 0000-01-01 and 0099-12-31
*/

bool Item_func_makedate::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  DBUG_ASSERT(fixed == 1);
  long daynr=  (long) args[1]->val_int();
  long year= (long) args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      year < 0 || year > 9999 || daynr <= 0)
    goto err;

  if (year < 100)
    year= year_2000_handling(year);

  days= calc_daynr(year, 1, 1) + daynr - 1;
  /* Day number from year 0 to 9999-12-31 */
  if (days >= 0 && days <= MAX_DAY_NUMBER)
  {
    null_value= 0;
    get_date_from_daynr(days, &ltime->year, &ltime->month, &ltime->day);
    ltime->neg= 0;
    ltime->hour= ltime->minute= ltime->second= ltime->second_part= 0;
    ltime->time_type= MYSQL_TIMESTAMP_DATE;
    return false;
  }

err:
  null_value= 1;
  return true;
}


void Item_func_add_time::fix_length_and_dec()
{
  /*
    The field type for the result of an Item_func_add_time function is defined
    as follows:

    - If first arg is a MYSQL_TYPE_DATETIME or MYSQL_TYPE_TIMESTAMP 
      result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_TIME result is MYSQL_TYPE_TIME
    - Otherwise the result is MYSQL_TYPE_STRING
    
    TODO: perhaps it should also return MYSQL_TYPE_DATETIME
    when the first argument is MYSQL_TYPE_DATE.
  */
  if (args[0]->field_type() == MYSQL_TYPE_TIME && !is_date)
  {
    cached_field_type= MYSQL_TYPE_TIME;
    uint8 dec= MY_MAX(args[0]->time_precision(), args[1]->time_precision());
    fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH, dec);
  }
  else if (args[0]->is_temporal_with_date_and_time() || is_date)
  {
    cached_field_type= MYSQL_TYPE_DATETIME;
    uint8 dec= MY_MAX(args[0]->datetime_precision(), args[1]->time_precision());
    fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, dec);
  }
  else
  {
    cached_field_type= MYSQL_TYPE_STRING;
    fix_length_and_charset(MAX_DATETIME_FULL_WIDTH, default_charset());
  }
  maybe_null= 1;
}


/**
  ADDTIME(t,a) and SUBTIME(t,a) are time functions that calculate a
  time/datetime value 

  t: time_or_datetime_expression
  a: time_expression
  
  @retval 0 on success
  @retval 1 on error
*/

bool Item_func_add_time::val_datetime(MYSQL_TIME *time, uint fuzzy_date)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME l_time1, l_time2;
  bool is_time= 0;
  long days, microseconds;
  longlong seconds;
  int l_sign= sign;

  null_value=0;
  if (cached_field_type == MYSQL_TYPE_DATETIME)  // TIMESTAMP function
  {
    if (get_arg0_date(&l_time1, fuzzy_date) || 
        args[1]->get_time(&l_time2) ||
        l_time1.time_type == MYSQL_TIMESTAMP_TIME || 
        l_time2.time_type != MYSQL_TIMESTAMP_TIME)
      goto null_date;
  }
  else                                // ADDTIME function
  {
    if (args[0]->get_time(&l_time1) || 
        args[1]->get_time(&l_time2) ||
        l_time2.time_type == MYSQL_TIMESTAMP_DATETIME)
      goto null_date;
    is_time= (l_time1.time_type == MYSQL_TIMESTAMP_TIME);
  }
  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;
  
  memset(time, 0, sizeof(MYSQL_TIME));
  
  time->neg= calc_time_diff(&l_time1, &l_time2, -l_sign,
                            &seconds, &microseconds);

  /*
    If first argument was negative and diff between arguments
    is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    time->neg= 1 - time->neg;         // Swap sign of result

  if (!is_time && time->neg)
    goto null_date;

  days= (long) (seconds / SECONDS_IN_24H);

  calc_time_from_sec(time, seconds % SECONDS_IN_24H, microseconds);

  if (!is_time)
  {
    get_date_from_daynr(days, &time->year, &time->month, &time->day);
    time->time_type= MYSQL_TIMESTAMP_DATETIME;
    if (time->day)
      return false;
    goto null_date;
  }
  time->time_type= MYSQL_TIMESTAMP_TIME;
  time->hour+= days*24;
  adjust_time_range_with_warn(time, 0);
  return false;

null_date:
  null_value= 1;
  return true;
}


void Item_func_add_time::print(String *str, enum_query_type query_type)
{
  if (is_date)
  {
    DBUG_ASSERT(sign > 0);
    str->append(STRING_WITH_LEN("timestamp("));
  }
  else
  {
    if (sign > 0)
      str->append(STRING_WITH_LEN("addtime("));
    else
      str->append(STRING_WITH_LEN("subtime("));
  }
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  str->append(')');
}


/**
  TIMEDIFF(t,s) is a time function that calculates the 
  time value between a start and end time.

  t and s: time_or_datetime_expression
  @param  l_time3[OUT]   Result is stored here.
  @param  flags[IN]      Not used in this class.

  @returns
  @retval   false  On succes
  @retval   true   On error
*/

bool Item_func_timediff::get_time(MYSQL_TIME *l_time3)
{
  DBUG_ASSERT(fixed == 1);
  longlong seconds;
  long microseconds;
  int l_sign= 1;
  MYSQL_TIME l_time1, l_time2;

  null_value= 0;

  if ((args[0]->is_temporal_with_date() &&
       args[1]->field_type() == MYSQL_TYPE_TIME) ||
      (args[1]->is_temporal_with_date() &&
       args[0]->field_type() == MYSQL_TYPE_TIME))
    goto null_date; // Incompatible types

  if (args[0]->is_temporal_with_date() ||
      args[1]->is_temporal_with_date())
  {
    if (args[0]->get_date(&l_time1, TIME_FUZZY_DATE) ||
        args[1]->get_date(&l_time2, TIME_FUZZY_DATE))
      goto null_date;
  }
  else
  {
    if (args[0]->get_time(&l_time1) ||
        args[1]->get_time(&l_time2))
      goto null_date;
  }

  if (l_time1.time_type != l_time2.time_type)
    goto null_date; // Incompatible types

  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  memset(l_time3, 0, sizeof(*l_time3));
  
  l_time3->neg= calc_time_diff(&l_time1, &l_time2, l_sign,
                               &seconds, &microseconds);

  /*
    For MYSQL_TIMESTAMP_TIME only:
      If first argument was negative and diff between arguments
      is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    l_time3->neg= 1 - l_time3->neg;         // Swap sign of result

  calc_time_from_sec(l_time3, seconds, microseconds);
  adjust_time_range_with_warn(l_time3, decimals);
  return false;

null_date:
  return (null_value= 1);
}


/**
  MAKETIME(h,m,s) is a time function that calculates a time value 
  from the total number of hours, minutes, and seconds.
  Result: Time value
*/

bool Item_func_maketime::get_time(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  bool overflow= 0;
  longlong hour=   args[0]->val_int();
  longlong minute= args[1]->val_int();
  my_decimal tmp, *sec= args[2]->val_decimal(&tmp);
  lldiv_t second;

  if ((null_value= (args[0]->null_value || 
                    args[1]->null_value ||
                    args[2]->null_value ||
                    my_decimal2lldiv_t(E_DEC_FATAL_ERROR, sec, &second) ||
                    minute < 0 || minute > 59 ||
                    second.quot < 0 || second.quot > 59 || second.rem < 0)))
    return true;

  set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);

  /* Check for integer overflows */
  if (hour < 0)
  {
    if (args[0]->unsigned_flag)
      overflow= 1;
    else
      ltime->neg= 1;
  }
  if (-hour > UINT_MAX || hour > UINT_MAX)
    overflow= 1;

  if (!overflow)
  {
    ltime->hour=   (uint) ((hour < 0 ? -hour : hour));
    ltime->minute= (uint) minute;
    ltime->second= (uint) second.quot;
    int warnings= 0;
    ltime->second_part= second.rem / 1000;
    adjust_time_range_with_warn(ltime, decimals);
    time_add_nanoseconds_with_round(ltime, second.rem % 1000, &warnings);
    if (!warnings)
      return false;
  }

  // Return maximum value (positive or negative)
  set_max_hhmmss(ltime);
  char buf[MAX_BIGINT_WIDTH /* hh */ + 6 /* :mm:ss */ + 10 /* .fffffffff */ +1];
  char *ptr= longlong10_to_str(hour, buf, args[0]->unsigned_flag ? 10 : -10);
  int len = (int)(ptr - buf) +
    sprintf(ptr, ":%02u:%02u", (uint) minute, (uint) second.quot);
  if (second.rem)
  {
    /*
      Display fractional part up to nanoseconds (9 digits),
      which is the maximum precision of my_decimal2lldiv_t().
    */
    int dec= MY_MIN(args[2]->decimals, 9);
    len+= sprintf(buf + len, ".%0*lld", dec,
                  second.rem / (ulong) log_10_int[9 - dec]);
  }
  DBUG_ASSERT(strlen(buf) < sizeof(buf));
  make_truncated_value_warning(ErrConvString(buf, len), MYSQL_TIMESTAMP_TIME);
  return false;
}


/**
  MICROSECOND(a) is a function ( extraction) that extracts the microseconds
  from a.

  a: Datetime or time value
  Result: int value
*/

longlong Item_func_microsecond::val_int()
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  return get_arg0_time(&ltime) ? 0 : ltime.second_part;
}


longlong Item_func_timestamp_diff::val_int()
{
  MYSQL_TIME ltime1, ltime2;
  longlong seconds;
  long microseconds;
  long months= 0;
  int neg= 1;

  null_value= 0;  
  if (args[0]->get_date(&ltime1, TIME_NO_ZERO_DATE) ||
      args[1]->get_date(&ltime2, TIME_NO_ZERO_DATE))
    goto null_date;

  if (calc_time_diff(&ltime2,&ltime1, 1,
		     &seconds, &microseconds))
    neg= -1;

  if (int_type == INTERVAL_YEAR ||
      int_type == INTERVAL_QUARTER ||
      int_type == INTERVAL_MONTH)
  {
    uint year_beg, year_end, month_beg, month_end, day_beg, day_end;
    uint years= 0;
    uint second_beg, second_end, microsecond_beg, microsecond_end;

    if (neg == -1)
    {
      year_beg= ltime2.year;
      year_end= ltime1.year;
      month_beg= ltime2.month;
      month_end= ltime1.month;
      day_beg= ltime2.day;
      day_end= ltime1.day;
      second_beg= ltime2.hour * 3600 + ltime2.minute * 60 + ltime2.second;
      second_end= ltime1.hour * 3600 + ltime1.minute * 60 + ltime1.second;
      microsecond_beg= ltime2.second_part;
      microsecond_end= ltime1.second_part;
    }
    else
    {
      year_beg= ltime1.year;
      year_end= ltime2.year;
      month_beg= ltime1.month;
      month_end= ltime2.month;
      day_beg= ltime1.day;
      day_end= ltime2.day;
      second_beg= ltime1.hour * 3600 + ltime1.minute * 60 + ltime1.second;
      second_end= ltime2.hour * 3600 + ltime2.minute * 60 + ltime2.second;
      microsecond_beg= ltime1.second_part;
      microsecond_end= ltime2.second_part;
    }

    /* calc years */
    years= year_end - year_beg;
    if (month_end < month_beg || (month_end == month_beg && day_end < day_beg))
      years-= 1;

    /* calc months */
    months= 12*years;
    if (month_end < month_beg || (month_end == month_beg && day_end < day_beg))
      months+= 12 - (month_beg - month_end);
    else
      months+= (month_end - month_beg);

    if (day_end < day_beg)
      months-= 1;
    else if ((day_end == day_beg) &&
	     ((second_end < second_beg) ||
	      (second_end == second_beg && microsecond_end < microsecond_beg)))
      months-= 1;
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    return months/12*neg;
  case INTERVAL_QUARTER:
    return months/3*neg;
  case INTERVAL_MONTH:
    return months*neg;
  case INTERVAL_WEEK:          
    return seconds / SECONDS_IN_24H / 7L * neg;
  case INTERVAL_DAY:		
    return seconds / SECONDS_IN_24H * neg;
  case INTERVAL_HOUR:		
    return seconds/3600L*neg;
  case INTERVAL_MINUTE:		
    return seconds/60L*neg;
  case INTERVAL_SECOND:		
    return seconds*neg;
  case INTERVAL_MICROSECOND:
    /*
      In MySQL difference between any two valid datetime values
      in microseconds fits into longlong.
    */
    return (seconds*1000000L+microseconds)*neg;
  default:
    break;
  }

null_date:
  null_value=1;
  return 0;
}


void Item_func_timestamp_diff::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');

  switch (int_type) {
  case INTERVAL_YEAR:
    str->append(STRING_WITH_LEN("YEAR"));
    break;
  case INTERVAL_QUARTER:
    str->append(STRING_WITH_LEN("QUARTER"));
    break;
  case INTERVAL_MONTH:
    str->append(STRING_WITH_LEN("MONTH"));
    break;
  case INTERVAL_WEEK:          
    str->append(STRING_WITH_LEN("WEEK"));
    break;
  case INTERVAL_DAY:		
    str->append(STRING_WITH_LEN("DAY"));
    break;
  case INTERVAL_HOUR:
    str->append(STRING_WITH_LEN("HOUR"));
    break;
  case INTERVAL_MINUTE:		
    str->append(STRING_WITH_LEN("MINUTE"));
    break;
  case INTERVAL_SECOND:
    str->append(STRING_WITH_LEN("SECOND"));
    break;		
  case INTERVAL_MICROSECOND:
    str->append(STRING_WITH_LEN("SECOND_FRAC"));
    break;
  default:
    break;
  }

  for (uint i=0 ; i < 2 ; i++)
  {
    str->append(',');
    args[i]->print(str, query_type);
  }
  str->append(')');
}


String *Item_func_get_format::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  const char *format_name;
  KNOWN_DATE_TIME_FORMAT *format;
  String *val= args[0]->val_str_ascii(str);
  ulong val_len;

  if ((null_value= args[0]->null_value))
    return 0;    

  val_len= val->length();
  for (format= &known_date_time_formats[0];
       (format_name= format->format_name);
       format++)
  {
    uint format_name_len;
    format_name_len= (uint) strlen(format_name);
    if (val_len == format_name_len &&
	!my_strnncoll(&my_charset_latin1, 
		      (const uchar *) val->ptr(), val_len, 
		      (const uchar *) format_name, val_len))
    {
      const char *format_str= get_date_time_format_str(format, type);
      str->set(format_str, (uint) strlen(format_str), &my_charset_numeric);
      return str;
    }
  }

  null_value= 1;
  return 0;
}


void Item_func_get_format::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');

  switch (type) {
  case MYSQL_TIMESTAMP_DATE:
    str->append(STRING_WITH_LEN("DATE, "));
    break;
  case MYSQL_TIMESTAMP_DATETIME:
    str->append(STRING_WITH_LEN("DATETIME, "));
    break;
  case MYSQL_TIMESTAMP_TIME:
    str->append(STRING_WITH_LEN("TIME, "));
    break;
  default:
    DBUG_ASSERT(0);
  }
  args[0]->print(str, query_type);
  str->append(')');
}


/**
  Set type of datetime value (DATE/TIME/...) which will be produced
  according to format string.

  @param format   format string
  @param length   length of format string

  @note
    We don't process day format's characters('D', 'd', 'e') because day
    may be a member of all date/time types.

  @note
    Format specifiers supported by this function should be in sync with
    specifiers supported by extract_date_time() function.
*/
void Item_func_str_to_date::fix_from_format(const char *format, uint length)
{
  const char *time_part_frms= "HISThiklrs";
  const char *date_part_frms= "MVUXYWabcjmvuxyw";
  bool date_part_used= 0, time_part_used= 0, frac_second_used= 0;
  const char *val= format;
  const char *end= format + length;

  for (; val != end && val != end; val++)
  {
    if (*val == '%' && val + 1 != end)
    {
      val++;
      if (*val == 'f')
        frac_second_used= time_part_used= 1;
      else if (!time_part_used && strchr(time_part_frms, *val))
	time_part_used= 1;
      else if (!date_part_used && strchr(date_part_frms, *val))
	date_part_used= 1;
      if (date_part_used && frac_second_used)
      {
        /*
          frac_second_used implies time_part_used, and thus we already
          have all types of date-time components and can end our search.
        */
        cached_timestamp_type= MYSQL_TIMESTAMP_DATETIME;
        cached_field_type= MYSQL_TYPE_DATETIME; 
        fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH,
                                                DATETIME_MAX_DECIMALS);
        return;
      }
    }
  }

  /* We don't have all three types of date-time components */
  if (frac_second_used) /* TIME with microseconds */
  {
    cached_timestamp_type= MYSQL_TIMESTAMP_TIME;
    cached_field_type= MYSQL_TYPE_TIME;
    fix_length_and_dec_and_charset_datetime(MAX_TIME_FULL_WIDTH,
                                            DATETIME_MAX_DECIMALS);
  }
  else if (time_part_used)
  {
    if (date_part_used) /* DATETIME, no microseconds */
    {
      cached_timestamp_type= MYSQL_TIMESTAMP_DATETIME;
      cached_field_type= MYSQL_TYPE_DATETIME; 
      fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH, 0);
    }
    else /* TIME, no microseconds */
    {
      cached_timestamp_type= MYSQL_TIMESTAMP_TIME;
      cached_field_type= MYSQL_TYPE_TIME;
      fix_length_and_dec_and_charset_datetime(MAX_TIME_WIDTH, 0);
    }
  }
  else /* DATE */
  {
    cached_timestamp_type= MYSQL_TIMESTAMP_DATE;
    cached_field_type= MYSQL_TYPE_DATE; 
    fix_length_and_dec_and_charset_datetime(MAX_DATE_WIDTH, 0);
  }
}


void Item_func_str_to_date::fix_length_and_dec()
{
  maybe_null= 1;
  cached_field_type= MYSQL_TYPE_DATETIME;
  cached_timestamp_type= MYSQL_TIMESTAMP_DATETIME;
  fix_length_and_dec_and_charset_datetime(MAX_DATETIME_WIDTH,
                                          DATETIME_MAX_DECIMALS);
  sql_mode= current_thd->datetime_flags();
  if ((const_item= args[1]->const_item()))
  {
    char format_buff[64];
    String format_str(format_buff, sizeof(format_buff), &my_charset_bin);
    String *format= args[1]->val_str(&format_str);
    if (!args[1]->null_value)
      fix_from_format(format->ptr(), format->length());
  }
}


bool Item_func_str_to_date::val_datetime(MYSQL_TIME *ltime, uint fuzzy_date)
{
  DATE_TIME_FORMAT date_time_format;
  char val_buff[64], format_buff[64];
  String val_string(val_buff, sizeof(val_buff), &my_charset_bin), *val;
  String format_str(format_buff, sizeof(format_buff), &my_charset_bin), *format;

  fuzzy_date|= sql_mode;
  val=    args[0]->val_str(&val_string);
  format= args[1]->val_str(&format_str);
  if (args[0]->null_value || args[1]->null_value)
    goto null_date;

  null_value= 0;
  memset(ltime, 0, sizeof(*ltime));
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length();
  if (extract_date_time(&date_time_format, val->ptr(), val->length(),
			ltime, cached_timestamp_type, 0, "datetime") ||
      ((fuzzy_date & TIME_NO_ZERO_DATE) &&
       (ltime->year == 0 || ltime->month == 0 || ltime->day == 0)))
    goto null_date;
  ltime->time_type= cached_timestamp_type;
  if (cached_timestamp_type == MYSQL_TIMESTAMP_TIME && ltime->day)
  {
    /*
      Day part for time type can be nonzero value and so 
      we should add hours from day part to hour part to
      keep valid time value.
    */
    ltime->hour+= ltime->day*24;
    ltime->day= 0;
  }
  return 0;

null_date:
  if (val && (fuzzy_date & TIME_NO_ZERO_DATE) /*warnings*/)
  {
    char buff[128];
    strmake(buff, val->ptr(), min<size_t>(val->length(), sizeof(buff)-1));
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WRONG_VALUE_FOR_TYPE, ER(ER_WRONG_VALUE_FOR_TYPE),
                        "datetime", buff, "str_to_date");
  }
  return (null_value= 1);
}


bool Item_func_last_day::get_date(MYSQL_TIME *ltime, uint fuzzy_date)
{
  if ((null_value= get_arg0_date(ltime, fuzzy_date)))
    return true;

  if (ltime->month == 0)
  {
    /*
      Cannot calculate last day for zero month.
      Let's print a warning and return NULL.
    */
    ltime->time_type= MYSQL_TIMESTAMP_DATE;
    ErrConvString str(ltime, 0);
    make_truncated_value_warning(ErrConvString(str), MYSQL_TIMESTAMP_ERROR);
    return (null_value= true);
  }

  uint month_idx= ltime->month - 1;
  ltime->day= days_in_month[month_idx];
  if (month_idx == 1 && calc_days_in_year(ltime->year) == 366)
    ltime->day= 29;
  datetime_to_date(ltime);
  return false;
}

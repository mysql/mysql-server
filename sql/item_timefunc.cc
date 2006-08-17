/* Copyright (C) 2000-2003 MySQL AB

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


/* This file defines all time functions */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <time.h>

/* TODO: Move month and days to language files */

/* Day number for Dec 31st, 9999 */
#define MAX_DAY_NUMBER 3652424L

/*
  OPTIMIZATION TODO:
   - Replace the switch with a function that should be called for each
     date type.
   - Remove sprintf and opencode the conversion, like we do in
     Field_datetime.

  The reason for this functions existence is that as we don't have a
  way to know if a datetime/time value has microseconds in them
  we are now only adding microseconds to the output if the
  value has microseconds.

  We can't use a standard make_date_time() for this as we don't know
  if someone will use %f in the format specifier in which case we would get
  the microseconds twice.
*/

static bool make_datetime(date_time_format_types format, TIME *ltime,
			  String *str)
{
  char *buff;
  CHARSET_INFO *cs= &my_charset_bin;
  uint length= 30;

  if (str->alloc(length))
    return 1;
  buff= (char*) str->ptr();

  switch (format) {
  case TIME_ONLY:
    length= cs->cset->snprintf(cs, buff, length, "%s%02d:%02d:%02d",
			       ltime->neg ? "-" : "",
			       ltime->hour, ltime->minute, ltime->second);
    break;
  case TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length, "%s%02d:%02d:%02d.%06ld",
			       ltime->neg ? "-" : "",
			       ltime->hour, ltime->minute, ltime->second,
			       ltime->second_part);
    break;
  case DATE_ONLY:
    length= cs->cset->snprintf(cs, buff, length, "%04d-%02d-%02d",
			       ltime->year, ltime->month, ltime->day);
    break;
  case DATE_TIME:
    length= cs->cset->snprintf(cs, buff, length,
			       "%04d-%02d-%02d %02d:%02d:%02d",
			       ltime->year, ltime->month, ltime->day,
			       ltime->hour, ltime->minute, ltime->second);
    break;
  case DATE_TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length,
			       "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
			       ltime->year, ltime->month, ltime->day,
			       ltime->hour, ltime->minute, ltime->second,
			       ltime->second_part);
    break;
  }

  str->length(length);
  str->set_charset(cs);
  return 0;
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

/*
  Extract datetime value to TIME struct from string value
  according to format string. 

  SYNOPSIS
    extract_date_time()
    format		date/time format specification
    val			String to decode
    length		Length of string
    l_time		Store result here
    cached_timestamp_type 
                       It uses to get an appropriate warning
                       in the case when the value is truncated.
    sub_pattern_end    if non-zero then we are parsing string which
                       should correspond compound specifier (like %T or
                       %r) and this parameter is pointer to place where
                       pointer to end of string matching this specifier
                       should be stored.
    NOTE
     Possibility to parse strings matching to patterns equivalent to compound
     specifiers is mainly intended for use from inside of this function in
     order to understand %T and %r conversion specifiers, so number of
     conversion specifiers that can be used in such sub-patterns is limited.
     Also most of checks are skipped in this case.

     If one adds new format specifiers to this function he should also
     consider adding them to get_date_time_result_type() function.

    RETURN
      0	ok
      1	error
*/

static bool extract_date_time(DATE_TIME_FORMAT *format,
			      const char *val, uint length, TIME *l_time,
                              timestamp_type cached_timestamp_type,
                              const char **sub_pattern_end)
{
  int weekday= 0, yearday= 0, daypart= 0;
  int week_number= -1;
  int error= 0;
  int  strict_week_number_year= -1;
  int frac_part;
  bool usa_time= 0;
  bool sunday_first_n_first_week_non_iso;
  bool strict_week_number;
  bool strict_week_number_year_type;
  const char *val_begin= val;
  const char *val_end= val + length;
  const char *ptr= format->format.str;
  const char *end= ptr + format->format.length;
  CHARSET_INFO *cs= &my_charset_bin;
  DBUG_ENTER("extract_date_time");

  LINT_INIT(strict_week_number);
  /* Remove valgrind varnings when using gcc 3.3 and -O1 */
  PURIFY_OR_LINT_INIT(strict_week_number_year_type);
  PURIFY_OR_LINT_INIT(sunday_first_n_first_week_non_iso);

  if (!sub_pattern_end)
    bzero((char*) l_time, sizeof(*l_time));

  for (; ptr != end && val != val_end; ptr++)
  {

    if (*ptr == '%' && ptr+1 != end)
    {
      int val_len;
      char *tmp;

      /* Skip pre-space between each argument */
      while (val != val_end && my_isspace(cs, *val))
	val++;

      val_len= (uint) (val_end - val);
      switch (*++ptr) {
	/* Year */
      case 'Y':
	tmp= (char*) val + min(4, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'y':
	tmp= (char*) val + min(2, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	l_time->year+= (l_time->year < YY_PART_YEAR ? 2000 : 1900);
	break;

	/* Month */
      case 'm':
      case 'c':
	tmp= (char*) val + min(2, val_len);
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
	tmp= (char*) val + min(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'D':
	tmp= (char*) val + min(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	/* Skip 'st, 'nd, 'th .. */
	val= tmp + min((int) (end-tmp), 2);
	break;

	/* Hour */
      case 'h':
      case 'I':
      case 'l':
	usa_time= 1;
	/* fall through */
      case 'k':
      case 'H':
	tmp= (char*) val + min(2, val_len);
	l_time->hour= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Minute */
      case 'i':
	tmp= (char*) val + min(2, val_len);
	l_time->minute= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second */
      case 's':
      case 'S':
	tmp= (char*) val + min(2, val_len);
	l_time->second= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second part */
      case 'f':
	tmp= (char*) val_end;
	if (tmp - val > 6)
	  tmp= (char*) val + 6;
	l_time->second_part= (int) my_strtoll10(val, &tmp, &error);
	frac_part= 6 - (tmp - val);
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
	tmp= (char*) val + min(val_len, 3);
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
	tmp= (char*) val + min(val_len, 2);
	if ((week_number= (int) my_strtoll10(val, &tmp, &error)) < 0 ||
            strict_week_number && !week_number ||
            week_number > 53)
          goto err;
	val= tmp;
	break;

        /* Year used with 'strict' %V and %v week numbers */
      case 'X':
      case 'x':
        strict_week_number_year_type= (*ptr=='X');
        tmp= (char*) val + min(4, val_len);
        strict_week_number_year= (int) my_strtoll10(val, &tmp, &error);
        val= tmp;
        break;

        /* Time in AM/PM notation */
      case 'r':
        error= extract_date_time(&time_ampm_format, val,
                                 (uint)(val_end - val), l_time,
                                 cached_timestamp_type, &val);
        break;

        /* Time in 24-hour notation */
      case 'T':
        error= extract_date_time(&time_24hrs_format, val,
                                 (uint)(val_end - val), l_time,
                                 cached_timestamp_type, &val);
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
    uint days= calc_daynr(l_time->year,1,1) +  yearday - 1;
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
    if (strict_week_number &&
        (strict_week_number_year < 0 ||
         strict_week_number_year_type != sunday_first_n_first_week_non_iso) ||
        !strict_week_number && strict_week_number_year >= 0)
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
	make_truncated_value_warning(current_thd, val_begin, length,
				     cached_timestamp_type);
	break;
      }
    } while (++val != val_end);
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


/*
  Create a formated date/time value in a string
*/

bool make_date_time(DATE_TIME_FORMAT *format, TIME *l_time,
		    timestamp_type type, String *str)
{
  char intbuff[15];
  uint days_i;
  uint hours_i;
  uint weekday;
  ulong length;
  const char *ptr, *end;
  MY_LOCALE *locale;
  THD *thd= current_thd;
  char buf[128];
  String tmp(buf, sizeof(buf), thd->variables.character_set_results);
  uint errors= 0;

  tmp.length(0);
  str->length(0);
  str->set_charset(&my_charset_bin);
  locale = thd->variables.lc_time_names;

  if (l_time->neg)
    str->append("-", 1);
  
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
	tmp.copy(locale->month_names->type_names[l_time->month-1],
		   strlen(locale->month_names->type_names[l_time->month-1]),
		   system_charset_info, tmp.charset(), &errors);
	str->append(tmp.ptr(), tmp.length());
	break;
      case 'b':
	if (!l_time->month)
	  return 1;
	tmp.copy(locale->ab_month_names->type_names[l_time->month-1],
		 strlen(locale->ab_month_names->type_names[l_time->month-1]),
		 system_charset_info, tmp.charset(), &errors);
	str->append(tmp.ptr(), tmp.length());
	break;
      case 'W':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	weekday= calc_weekday(calc_daynr(l_time->year,l_time->month,
					 l_time->day),0);
	tmp.copy(locale->day_names->type_names[weekday],
		 strlen(locale->day_names->type_names[weekday]),
		 system_charset_info, tmp.charset(), &errors);
	str->append(tmp.ptr(), tmp.length());
	break;
      case 'a':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
					l_time->day),0);
	tmp.copy(locale->ab_day_names->type_names[weekday],
		 strlen(locale->ab_day_names->type_names[weekday]),
		 system_charset_info, tmp.charset(), &errors);
	str->append(tmp.ptr(), tmp.length());
	break;
      case 'D':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	if (l_time->day >= 10 &&  l_time->day <= 19)
	  str->append("th", 2);
	else
	{
	  switch (l_time->day %10) {
	  case 1:
	    str->append("st",2);
	    break;
	  case 2:
	    str->append("nd",2);
	    break;
	  case 3:
	    str->append("rd",2);
	    break;
	  default:
	    str->append("th",2);
	    break;
	  }
	}
	break;
      case 'Y':
	length= int10_to_str(l_time->year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
	break;
      case 'y':
	length= int10_to_str(l_time->year%100, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'm':
	length= int10_to_str(l_time->month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'c':
	length= int10_to_str(l_time->month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'd':
	length= int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'e':
	length= int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'f':
	length= int10_to_str(l_time->second_part, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 6, '0');
	break;
      case 'H':
	length= int10_to_str(l_time->hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'h':
      case 'I':
	days_i= l_time->hour/24;
	hours_i= (l_time->hour%24 + 11)%12+1 + 24*days_i;
	length= int10_to_str(hours_i, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'i':					/* minutes */
	length= int10_to_str(l_time->minute, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'j':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= int10_to_str(calc_daynr(l_time->year,l_time->month,
					l_time->day) - 
		     calc_daynr(l_time->year,1,1) + 1, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 3, '0');
	break;
      case 'k':
	length= int10_to_str(l_time->hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'l':
	days_i= l_time->hour/24;
	hours_i= (l_time->hour%24 + 11)%12+1;
	length= int10_to_str(hours_i, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'p':
	hours_i= l_time->hour%24;
	str->append(hours_i < 12 ? "AM" : "PM",2);
	break;
      case 'r':
	length= my_sprintf(intbuff, 
		   (intbuff, 
		    ((l_time->hour % 24) < 12) ?
                    "%02d:%02d:%02d AM" : "%02d:%02d:%02d PM",
		    (l_time->hour+11)%12+1,
		    l_time->minute,
		    l_time->second));
	str->append(intbuff, length);
	break;
      case 'S':
      case 's':
	length= int10_to_str(l_time->second, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'T':
	length= my_sprintf(intbuff, 
		   (intbuff, 
		    "%02d:%02d:%02d", 
		    l_time->hour, 
		    l_time->minute,
		    l_time->second));
	str->append(intbuff, length);
	break;
      case 'U':
      case 'u':
      {
	uint year;
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= int10_to_str(calc_week(l_time,
				       (*ptr) == 'U' ?
				       WEEK_FIRST_WEEKDAY : WEEK_MONDAY_FIRST,
				       &year),
			     intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'v':
      case 'V':
      {
	uint year;
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	length= int10_to_str(calc_week(l_time,
				       ((*ptr) == 'V' ?
					(WEEK_YEAR | WEEK_FIRST_WEEKDAY) :
					(WEEK_YEAR | WEEK_MONDAY_FIRST)),
				       &year),
			     intbuff, 10) - intbuff;
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
	length= int10_to_str(year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
      }
      break;
      case 'w':
	if (type == MYSQL_TIMESTAMP_TIME)
	  return 1;
	weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
					l_time->day),1);
	length= int10_to_str(weekday, intbuff, 10) - intbuff;
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


/*
  Get a array of positive numbers from a string object.
  Each number is separated by 1 non digit character
  Return error if there is too many numbers.
  If there is too few numbers, assume that the numbers are left out
  from the high end. This allows one to give:
  DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.

  SYNOPSIS
    str:            string value
    length:         length of str
    cs:             charset of str
    values:         array of results
    count:          count of elements in result array
    transform_msec: if value is true we suppose
                    that the last part of string value is microseconds
                    and we should transform value to six digit value.
                    For example, '1.1' -> '1.100000'
*/

static bool get_interval_info(const char *str,uint length,CHARSET_INFO *cs,
                              uint count, ulonglong *values,
                              bool transform_msec)
{
  const char *end=str+length;
  uint i;
  while (str != end && !my_isdigit(cs,*str))
    str++;

  for (i=0 ; i < count ; i++)
  {
    longlong value;
    const char *start= str;
    for (value=0; str != end && my_isdigit(cs,*str) ; str++)
      value= value*LL(10) + (longlong) (*str - '0');
    if (transform_msec && i == count - 1) // microseconds always last
    {
      long msec_length= 6 - (str - start);
      if (msec_length > 0)
	value*= (long) log_10_int[msec_length];
    }
    values[i]= value;
    while (str != end && !my_isdigit(cs,*str))
      str++;
    if (str == end && i != count-1)
    {
      i++;
      /* Change values[0...i-1] -> values[0...count-1] */
      bmove_upp((char*) (values+count), (char*) (values+i),
		sizeof(*values)*i);
      bzero((char*) values, sizeof(*values)*(count-i));
      break;
    }
  }
  return (str != end);
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
    signs into account (i.e. for TIME values).

  RETURN VALUES
    Returns sign of difference.
    1 means negative result
    0 means positive result

*/

static bool calc_time_diff(TIME *l_time1, TIME *l_time2, int l_sign,
                           longlong *seconds_out, long *microseconds_out)
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

  microseconds= ((longlong)days*LL(86400) +
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
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day);
}

longlong Item_func_dayofyear::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day) -
    calc_daynr(ltime.year,1,1) + 1;
}

longlong Item_func_dayofmonth::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.day;
}

longlong Item_func_month::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.month;
}


String* Item_func_monthname::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  const char *month_name;
  uint   month= (uint) val_int();
  THD *thd= current_thd;

  if (null_value || !month)
  {
    null_value=1;
    return (String*) 0;
  }
  null_value=0;
  month_name= thd->variables.lc_time_names->month_names->type_names[month-1];
  str->set(month_name, strlen(month_name), system_charset_info);
  return str;
}


// Returns the quarter of the year

longlong Item_func_quarter::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ((ltime.month+2)/3);
}

longlong Item_func_hour::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.hour;
}

longlong Item_func_minute::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.minute;
}
// Returns the second in time_exp in the range of 0 - 59

longlong Item_func_second::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.second;
}


uint week_mode(uint mode)
{
  uint week_format= (mode & 7);
  if (!(week_format & WEEK_MONDAY_FIRST))
    week_format^= WEEK_FIRST_WEEKDAY;
  return week_format;
}

/*
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
*/

longlong Item_func_week::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint year;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_week(&ltime,
			      week_mode((uint) args[1]->val_int()),
			      &year);
}


longlong Item_func_yearweek::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint year,week;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  week= calc_week(&ltime, 
		  (week_mode((uint) args[1]->val_int()) | WEEK_YEAR),
		  &year);
  return week+year*100;
}


/* weekday() has a automatic to_days() on item */

longlong Item_func_weekday::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulong tmp_value=(ulong) args[0]->val_int();
  if ((null_value=(args[0]->null_value || !tmp_value)))
    return 0; /* purecov: inspected */

  return (longlong) calc_weekday(tmp_value,odbc_type)+test(odbc_type);
}


String* Item_func_dayname::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  uint weekday=(uint) val_int();		// Always Item_func_daynr()
  const char *name;
  THD *thd= current_thd;

  if (null_value)
    return (String*) 0;
  
  name= thd->variables.lc_time_names->day_names->type_names[weekday];
  str->set(name, strlen(name), system_charset_info);
  return str;
}


longlong Item_func_year::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.year;
}


longlong Item_func_unix_timestamp::val_int()
{
  TIME ltime;
  bool not_used;
  
  DBUG_ASSERT(fixed == 1);
  if (arg_count == 0)
    return (longlong) current_thd->query_start();
  if (args[0]->type() == FIELD_ITEM)
  {						// Optimize timestamp field
    Field *field=((Item_field*) args[0])->field;
    if (field->type() == FIELD_TYPE_TIMESTAMP)
      return ((Field_timestamp*) field)->get_timestamp(&null_value);
  }
  
  if (get_arg0_date(&ltime, 0))
  {
    /*
      We have to set null_value again because get_arg0_date will also set it
      to true if we have wrong datetime parameter (and we should return 0 in 
      this case).
    */
    null_value= args[0]->null_value;
    return 0;
  }
  
  return (longlong) TIME_to_timestamp(current_thd, &ltime, &not_used);
}


longlong Item_func_time_to_sec::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  longlong seconds;
  (void) get_arg0_time(&ltime);
  seconds=ltime.hour*3600L+ltime.minute*60+ltime.second;
  return ltime.neg ? -seconds : seconds;
}


/*
  Convert a string to a interval value
  To make code easy, allow interval objects without separators.
*/

static bool get_interval_value(Item *args,interval_type int_type,
			       String *str_value, INTERVAL *interval)
{
  ulonglong array[5];
  longlong value;
  const char *str;
  uint32 length;
  CHARSET_INFO *cs=str_value->charset();

  LINT_INIT(value);
  LINT_INIT(str);
  LINT_INIT(length);

  bzero((char*) interval,sizeof(*interval));
  if ((int) int_type <= INTERVAL_MICROSECOND)
  {
    value= args->val_int();
    if (args->null_value)
      return 1;
    if (value < 0)
    {
      interval->neg=1;
      value= -value;
    }
  }
  else
  {
    String *res;
    if (!(res=args->val_str(str_value)))
      return (1);

    /* record negative intervalls in interval->neg */
    str=res->ptr();
    const char *end=str+res->length();
    while (str != end && my_isspace(cs,*str))
      str++;
    if (str != end && *str == '-')
    {
      interval->neg=1;
      str++;
    }
    length=(uint32) (end-str);		// Set up pointers to new str
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    interval->year= (ulong) value;
    break;
  case INTERVAL_MONTH:
    interval->month= (ulong) value;
    break;
  case INTERVAL_DAY:
    interval->day= (ulong) value;
    break;
  case INTERVAL_HOUR:
    interval->hour= (ulong) value;
    break;
  case INTERVAL_MICROSECOND:
    interval->second_part=value;
    break;
  case INTERVAL_MINUTE:
    interval->minute=value;
    break;
  case INTERVAL_SECOND:
    interval->second=value;
    break;
  case INTERVAL_YEAR_MONTH:			// Allow YEAR-MONTH YYYYYMM
    if (get_interval_info(str,length,cs,2,array,0))
      return (1);
    interval->year=  (ulong) array[0];
    interval->month= (ulong) array[1];
    break;
  case INTERVAL_DAY_HOUR:
    if (get_interval_info(str,length,cs,2,array,0))
      return (1);
    interval->day=  (ulong) array[0];
    interval->hour= (ulong) array[1];
    break;
  case INTERVAL_DAY_MICROSECOND:
    if (get_interval_info(str,length,cs,5,array,1))
      return (1);
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    interval->second= array[3];
    interval->second_part= array[4];
    break;
  case INTERVAL_DAY_MINUTE:
    if (get_interval_info(str,length,cs,3,array,0))
      return (1);
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    break;
  case INTERVAL_DAY_SECOND:
    if (get_interval_info(str,length,cs,4,array,0))
      return (1);
    interval->day=    (ulong) array[0];
    interval->hour=   (ulong) array[1];
    interval->minute= array[2];
    interval->second= array[3];
    break;
  case INTERVAL_HOUR_MICROSECOND:
    if (get_interval_info(str,length,cs,4,array,1))
      return (1);
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    interval->second= array[2];
    interval->second_part= array[3];
    break;
  case INTERVAL_HOUR_MINUTE:
    if (get_interval_info(str,length,cs,2,array,0))
      return (1);
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    break;
  case INTERVAL_HOUR_SECOND:
    if (get_interval_info(str,length,cs,3,array,0))
      return (1);
    interval->hour=   (ulong) array[0];
    interval->minute= array[1];
    interval->second= array[2];
    break;
  case INTERVAL_MINUTE_MICROSECOND:
    if (get_interval_info(str,length,cs,3,array,1))
      return (1);
    interval->minute= array[0];
    interval->second= array[1];
    interval->second_part= array[2];
    break;
  case INTERVAL_MINUTE_SECOND:
    if (get_interval_info(str,length,cs,2,array,0))
      return (1);
    interval->minute= array[0];
    interval->second= array[1];
    break;
  case INTERVAL_SECOND_MICROSECOND:
    if (get_interval_info(str,length,cs,2,array,1))
      return (1);
    interval->second= array[0];
    interval->second_part= array[1];
    break;
  }
  return 0;
}


String *Item_date::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE))
    return (String *) 0;
  if (str->alloc(11))
  {
    null_value= 1;
    return (String *) 0;
  }
  make_date((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}


int Item_date::save_in_field(Field *field, bool no_conversions)
{
  TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE))
    return set_field_to_null(field);
  field->set_notnull();
  field->store_time(&ltime, MYSQL_TIMESTAMP_DATE);
  return 0;
}


longlong Item_date::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE))
    return 0;
  return (longlong) (ltime.year*10000L+ltime.month*100+ltime.day);
}


bool Item_func_from_days::get_date(TIME *ltime, uint fuzzy_date)
{
  longlong value=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 1;
  bzero(ltime, sizeof(TIME));
  get_date_from_daynr((long) value, &ltime->year, &ltime->month, &ltime->day);
  ltime->time_type= MYSQL_TIMESTAMP_DATE;
  return 0;
}


void Item_func_curdate::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0; 
  max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;

  store_now_in_TIME(&ltime);
  
  /* We don't need to set second_part and neg because they already 0 */
  ltime.hour= ltime.minute= ltime.second= 0;
  ltime.time_type= MYSQL_TIMESTAMP_DATE;
  value= (longlong) TIME_to_ulonglong_date(&ltime);
}

String *Item_func_curdate::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (str->alloc(11))
  {
    null_value= 1;
    return (String *) 0;
  }
  make_date((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}

/*
    Converts current time in my_time_t to TIME represenatation for local
    time zone. Defines time zone (local) used for whole CURDATE function.
*/
void Item_func_curdate_local::store_now_in_TIME(TIME *now_time)
{
  THD *thd= current_thd;
  thd->variables.time_zone->gmt_sec_to_TIME(now_time, 
                                             (my_time_t)thd->query_start());
  thd->time_zone_used= 1;
}


/*
    Converts current time in my_time_t to TIME represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_DATE function.
*/
void Item_func_curdate_utc::store_now_in_TIME(TIME *now_time)
{
  my_tz_UTC->gmt_sec_to_TIME(now_time, 
                             (my_time_t)(current_thd->query_start()));
  /* 
    We are not flagging this query as using time zone, since it uses fixed
    UTC-SYSTEM time-zone.
  */
}


bool Item_func_curdate::get_date(TIME *res,
				 uint fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


String *Item_func_curtime::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str_value.set(buff, buff_length, &my_charset_bin);
  return &str_value;
}


void Item_func_curtime::fix_length_and_dec()
{
  TIME ltime;

  decimals= DATETIME_DEC;
  collation.set(&my_charset_bin);
  store_now_in_TIME(&ltime);
  value= TIME_to_ulonglong_time(&ltime);
  buff_length= (uint) my_time_to_str(&ltime, buff);
  max_length= buff_length;
}


/*
    Converts current time in my_time_t to TIME represenatation for local
    time zone. Defines time zone (local) used for whole CURTIME function.
*/
void Item_func_curtime_local::store_now_in_TIME(TIME *now_time)
{
  THD *thd= current_thd;
  thd->variables.time_zone->gmt_sec_to_TIME(now_time, 
                                             (my_time_t)thd->query_start());
  thd->time_zone_used= 1;
}


/*
    Converts current time in my_time_t to TIME represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_TIME function.
*/
void Item_func_curtime_utc::store_now_in_TIME(TIME *now_time)
{
  my_tz_UTC->gmt_sec_to_TIME(now_time, 
                             (my_time_t)(current_thd->query_start()));
  /* 
    We are not flagging this query as using time zone, since it uses fixed
    UTC-SYSTEM time-zone.
  */
}


String *Item_func_now::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str_value.set(buff,buff_length, &my_charset_bin);
  return &str_value;
}


void Item_func_now::fix_length_and_dec()
{
  decimals= DATETIME_DEC;
  collation.set(&my_charset_bin);

  store_now_in_TIME(&ltime);
  value= (longlong) TIME_to_ulonglong_datetime(&ltime);

  buff_length= (uint) my_datetime_to_str(&ltime, buff);
  max_length= buff_length;
}


/*
    Converts current time in my_time_t to TIME represenatation for local
    time zone. Defines time zone (local) used for whole NOW function.
*/
void Item_func_now_local::store_now_in_TIME(TIME *now_time)
{
  THD *thd= current_thd;
  thd->variables.time_zone->gmt_sec_to_TIME(now_time, 
                                             (my_time_t)thd->query_start());
  thd->time_zone_used= 1;
}


/*
    Converts current time in my_time_t to TIME represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_TIMESTAMP function.
*/
void Item_func_now_utc::store_now_in_TIME(TIME *now_time)
{
  my_tz_UTC->gmt_sec_to_TIME(now_time, 
                             (my_time_t)(current_thd->query_start()));
  /* 
    We are not flagging this query as using time zone, since it uses fixed
    UTC-SYSTEM time-zone.
  */
}


bool Item_func_now::get_date(TIME *res,
			     uint fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


int Item_func_now::save_in_field(Field *to, bool no_conversions)
{
  to->set_notnull();
  to->store_time(&ltime, MYSQL_TIMESTAMP_DATETIME);
  return 0;
}


String *Item_func_sec_to_time::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong seconds=(longlong) args[0]->val_int();
  uint sec;
  TIME ltime;

  if ((null_value=args[0]->null_value) || str->alloc(19))
  {
    null_value= 1;
    return (String*) 0;
  }

  ltime.neg= 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    ltime.neg= 1;
  }

  sec= (uint) ((ulonglong) seconds % 3600);
  ltime.day= 0;
  ltime.hour= (uint) (seconds/3600);
  ltime.minute= sec/60;
  ltime.second= sec % 60;

  make_time((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}


longlong Item_func_sec_to_time::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong seconds=args[0]->val_int();
  longlong sign=1;
  if ((null_value=args[0]->null_value))
    return 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    sign= -1;
  }
  return sign*((seconds / 3600)*10000+((seconds/60) % 60)*100+ (seconds % 60));
}


void Item_func_date_format::fix_length_and_dec()
{
  decimals=0;
  collation.set(&my_charset_bin);
  if (args[1]->type() == STRING_ITEM)
  {						// Optimize the normal case
    fixed_length=1;

    /*
      Force case sensitive collation on format string.
      This needed because format modifiers with different case,
      for example %m and %M, have different meaning. Thus eq()
      will distinguish them.
    */
    args[1]->collation.set(
        get_charset_by_csname(args[1]->collation.collation->csname,
                              MY_CS_BINSORT,MYF(0)), DERIVATION_COERCIBLE);
    /*
      The result is a binary string (no reason to use collation->mbmaxlen
      This is becasue make_date_time() only returns binary strings
    */
    max_length= format_length(((Item_string*) args[1])->const_string());
  }
  else
  {
    fixed_length=0;
    /* The result is a binary string (no reason to use collation->mbmaxlen */
    max_length=min(args[1]->max_length,MAX_BLOB_WIDTH) * 10;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null=1;					// If wrong date
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
      case 'H': /* hour (00..23) */
      case 'y': /* year, numeric, 2 digits */
      case 'm': /* month, numeric */
      case 'd': /* day (of the month), numeric */
      case 'h': /* hour (01..12) */
      case 'I': /* --||-- */
      case 'i': /* minutes, numeric */
      case 'k': /* hour ( 0..23) */
      case 'l': /* hour ( 1..12) */
      case 'p': /* locale's AM or PM */
      case 'S': /* second (00..61) */
      case 's': /* seconds, numeric */
      case 'c': /* month (0..12) */
      case 'e': /* day (0..31) */
	size += 2;
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
  TIME l_time;
  uint size;
  DBUG_ASSERT(fixed == 1);

  if (!is_time_format)
  {
    if (get_arg0_date(&l_time,1))
      return 0;
  }
  else
  {
    String *res;
    if (!(res=args[0]->val_str(str)) ||
	(str_to_time_with_warn(res->ptr(), res->length(), &l_time)))
      goto null_date;

    l_time.year=l_time.month=l_time.day=0;
    null_value=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
    goto null_date;

  if (fixed_length)
    size=max_length;
  else
    size=format_length(format);
  if (format == str)
    str= &value;				// Save result here
  if (str->alloc(size))
    goto null_date;

  DATE_TIME_FORMAT date_time_format;
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length(); 

  /* Create the result string */
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
  collation.set(&my_charset_bin);
  decimals= DATETIME_DEC;
  max_length=MAX_DATETIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;
  thd->time_zone_used= 1;
}


String *Item_func_from_unixtime::val_str(String *str)
{
  TIME time_tmp;

  DBUG_ASSERT(fixed == 1);

  if (get_date(&time_tmp, 0))
    return 0;

  if (str->alloc(20*MY_CHARSET_BIN_MB_MAXLEN))
  {
    null_value= 1;
    return 0;
  }

  make_datetime((DATE_TIME_FORMAT *) 0, &time_tmp, str);
  return str;
}


longlong Item_func_from_unixtime::val_int()
{
  TIME time_tmp;

  DBUG_ASSERT(fixed == 1);

  if (get_date(&time_tmp, 0))
    return 0;

  return (longlong) TIME_to_ulonglong_datetime(&time_tmp);
}

bool Item_func_from_unixtime::get_date(TIME *ltime,
				       uint fuzzy_date __attribute__((unused)))
{
  ulonglong tmp= (ulonglong)(args[0]->val_int());
  /*
    "tmp > TIMESTAMP_MAX_VALUE" check also covers case of negative
    from_unixtime() argument since tmp is unsigned.
  */
  if ((null_value= (args[0]->null_value || tmp > TIMESTAMP_MAX_VALUE)))
    return 1;

  thd->variables.time_zone->gmt_sec_to_TIME(ltime, (my_time_t)tmp);

  return 0;
}


void Item_func_convert_tz::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals= 0;
  max_length= MAX_DATETIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;
}


bool
Item_func_convert_tz::fix_fields(THD *thd_arg, TABLE_LIST *tables_arg, Item **ref)
{
  String str;
  if (Item_date_func::fix_fields(thd_arg, tables_arg, ref))
    return 1;

  tz_tables= thd_arg->lex->time_zone_tables_used;

  return 0;
}


String *Item_func_convert_tz::val_str(String *str)
{
  TIME time_tmp;

  if (get_date(&time_tmp, 0))
    return 0;
  
  if (str->alloc(20*MY_CHARSET_BIN_MB_MAXLEN))
  {
    null_value= 1;
    return 0;
  }
  
  make_datetime((DATE_TIME_FORMAT *) 0, &time_tmp, str);
  return str;
}


longlong Item_func_convert_tz::val_int()
{
  TIME time_tmp;

  if (get_date(&time_tmp, 0))
    return 0;
  
  return (longlong)TIME_to_ulonglong_datetime(&time_tmp);
}


bool Item_func_convert_tz::get_date(TIME *ltime,
				       uint fuzzy_date __attribute__((unused)))
{
  my_time_t my_time_tmp;
  bool not_used;
  String str;

  if (!from_tz_cached)
  {
    from_tz= my_tz_find(args[1]->val_str(&str), tz_tables);
    from_tz_cached= args[1]->const_item();
  }

  if (!to_tz_cached)
  {
    to_tz= my_tz_find(args[2]->val_str(&str), tz_tables);
    to_tz_cached= args[2]->const_item();
  }

  if (from_tz==0 || to_tz==0 || get_arg0_date(ltime, 0))
  {
    null_value= 1;
    return 1;
  }

  /* Check if we in range where we treat datetime values as non-UTC */
  if (ltime->year < TIMESTAMP_MAX_YEAR && ltime->year > TIMESTAMP_MIN_YEAR ||
      ltime->year==TIMESTAMP_MAX_YEAR && ltime->month==1 && ltime->day==1 ||
      ltime->year==TIMESTAMP_MIN_YEAR && ltime->month==12 && ltime->day==31)
  {
    my_time_tmp= from_tz->TIME_to_gmt_sec(ltime, &not_used);
    if (my_time_tmp >= TIMESTAMP_MIN_VALUE && my_time_tmp <= TIMESTAMP_MAX_VALUE)
      to_tz->gmt_sec_to_TIME(ltime, my_time_tmp);
  }
  
  null_value= 0;
  return 0;
}


void Item_func_convert_tz::cleanup()
{
  from_tz_cached= to_tz_cached= 0;
  Item_date_func::cleanup();
}


void Item_date_add_interval::fix_length_and_dec()
{
  enum_field_types arg0_field_type;

  collation.set(&my_charset_bin);
  maybe_null=1;
  max_length=MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  value.alloc(max_length);

  /*
    The field type for the result of an Item_date function is defined as
    follows:

    - If first arg is a MYSQL_TYPE_DATETIME result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_DATE and the interval type uses hours,
      minutes or seconds then type is MYSQL_TYPE_DATETIME.
    - Otherwise the result is MYSQL_TYPE_STRING
      (This is because you can't know if the string contains a DATE, TIME or
      DATETIME argument)
  */
  cached_field_type= MYSQL_TYPE_STRING;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == MYSQL_TYPE_DATETIME ||
      arg0_field_type == MYSQL_TYPE_TIMESTAMP)
    cached_field_type= MYSQL_TYPE_DATETIME;
  else if (arg0_field_type == MYSQL_TYPE_DATE)
  {
    if (int_type <= INTERVAL_DAY || int_type == INTERVAL_YEAR_MONTH)
      cached_field_type= arg0_field_type;
    else
      cached_field_type= MYSQL_TYPE_DATETIME;
  }
}


/* Here arg[1] is a Item_interval object */

bool Item_date_add_interval::get_date(TIME *ltime, uint fuzzy_date)
{
  long period,sign;
  INTERVAL interval;

  ltime->neg= 0;
  if (args[0]->get_date(ltime,0) ||
      get_interval_value(args[1],int_type,&value,&interval))
    goto null_date;
  sign= (interval.neg ? -1 : 1);
  if (date_sub_interval)
    sign = -sign;

  null_value=0;
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
      goto null_date;
    get_date_from_daynr((long) daynr, &ltime->year, &ltime->month,
                        &ltime->day);
    break;
  }
  case INTERVAL_DAY:
    period= (calc_daynr(ltime->year,ltime->month,ltime->day) +
             sign * (long) interval.day);
    /* Daynumber from year 0 to 9999-12-31 */
    if ((ulong) period > MAX_DAY_NUMBER)
      goto null_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year+= sign * (long) interval.year;
    if ((ulong) ltime->year >= 10000L)
      goto null_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign * (long) interval.year*12 +
	     ltime->month-1 + sign * (long) interval.month);
    if ((ulong) period >= 120000L)
      goto null_date;
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

 null_date:
  return (null_value=1);
}


String *Item_date_add_interval::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  enum date_time_format_types format;

  if (Item_date_add_interval::get_date(&ltime,0))
    return 0;

  if (ltime.time_type == MYSQL_TIMESTAMP_DATE)
    format= DATE_ONLY;
  else if (ltime.second_part)
    format= DATE_TIME_MICROSECOND;
  else
    format= DATE_TIME;

  if (!make_datetime(format, &ltime, str))
    return str;

  null_value=1;
  return 0;
}


longlong Item_date_add_interval::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  longlong date;
  if (Item_date_add_interval::get_date(&ltime,0))
    return (longlong) 0;
  date = (ltime.year*100L + ltime.month)*100L + ltime.day;
  return ltime.time_type == MYSQL_TIMESTAMP_DATE ? date :
    ((date*100L + ltime.hour)*100L+ ltime.minute)*100L + ltime.second;
}

static const char *interval_names[]=
{
  "year", "month", "day", "hour", "minute",
  "second", "microsecond", "year_month",
  "day_hour", "day_minute", "day_second",
  "hour_minute", "hour_second", "minute_second",
  "day_microsecond", "hour_microsecond",
  "minute_microsecond", "second_microsecond"
};

void Item_date_add_interval::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  str->append(date_sub_interval?" - interval ":" + interval ");
  args[1]->print(str);
  str->append(' ');
  str->append(interval_names[int_type]);
  str->append(')');
}

void Item_extract::print(String *str)
{
  str->append("extract(", 8);
  str->append(interval_names[int_type]);
  str->append(" from ", 6);
  args[0]->print(str);
  str->append(')');
}

void Item_extract::fix_length_and_dec()
{
  value.alloc(32);				// alloc buffer

  maybe_null=1;					// If wrong date
  switch (int_type) {
  case INTERVAL_YEAR:		max_length=4; date_value=1; break;
  case INTERVAL_YEAR_MONTH:	max_length=6; date_value=1; break;
  case INTERVAL_MONTH:		max_length=2; date_value=1; break;
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
  }
}


longlong Item_extract::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  long neg;
  if (date_value)
  {
    if (get_arg0_date(&ltime,1))
      return 0;
    neg=1;
  }
  else
  {
    String *res= args[0]->val_str(&value);
    if (!res || str_to_time_with_warn(res->ptr(), res->length(), &ltime))
    {
      null_value=1;
      return 0;
    }
    neg= ltime.neg ? -1 : 1;
    null_value=0;
  }
  switch (int_type) {
  case INTERVAL_YEAR:		return ltime.year;
  case INTERVAL_YEAR_MONTH:	return ltime.year*100L+ltime.month;
  case INTERVAL_MONTH:		return ltime.month;
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
  }
  return 0;					// Impossible
}

bool Item_extract::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_extract* ie= (Item_extract*)item;
  if (ie->int_type != int_type)
    return 0;

  if (!args[0]->eq(ie->args[0], binary_cmp))
      return 0;
  return 1;
}


bool Item_char_typecast::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_char_typecast *cast= (Item_char_typecast*)item;
  if (cast_length != cast->cast_length ||
      cast_cs     != cast->cast_cs)
    return 0;

  if (!args[0]->eq(cast->args[0], binary_cmp))
      return 0;
  return 1;
}

void Item_typecast::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as ", 4);
  str->append(cast_type());
  str->append(')');
}


void Item_char_typecast::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as char", 8);
  if (cast_length >= 0)
  {
    str->append('(');
    char buffer[20];
    // my_charset_bin is good enough for numbers
    String st(buffer, sizeof(buffer), &my_charset_bin);
    st.set((ulonglong)cast_length, &my_charset_bin);
    str->append(st);
    str->append(')');
  }
  if (cast_cs)
  {
    str->append(" charset ", 9);
    str->append(cast_cs->name);
  }
  str->append(')');
}

String *Item_char_typecast::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  uint32 length;

  if (!charset_conversion)
  {
    if (!(res= args[0]->val_str(str)))
    {
      null_value= 1;
      return 0;
    }
  }
  else
  {
    // Convert character set if differ
    uint dummy_errors;
    if (!(res= args[0]->val_str(&tmp_value)) ||
        str->copy(res->ptr(), res->length(), from_cs,
        cast_cs, &dummy_errors))
    {
      null_value= 1;
      return 0;
    }
    res= str;
  }

  res->set_charset(cast_cs);

  /*
     Cut the tail if cast with length
     and the result is longer than cast length, e.g.
     CAST('string' AS CHAR(1))
  */
  if (cast_length >= 0 &&
      (res->length() > (length= (uint32) res->charpos(cast_length))))
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      res= &str_value;
    }
    res->length((uint) length);
  }
  null_value= 0;
  return res;
}

void Item_char_typecast::fix_length_and_dec()
{
  uint32 char_length;
  /* 
     We always force character set conversion if cast_cs
     is a multi-byte character set. It garantees that the
     result of CAST is a well-formed string.
     For single-byte character sets we allow just to copy
     from the argument. A single-byte character sets string
     is always well-formed. 
     
     There is a special trick to convert form a number to ucs2.
     As numbers have my_charset_bin as their character set,
     it wouldn't do conversion to ucs2 without an additional action.
     To force conversion, we should pretend to be non-binary.
     Let's choose from_cs this way:
     - If the argument in a number and cast_cs is ucs2 (i.e. mbminlen > 1),
       then from_cs is set to latin1, to perform latin1 -> ucs2 conversion.
     - If the argument is a number and cast_cs is ASCII-compatible
       (i.e. mbminlen == 1), then from_cs is set to cast_cs,
       which allows just to take over the args[0]->val_str() result
       and thus avoid unnecessary character set conversion.
     - If the argument is not a number, then from_cs is set to
       the argument's charset.
  */
  from_cs= (args[0]->result_type() == INT_RESULT || 
            args[0]->result_type() == REAL_RESULT) ?
           (cast_cs->mbminlen == 1 ? cast_cs : &my_charset_latin1) :
           args[0]->collation.collation;
  charset_conversion= (cast_cs->mbmaxlen > 1) ||
                      !my_charset_same(from_cs, cast_cs) &&
                      from_cs != &my_charset_bin &&
                      cast_cs != &my_charset_bin;
  collation.set(cast_cs, DERIVATION_IMPLICIT);
  char_length= (cast_length >= 0) ? cast_length : 
	       args[0]->max_length/from_cs->mbmaxlen;
  max_length= char_length * cast_cs->mbmaxlen;
}


String *Item_datetime_typecast::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (!get_arg0_date(&ltime,1) &&
      !make_datetime(ltime.second_part ? DATE_TIME_MICROSECOND : DATE_TIME, 
		     &ltime, str))
    return str;

  null_value=1;
  return 0;
}


longlong Item_datetime_typecast::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (get_arg0_date(&ltime,1))
  {
    null_value= 1;
    return 0;
  }

  return TIME_to_ulonglong_datetime(&ltime);
}


bool Item_time_typecast::get_time(TIME *ltime)
{
  bool res= get_arg0_time(ltime);
  /*
    For MYSQL_TIMESTAMP_TIME value we can have non-zero day part,
    which we should not lose.
  */
  if (ltime->time_type == MYSQL_TIMESTAMP_DATETIME)
    ltime->year= ltime->month= ltime->day= 0;
  ltime->time_type= MYSQL_TIMESTAMP_TIME;
  return res;
}


longlong Item_time_typecast::val_int()
{
  TIME ltime;
  if (get_time(&ltime))
  {
    null_value= 1;
    return 0;
  }
  return ltime.hour * 10000L + ltime.minute * 100 + ltime.second;
}

String *Item_time_typecast::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;

  if (!get_arg0_time(&ltime) &&
      !make_datetime(ltime.second_part ? TIME_MICROSECOND : TIME_ONLY,
		     &ltime, str))
    return str;

  null_value=1;
  return 0;
}


bool Item_date_typecast::get_date(TIME *ltime, uint fuzzy_date)
{
  bool res= get_arg0_date(ltime,1);
  ltime->hour= ltime->minute= ltime->second= ltime->second_part= 0;
  ltime->time_type= MYSQL_TIMESTAMP_DATE;
  return res;
}


String *Item_date_typecast::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;

  if (!get_arg0_date(&ltime,1) && !str->alloc(11))
  {
    make_date((DATE_TIME_FORMAT *) 0, &ltime, str);
    return str;
  }

  null_value=1;
  return 0;
}

longlong Item_date_typecast::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (args[0]->get_date(&ltime, TIME_FUZZY_DATE))
    return 0;
  return (longlong) (ltime.year * 10000L + ltime.month * 100 + ltime.day);
}

/*
  MAKEDATE(a,b) is a date function that creates a date value 
  from a year and day value.
*/

String *Item_func_makedate::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME l_time;
  long daynr=  (long) args[1]->val_int();
  long yearnr= (long) args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      yearnr < 0 || daynr <= 0)
    goto err;

  days= calc_daynr(yearnr,1,1) + daynr - 1;
  /* Day number from year 0 to 9999-12-31 */
  if (days >= 0 && days <= MAX_DAY_NUMBER)
  {
    null_value=0;
    get_date_from_daynr(days,&l_time.year,&l_time.month,&l_time.day);
    if (str->alloc(11))
      goto err;
    make_date((DATE_TIME_FORMAT *) 0, &l_time, str);
    return str;
  }

err:
  null_value=1;
  return 0;
}


longlong Item_func_makedate::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME l_time;
  long daynr=  (long) args[1]->val_int();
  long yearnr= (long) args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      yearnr < 0 || daynr <= 0)
    goto err;

  days= calc_daynr(yearnr,1,1) + daynr - 1;
  /* Day number from year 0 to 9999-12-31 */
  if (days >= 0 && days < MAX_DAY_NUMBER)
  {
    null_value=0;
    get_date_from_daynr(days,&l_time.year,&l_time.month,&l_time.day);
    return (longlong) (l_time.year * 10000L + l_time.month * 100 + l_time.day);
  }

err:
  null_value= 1;
  return 0;
}


void Item_func_add_time::fix_length_and_dec()
{
  enum_field_types arg0_field_type;
  decimals=0;
  max_length=MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;

  /*
    The field type for the result of an Item_func_add_time function is defined
    as follows:

    - If first arg is a MYSQL_TYPE_DATETIME or MYSQL_TYPE_TIMESTAMP 
      result is MYSQL_TYPE_DATETIME
    - If first arg is a MYSQL_TYPE_TIME result is MYSQL_TYPE_TIME
    - Otherwise the result is MYSQL_TYPE_STRING
  */

  cached_field_type= MYSQL_TYPE_STRING;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == MYSQL_TYPE_DATE ||
      arg0_field_type == MYSQL_TYPE_DATETIME ||
      arg0_field_type == MYSQL_TYPE_TIMESTAMP)
    cached_field_type= MYSQL_TYPE_DATETIME;
  else if (arg0_field_type == MYSQL_TYPE_TIME)
    cached_field_type= MYSQL_TYPE_TIME;
}

/*
  ADDTIME(t,a) and SUBTIME(t,a) are time functions that calculate a
  time/datetime value 

  t: time_or_datetime_expression
  a: time_expression
  
  Result: Time value or datetime value
*/

String *Item_func_add_time::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME l_time1, l_time2, l_time3;
  bool is_time= 0;
  long days, microseconds;
  longlong seconds;
  int l_sign= sign;

  null_value=0;
  if (is_date)                        // TIMESTAMP function
  {
    if (get_arg0_date(&l_time1,1) || 
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

  l_time3.neg= calc_time_diff(&l_time1, &l_time2, -l_sign,
			      &seconds, &microseconds);

  /*
    If first argument was negative and diff between arguments
    is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    l_time3.neg= 1-l_time3.neg;         // Swap sign of result

  if (!is_time && l_time3.neg)
    goto null_date;

  days= (long)(seconds/86400L);

  calc_time_from_sec(&l_time3, (long)(seconds%86400L), microseconds);
  if (!is_time)
  {
    get_date_from_daynr(days,&l_time3.year,&l_time3.month,&l_time3.day);
    if (l_time3.day &&
	!make_datetime(l_time1.second_part || l_time2.second_part ?
		       DATE_TIME_MICROSECOND : DATE_TIME,
		       &l_time3, str))
      return str;
    goto null_date;
  }
  
  l_time3.hour+= days*24;
  if (!make_datetime(l_time1.second_part || l_time2.second_part ?
		     TIME_MICROSECOND : TIME_ONLY,
		     &l_time3, str))
    return str;

null_date:
  null_value=1;
  return 0;
}


void Item_func_add_time::print(String *str)
{
  if (is_date)
  {
    DBUG_ASSERT(sign > 0);
    str->append("timestamp(", 10);
  }
  else
  {
    if (sign > 0)
      str->append("addtime(", 8);
    else
      str->append("subtime(", 8);
  }
  args[0]->print(str);
  str->append(',');
  args[0]->print(str);
  str->append(')');
}


/*
  TIMEDIFF(t,s) is a time function that calculates the 
  time value between a start and end time.

  t and s: time_or_datetime_expression
  Result: Time value
*/

String *Item_func_timediff::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong seconds;
  long microseconds;
  int l_sign= 1;
  TIME l_time1 ,l_time2, l_time3;

  null_value= 0;  
  if (args[0]->get_time(&l_time1) ||
      args[1]->get_time(&l_time2) ||
      l_time1.time_type != l_time2.time_type)
    goto null_date;

  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  l_time3.neg= calc_time_diff(&l_time1, &l_time2, l_sign,
			      &seconds, &microseconds);

  /*
    For MYSQL_TIMESTAMP_TIME only:
      If first argument was negative and diff between arguments
      is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    l_time3.neg= 1-l_time3.neg;         // Swap sign of result

  calc_time_from_sec(&l_time3, (long) seconds, microseconds);

  if (!make_datetime(l_time1.second_part || l_time2.second_part ?
		     TIME_MICROSECOND : TIME_ONLY,
		     &l_time3, str))
    return str;

null_date:
  null_value=1;
  return 0;
}

/*
  MAKETIME(h,m,s) is a time function that calculates a time value 
  from the total number of hours, minutes, and seconds.
  Result: Time value
*/

String *Item_func_maketime::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;

  long hour=   (long) args[0]->val_int();
  long minute= (long) args[1]->val_int();
  long second= (long) args[2]->val_int();

  if ((null_value=(args[0]->null_value || 
		   args[1]->null_value ||
		   args[2]->null_value || 
		   minute > 59 || minute < 0 || 
		   second > 59 || second < 0 ||
		   str->alloc(19))))
    return 0;

  ltime.neg= 0;
  if (hour < 0)
  {
    ltime.neg= 1;
    hour= -hour;
  }
  ltime.hour=   (ulong) hour;
  ltime.minute= (ulong) minute;
  ltime.second= (ulong) second;
  make_time((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}


/*
  MICROSECOND(a) is a function ( extraction) that extracts the microseconds
  from a.

  a: Datetime or time value
  Result: int value
*/

longlong Item_func_microsecond::val_int()
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;
  if (!get_arg0_time(&ltime))
    return ltime.second_part;
  return 0;
}


String *Item_func_get_format::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  const char *format_name;
  KNOWN_DATE_TIME_FORMAT *format;
  String *val= args[0]->val_str(str);
  ulong val_len;

  if ((null_value= args[0]->null_value))
    return 0;    

  val_len= val->length();
  for (format= &known_date_time_formats[0];
       (format_name= format->format_name);
       format++)
  {
    uint format_name_len;
    format_name_len= strlen(format_name);
    if (val_len == format_name_len &&
	!my_strnncoll(&my_charset_latin1, 
		      (const uchar *) val->ptr(), val_len, 
		      (const uchar *) format_name, val_len))
    {
      const char *format_str= get_date_time_format_str(format, type);
      str->set(format_str, strlen(format_str), &my_charset_bin);
      return str;
    }
  }

  null_value= 1;
  return 0;
}


void Item_func_get_format::print(String *str)
{
  str->append(func_name());
  str->append('(');

  switch (type) {
  case MYSQL_TIMESTAMP_DATE:
    str->append("DATE, ");
    break;
  case MYSQL_TIMESTAMP_DATETIME:
    str->append("DATETIME, ");
    break;
  case MYSQL_TIMESTAMP_TIME:
    str->append("TIME, ");
    break;
  default:
    DBUG_ASSERT(0);
  }
  args[0]->print(str);
  str->append(')');
}


/*
  Get type of datetime value (DATE/TIME/...) which will be produced
  according to format string.

  SYNOPSIS
    get_date_time_result_type()
      format - format string
      length - length of format string

  NOTE
    We don't process day format's characters('D', 'd', 'e') because day
    may be a member of all date/time types.

    Format specifiers supported by this function should be in sync with
    specifiers supported by extract_date_time() function.

  RETURN VALUE
    One of date_time_format_types values:
    DATE_TIME_MICROSECOND, DATE_TIME, DATE_ONLY, TIME_MICROSECOND, TIME_ONLY
*/

static date_time_format_types
get_date_time_result_type(const char *format, uint length)
{
  const char *time_part_frms= "HISThiklrs";
  const char *date_part_frms= "MVUXYWabcjmvuxyw";
  bool date_part_used= 0, time_part_used= 0, frac_second_used= 0;
  
  const char *val= format;
  const char *end= format + length;

  for (; val != end && val != end; val++)
  {
    if (*val == '%' && val+1 != end)
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
	return DATE_TIME_MICROSECOND;
      }
    }
  }

  /* We don't have all three types of date-time components */
  if (frac_second_used)
    return TIME_MICROSECOND;
  if (time_part_used)
  {
    if (date_part_used)
      return DATE_TIME;
    return TIME_ONLY;
  }
  return DATE_ONLY;
}


Field *Item_func_str_to_date::tmp_table_field(TABLE *t_arg)
{
  if (cached_field_type == MYSQL_TYPE_TIME)
    return (new Field_time(maybe_null, name, t_arg, &my_charset_bin));
  if (cached_field_type == MYSQL_TYPE_DATE)
    return (new Field_date(maybe_null, name, t_arg, &my_charset_bin));
  if (cached_field_type == MYSQL_TYPE_DATETIME)
    return (new Field_datetime(maybe_null, name, t_arg, &my_charset_bin));
  return (new Field_string(max_length, maybe_null, name, t_arg, &my_charset_bin));
}


void Item_func_str_to_date::fix_length_and_dec()
{
  char format_buff[64];
  String format_str(format_buff, sizeof(format_buff), &my_charset_bin);
  String *format;
  maybe_null= 1;
  decimals=0;
  cached_field_type= MYSQL_TYPE_STRING;
  max_length= MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  cached_timestamp_type= MYSQL_TIMESTAMP_NONE;
  format= args[1]->val_str(&format_str);
  if (!args[1]->null_value && (const_item= args[1]->const_item()))
  {
    cached_format_type= get_date_time_result_type(format->ptr(),
                                                  format->length());
    switch (cached_format_type) {
    case DATE_ONLY:
      cached_timestamp_type= MYSQL_TIMESTAMP_DATE;
      cached_field_type= MYSQL_TYPE_DATE; 
      max_length= MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
      break;
    case TIME_ONLY:
    case TIME_MICROSECOND:
      cached_timestamp_type= MYSQL_TIMESTAMP_TIME;
      cached_field_type= MYSQL_TYPE_TIME; 
      max_length= MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
      break;
    default:
      cached_timestamp_type= MYSQL_TIMESTAMP_DATETIME;
      cached_field_type= MYSQL_TYPE_DATETIME; 
      break;
    }
  }
}

bool Item_func_str_to_date::get_date(TIME *ltime, uint fuzzy_date)
{
  DATE_TIME_FORMAT date_time_format;
  char val_buff[64], format_buff[64];
  String val_str(val_buff, sizeof(val_buff), &my_charset_bin), *val;
  String format_str(format_buff, sizeof(format_buff), &my_charset_bin), *format;

  val=    args[0]->val_str(&val_str);
  format= args[1]->val_str(&format_str);
  if (args[0]->null_value || args[1]->null_value)
    goto null_date;

  null_value= 0;
  bzero((char*) ltime, sizeof(ltime));
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length();
  if (extract_date_time(&date_time_format, val->ptr(), val->length(),
			ltime, cached_timestamp_type, 0))
    goto null_date;
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
  return (null_value=1);
}


String *Item_func_str_to_date::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  TIME ltime;

  if (Item_func_str_to_date::get_date(&ltime, TIME_FUZZY_DATE))
    return 0;

  if (!make_datetime((const_item ? cached_format_type :
		     (ltime.second_part ? DATE_TIME_MICROSECOND : DATE_TIME)),
		     &ltime, str))
    return str;
  return 0;
}


bool Item_func_last_day::get_date(TIME *ltime, uint fuzzy_date)
{
  if (get_arg0_date(ltime, fuzzy_date & ~TIME_FUZZY_DATE))
    return 1;
  uint month_idx= ltime->month-1;
  ltime->day= days_in_month[month_idx];
  if ( month_idx == 1 && calc_days_in_year(ltime->year) == 366)
    ltime->day= 29;
  ltime->time_type= MYSQL_TIMESTAMP_DATE;
  return 0;
}

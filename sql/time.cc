/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/* Functions to handle date and time */

#include "mysql_priv.h"
#include <m_ctype.h>

static ulong const days_at_timestart=719528;	/* daynr at 1970.01.01 */
uchar *days_in_month= (uchar*) "\037\034\037\036\037\036\037\037\036\037\036\037";

	/* Init some variabels needed when using my_local_time */
	/* Currently only my_time_zone is inited */

static long my_time_zone=0;

void init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;;
  TIME my_time;

  seconds= (time_t) time((time_t*) 0);
  localtime_r(&seconds,&tm_tmp);
  l_time= &tm_tmp;
  my_time_zone=		3600;		/* Comp. for -3600 in my_gmt_sec */
  my_time.year=		(uint) l_time->tm_year+1900;
  my_time.month=	(uint) l_time->tm_mon+1;
  my_time.day=		(uint) l_time->tm_mday;
  my_time.hour=		(uint) l_time->tm_hour;
  my_time.minute=	(uint) l_time->tm_min;
  my_time.second=	(uint) l_time->tm_sec;
  my_gmt_sec(&my_time, &my_time_zone);	/* Init my_time_zone */
}

/*
  Convert current time to sec. since 1970.01.01 
  This code handles also day light saving time.
  The idea is to cache the time zone (including daylight saving time)
  for the next call to make things faster.

*/

long my_gmt_sec(TIME *t, long *my_timezone)
{
  uint loop;
  time_t tmp;
  struct tm *l_time,tm_tmp;
  long diff, current_timezone;

  if (t->hour >= 24)
  {					/* Fix for time-loop */
    t->day+=t->hour/24;
    t->hour%=24;
  }

  /*
    Calculate the gmt time based on current time and timezone
    The -1 on the end is to ensure that if have a date that exists twice
    (like 2002-10-27 02:00:0 MET), we will find the initial date.

    By doing -3600 we will have to call localtime_r() several times, but
    I couldn't come up with a better way to get a repeatable result :(

    We can't use mktime() as it's buggy on many platforms and not thread safe.
  */
  tmp=(time_t) (((calc_daynr((uint) t->year,(uint) t->month,(uint) t->day) -
		  (long) days_at_timestart)*86400L + (long) t->hour*3600L +
		 (long) (t->minute*60 + t->second)) + (time_t) my_time_zone -
		3600);
  current_timezone= my_time_zone;

  localtime_r(&tmp,&tm_tmp);
  l_time=&tm_tmp;
  for (loop=0;
       loop < 2 &&
	 (t->hour != (uint) l_time->tm_hour ||
	  t->minute != (uint) l_time->tm_min);
       loop++)
  {					/* One check should be enough ? */
    /* Get difference in days */
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days= 1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour)) +
	  (long) (60*((int) t->minute - (int) l_time->tm_min)));
    current_timezone+= diff+3600;		// Compensate for -3600 above
    tmp+= (time_t) diff;
    localtime_r(&tmp,&tm_tmp);
    l_time=&tm_tmp;
  }
  /*
    Fix that if we are in the not existing daylight saving time hour
    we move the start of the next real hour
  */
  if (loop == 2 && t->hour != (uint) l_time->tm_hour)
  {
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days=1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour))+
	  (long) (60*((int) t->minute - (int) l_time->tm_min)));
    if (diff == 3600)
      tmp+=3600 - t->minute*60 - t->second;	// Move to next hour
    else if (diff == -3600)
      tmp-=t->minute*60 + t->second;		// Move to previous hour
  }
  *my_timezone= current_timezone;
  return (long) tmp;
} /* my_gmt_sec */


	/* Some functions to calculate dates */

	/* Calculate nr of day since year 0 in new date-system (from 1615) */

long calc_daynr(uint year,uint month,uint day)
{
  long delsum;
  int temp;
  DBUG_ENTER("calc_daynr");

  if (year == 0 && month == 0 && day == 0)
    DBUG_RETURN(0);				/* Skip errors */
  if (year < 200)
  {
    if ((year=year+1900) < 1900+YY_PART_YEAR)
      year+=100;
  }
  delsum= (long) (365L * year+ 31*(month-1) +day);
  if (month <= 2)
      year--;
  else
    delsum-= (long) (month*4+23)/10;
  temp=(int) ((year/100+1)*3)/4;
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d -> daynr: %ld",
		     year+(month <= 2),month,day,delsum+year/4-temp));
  DBUG_RETURN(delsum+(int) year/4-temp);
} /* calc_daynr */


	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  DBUG_ENTER("calc_weekday");
  DBUG_RETURN ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}

	/* Calc days in one year. works with 0 <= year <= 99 */

uint calc_days_in_year(uint year)
{
  return (year & 3) == 0 && (year%100 || (year%400 == 0 && year)) ?
    366 : 365;
}

/* Calculate week.  If 'with_year' is not set, then return a week 0-53, where
   0 means that it's the last week of the previous year.
   If 'with_year' is set then the week will always be in the range 1-53 and
   the year out parameter will contain the year for the week */

uint calc_week(TIME *l_time, bool with_year, bool sunday_first_day_of_week,
	       uint *year)
{
  uint days;
  ulong daynr=calc_daynr(l_time->year,l_time->month,l_time->day);
  ulong first_daynr=calc_daynr(l_time->year,1,1);
  uint weekday=calc_weekday(first_daynr,sunday_first_day_of_week);
  *year=l_time->year;
  if (l_time->month == 1 && l_time->day <= 7-weekday &&
      ((!sunday_first_day_of_week && weekday >= 4) ||
       (sunday_first_day_of_week && weekday != 0)))
  {
    /* Last week of the previous year */
    if (!with_year)
      return 0;
    with_year=0;				// Don't check the week again
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }
  if ((sunday_first_day_of_week && weekday != 0) ||
      (!sunday_first_day_of_week && weekday >= 4))
    days= daynr - (first_daynr+ (7-weekday));
  else
    days= daynr - (first_daynr - weekday);
  if (with_year && days >= 52*7)
  {
    /* Check if we are on the first week of the next year (or week 53) */
    weekday= (weekday + calc_days_in_year(*year)) % 7;
    if (weekday < 4)
    {					// We are at first week on next year
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


/* Position for YYYY-DD-MM HH-MM-DD.FFFFFF AM in default format */

static uchar internal_format_positions[]=
{0, 1, 2, 3, 4, 5, 6, (uchar) 255};

static char time_separator=':';

/*
  Convert a timestamp string to a TIME value.

  SYNOPSIS
    str_to_TIME()
    str			String to parse
    length		Length of string
    l_time		Date is stored here
    flags		Bitmap of following items
			TIME_FUZZY_DATE    Set if we should allow partial dates
			TIME_DATETIME_ONLY Set if we only allow full datetimes.

  DESCRIPTION
    At least the following formats are recogniced (based on number of digits)
    YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
    YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
    YYYYMMDDTHHMMSS  where T is a the character T (ISO8601)
    Also dates where all parts are zero are allowed

    The second part may have an optional .###### fraction part.

  NOTES
   This function should work with a format position vector as long as the
   following things holds:
   - All date are kept together and all time parts are kept together
   - Date and time parts must be separated by blank
   - Second fractions must come after second part and be separated
     by a '.'.  (The second fractions are optional)
   - AM/PM must come after second fractions (or after seconds if no fractions)
   - Year must always been specified.
   - If time is before date, then we will use datetime format only if
     the argument consist of two parts, separated by space.
     Otherwise we will assume the argument is a date.
   - The hour part must be specified in hour-minute-second order.

  RETURN VALUES
    TIMESTAMP_NONE	String wasn't a timestamp, like
			[DD [HH:[MM:[SS]]]].fraction.
			l_time is not changed.
    TIMESTAMP_DATE	DATE string (YY MM and DD parts ok)
    TIMESTAMP_DATETIME	Full timestamp
    TIMESTAMP_DATETIME_ERROR Timestamp with wrong values.
			All elements in l_time is set to 0
*/

#define MAX_DATE_PARTS 8

timestamp_type
str_to_TIME(const char *str, uint length, TIME *l_time, uint flags)
{
  uint field_length, year_length, digits, i, number_of_fields;
  uint date[MAX_DATE_PARTS], date_len[MAX_DATE_PARTS];
  uint add_hours= 0, start_loop;
  ulong not_zero_date, allow_space;
  bool is_internal_format;
  const char *pos, *last_field_pos;
  const char *end=str+length;
  const uchar *format_position;
  bool found_delimitier= 0, found_space= 0;
  DBUG_ENTER("str_to_TIME");
  DBUG_PRINT("ENTER",("str: %.*s",length,str));

  LINT_INIT(field_length);
  LINT_INIT(year_length);
  LINT_INIT(last_field_pos);

  // Skip space at start
  for (; str != end && my_isspace(&my_charset_latin1, *str) ; str++)
    ;
  if (str == end || ! my_isdigit(&my_charset_latin1, *str))
    DBUG_RETURN(TIMESTAMP_NONE);

  is_internal_format= 0;
  /* This has to be changed if want to activate different timestamp formats */
  format_position= internal_format_positions;

  /*
    Calculate number of digits in first part.
    If length= 8 or >= 14 then year is of format YYYY.
    (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str; pos != end && my_isdigit(&my_charset_latin1,*pos) ; pos++)
    ;

  digits= (uint) (pos-str);
  start_loop= 0;				// Start of scan loop
  date_len[format_position[0]]= 0;		// Length of year field
  if (pos == end)
  {
    /* Found date in internal format (only numbers like YYYYMMDD) */
    year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
    field_length=year_length-1;
    is_internal_format= 1;
    format_position= internal_format_positions;
  }
  else
  {
    if (format_position[0] >= 3)		// If year is after HHMMDD
    {
      /*
	If year is not in first part then we have to determinate if we got
	a date field or a datetime field.
	We do this by checking if there is two numbers separated by
	space in the input.
      */
      while (pos < end && !my_isspace(&my_charset_latin1, *pos))
	pos++;
      while (pos < end && !my_isdigit(&my_charset_latin1, *pos))
	pos++;
      if (pos == end)
      {
	if (flags & TIME_DATETIME_ONLY)
	  DBUG_RETURN(TIMESTAMP_NONE);		// Can't be a full datetime
	/* Date field.  Set hour, minutes and seconds to 0 */
	date[0]= date[1]= date[2]= date[3]= date[4]= 0;
	start_loop= 5;				// Start with first date part
      }
    }
  }

  /*
    Only allow space in the first "part" of the datetime field and:
    - after days, part seconds
    - before and after AM/PM (handled by code later)

    2003-03-03 20:00:20 AM
    20:00:20.000000 AM 03-03-2000
  */
  i= max((uint) format_position[0], (uint) format_position[1]);
  set_if_bigger(i, (uint) format_position[2]);
  allow_space= ((1 << i) | (1 << format_position[6]));
  allow_space&= (1 | 2 | 4 | 8);

  not_zero_date= 0;
  for (i = start_loop;
       i < MAX_DATE_PARTS-1 && str != end &&
	 my_isdigit(&my_charset_latin1,*str);
       i++)
  {
    const char *start= str;
    ulong tmp_value= (uint) (uchar) (*str++ - '0');
    while (str != end && my_isdigit(&my_charset_latin1,str[0]) &&
	   (!is_internal_format || field_length--))
    {
      tmp_value=tmp_value*10 + (ulong) (uchar) (*str - '0');
      str++;
    }
    date_len[i]+= (uint) (str - start);
    if (tmp_value > 999999)			// Impossible date part
      DBUG_RETURN(TIMESTAMP_NONE);
    date[i]=tmp_value;
    not_zero_date|= tmp_value;

    /* Length-1 of next field */
    field_length= format_position[i+1] == 0 ? 3 : 1;

    if ((last_field_pos= str) == end)
    {
      i++;					// Register last found part
      break;
    }
    /* Allow a 'T' after day to allow CCYYMMDDT type of fields */
    if (i == format_position[2] && *str == 'T')
    {
      str++;					// ISO8601:  CCYYMMDDThhmmss
      continue;
    }
    if (i == format_position[5])		// Seconds
    {
      if (*str == '.')				// Followed by part seconds
      {
	str++;
	field_length= 5;			// 5 digits after first (=6)
      }
      continue;

      /* No part seconds */
      date[++i]= 0;
    }
    while (str != end &&
	   (my_ispunct(&my_charset_latin1,*str) ||
	    my_isspace(&my_charset_latin1,*str)))
    {
      if (my_isspace(&my_charset_latin1,*str))
      {
	if (!(allow_space & (1 << i)))
	  DBUG_RETURN(TIMESTAMP_NONE);
	found_space= 1;
      }
      str++;
      found_delimitier= 1;			// Should be a 'normal' date
    }
    /* Check if next position is AM/PM */
    if (i == format_position[6])		// Seconds, time for AM/PM
    {
      i++;					// Skip AM/PM part
      if (format_position[7] != 255)		// If using AM/PM
      {
	if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
	{
	  if (str[1] == 'p' || str[1] == 'P')
	    add_hours= 12;
	  else if (str[1] != 'a' || str[1] != 'A')
	    continue;				// Not AM/PM
	  str+= 2;				// Skip AM/PM
	  /* Skip space after AM/PM */
	  while (str != end && my_isspace(&my_charset_latin1,*str))
	    str++;
	}
      }
    }
    last_field_pos= str;
  }
  if (found_delimitier && !found_space && (flags & TIME_DATETIME_ONLY))
    DBUG_RETURN(TIMESTAMP_NONE);		// Can't be a datetime

  str= last_field_pos;

  number_of_fields= i - start_loop;
  while (i < MAX_DATE_PARTS)
    date[i++]=0;

  if (!is_internal_format)
  {
    year_length= date_len[(uint) format_position[0]];
    if (!year_length)				// Year must be specified
      DBUG_RETURN(TIMESTAMP_NONE);

    l_time->year=		date[(uint) format_position[0]];
    l_time->month=		date[(uint) format_position[1]];
    l_time->day=		date[(uint) format_position[2]];
    l_time->hour=		date[(uint) format_position[3]];
    l_time->minute=		date[(uint) format_position[4]];
    l_time->second=		date[(uint) format_position[5]];
    l_time->second_part=	date[(uint) format_position[6]];
    if (format_position[7] != (uchar) 255)
    {
      if (l_time->hour > 12)
	goto err;
      l_time->hour= l_time->hour%12 + add_hours;
    }
  }
  else
  {
    l_time->year=	date[0];
    l_time->month=	date[1];
    l_time->day=	date[2];
    l_time->hour=	date[3];
    l_time->minute=	date[4];
    l_time->second=	date[5];
    l_time->second_part=date[6];
  }
  l_time->neg= 0;

  if (year_length == 2 && i >= format_position[1] && i >=format_position[2] &&
      (l_time->month || l_time->day))
    l_time->year+= (l_time->year < YY_PART_YEAR ? 2000 : 1900);

  if (number_of_fields < 3 || l_time->month > 12 ||
      l_time->day > 31 || l_time->hour > 23 ||
      l_time->minute > 59 || l_time->second > 59 ||
      (!(flags & TIME_FUZZY_DATE) && (l_time->month == 0 || l_time->day == 0)))
  {
    /* Only give warning for a zero date if there is some garbage after */
    if (!not_zero_date)				// If zero date
    {
      for (; str != end ; str++)
      {
	if (!my_isspace(&my_charset_latin1, *str))
	{
	  not_zero_date= 1;			// Give warning
	  break;
	}
      }
    }
    if (not_zero_date)
      current_thd->cuted_fields++;
    goto err;
  }
  if (str != end && current_thd->count_cuted_fields)
  {
    for (; str != end ; str++)
    {
      if (!my_isspace(&my_charset_latin1,*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    }
  }

  DBUG_RETURN(l_time->time_type=
	      (number_of_fields <= 3 ? TIMESTAMP_DATE : TIMESTAMP_DATETIME));

err:
  bzero((char*) l_time, sizeof(*l_time));
  DBUG_RETURN(TIMESTAMP_DATETIME_ERROR);
}


time_t str_to_timestamp(const char *str,uint length)
{
  TIME l_time;
  long not_used;

  if (str_to_TIME(str,length,&l_time,0) <= TIMESTAMP_DATETIME_ERROR)
    return(0);
  if (l_time.year >= TIMESTAMP_MAX_YEAR || l_time.year < 1900+YY_PART_YEAR)
  {
    current_thd->cuted_fields++;
    return(0);
  }
  return(my_gmt_sec(&l_time, &not_used));
}


longlong str_to_datetime(const char *str,uint length, uint fuzzy_date)
{
  TIME l_time;
  if (str_to_TIME(str,length,&l_time,fuzzy_date) <= TIMESTAMP_DATETIME_ERROR)
    return(0);
  return (longlong) (l_time.year*LL(10000000000) +
		     l_time.month*LL(100000000)+
		     l_time.day*LL(1000000)+
		     l_time.hour*LL(10000)+
		     (longlong) (l_time.minute*100+l_time.second));
}


/*
 Convert a time string to a TIME struct.

  SYNOPSIS
   str_to_time()
   str			A string in full TIMESTAMP format or
			[-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS,
			[M]MSS or [S]S
			There may be an optional [.second_part] after seconds
   length		Length of str
   l_time		Store result here

   NOTES
     Because of the extra days argument, this function can only
     work with times where the time arguments are in the above order.

   RETURN
     0	ok
     1  error
*/

bool str_to_time(const char *str,uint length,TIME *l_time)
{
  long date[5],value;
  const char *end=str+length, *end_of_days;
  bool found_days,found_hours;
  uint state;

  l_time->neg=0;
  for (; str != end && my_isspace(&my_charset_latin1,*str) ; str++)
    length--;
  if (str != end && *str == '-')
  {
    l_time->neg=1;
    str++;
    length--;
  }
  if (str == end)
    return 1;

  /* Check first if this is a full TIMESTAMP */
  if (length >= 12)
  {						// Probably full timestamp
    enum timestamp_type res= str_to_TIME(str,length,l_time,
					 (TIME_FUZZY_DATE |
					 TIME_DATETIME_ONLY));
    if ((int) res >= (int) TIMESTAMP_DATETIME_ERROR)
      return res == TIMESTAMP_DATETIME_ERROR;
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
    value=value*10L + (long) (*str - '0');

  /* Skipp all space after 'days' */
  end_of_days= str;
  for (; str != end && my_isspace(&my_charset_latin1, str[0]) ; str++)
    ;

  LINT_INIT(state);
  found_days=found_hours=0;
  if ((uint) (end-str) > 1 && str != end_of_days &&
      my_isdigit(&my_charset_latin1, *str))
  {						// Found days part
    date[0]= value;
    state= 1;					// Assume next is hours
    found_days= 1;
  }
  else if ((end-str) > 1 &&  *str == time_separator &&
           my_isdigit(&my_charset_latin1, str[1]))
  {
    date[0]=0;					// Assume we found hours
    date[1]=value;
    state=2;
    found_hours=1;
    str++;					// skip ':'
  }
  else
  {
    /* String given as one number; assume HHMMSS format */
    date[0]= 0;
    date[1]= value/10000;
    date[2]= value/100 % 100;
    date[3]= value % 100;
    state=4;
    goto fractional;
  }

  /* Read hours, minutes and seconds */
  for (;;)
  {
    for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
      value=value*10L + (long) (*str - '0');
    date[state++]=value;
    if (state == 4 || (end-str) < 2 || *str != time_separator ||
	!my_isdigit(&my_charset_latin1,str[1]))
      break;
    str++;					// Skip time_separator (':')
  }

  if (state != 4)
  {						// Not HH:MM:SS
    /* Fix the date to assume that seconds was given */
    if (!found_hours && !found_days)
    {
      bmove_upp((char*) (date+4), (char*) (date+state),
		sizeof(long)*(state-1));
      bzero((char*) date, sizeof(long)*(4-state));
    }
    else
      bzero((char*) (date+state), sizeof(long)*(4-state));
  }

fractional:
  /* Get fractional second part */
  if ((end-str) >= 2 && *str == '.' && my_isdigit(&my_charset_latin1,str[1]))
  {
    uint field_length=5;
    str++; value=(uint) (uchar) (*str - '0');
    while (++str != end && 
           my_isdigit(&my_charset_latin1,str[0]) && 
           field_length--)
      value=value*10 + (uint) (uchar) (*str - '0');
    date[4]=value;
  }
  else
    date[4]=0;

  if (internal_format_positions[7] != 255)
  {
    /* Read a possible AM/PM */
    while (str != end && my_isspace(&my_charset_latin1, *str))
      str++;
    if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
    {
      if (str[1] == 'p' || str[1] == 'P')
      {
	str+= 2;
	date[1]= date[1]%12 + 12;
      }
      else if (str[1] == 'a' || str[1] == 'A')
	str+=2;
    }
  }

  /* Some simple checks */
  if (date[2] >= 60 || date[3] >= 60)
  {
    current_thd->cuted_fields++;
    return 1;
  }
  l_time->year= 	0;			// For protocol::store_time
  l_time->month=	0;
  l_time->day=		date[0];
  l_time->hour=		date[1];
  l_time->minute=	date[2];
  l_time->second=	date[3];
  l_time->second_part=	date[4];
  l_time->time_type= TIMESTAMP_TIME;

  /* Check if there is garbage at end of the TIME specification */
  if (str != end && current_thd->count_cuted_fields)
  {
    do
    {
      if (!my_isspace(&my_charset_latin1,*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    } while (++str != end);
  }
  return 0;
}


/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(TIME *to, struct tm *from)
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

void calc_time_from_sec(TIME *to, long seconds, long microseconds)
{
  long t_seconds;
  to->hour= seconds/3600L;
  t_seconds= seconds%3600L;
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
  if ((format_type == TIMESTAMP_DATETIME &&
       !test_all_bits(part_map, (1 | 2 | 4 | 8 | 16 | 32))) ||
      (format_type == TIMESTAMP_DATE && part_map != (1 | 2 | 4)) ||
      (format_type == TIMESTAMP_TIME &&
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
  case TIMESTAMP_DATE:
    format_str= known_date_time_formats[INTERNAL_FORMAT].date_format;
    /* fall through */
  case TIMESTAMP_TIME:
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
      if (format_type == TIMESTAMP_TIME)
      {
	if (*(format+2) != *(format+5))
	  break;				// Error
	/* Store the character used for time formats */
	date_time_format->time_separator= *(format+2);
      }
      return 0;
    }
    break;
  case TIMESTAMP_DATETIME:
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
  case TIMESTAMP_DATE:
    return format->date_format;
  case TIMESTAMP_DATETIME:
    return format->datetime_format;
  case TIMESTAMP_TIME:
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
****************************************************************************/

void make_time(DATE_TIME_FORMAT *format, TIME *l_time, String *str)
{
  long length= my_sprintf((char*) str->ptr(),
			  ((char*) str->ptr(),
			   "%s%02d:%02d:%02d",
			   (l_time->neg ? "-" : ""),
			   l_time->hour,
			   l_time->minute,
			   l_time->second));
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_date(DATE_TIME_FORMAT *format, TIME *l_time, String *str)
{
  long length= my_sprintf((char*) str->ptr(),
			  ((char*) str->ptr(),
			   "%04d-%02d-%02d",
			   l_time->year,
			   l_time->month,
			   l_time->day));
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_datetime(DATE_TIME_FORMAT *format, TIME *l_time, String *str)
{
  long length= my_sprintf((char*) str->ptr(),
			  ((char*) str->ptr(),
			   "%04d-%02d-%02d %02d:%02d:%02d",
			   l_time->year,
			   l_time->month,
			   l_time->day,
			   l_time->hour,
			   l_time->minute,
			   l_time->second));
  str->length(length);
  str->set_charset(&my_charset_bin);
}
